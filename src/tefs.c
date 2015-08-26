/******************************************************************************/
/**
@file		tefs.c
@author     Wade H. Penson
@date		June, 2015
@brief      TEFS (Tiny Embedded File System) implementation.

@copyright  Copyright 2015 Wade Penson

@license    Licensed under the Apache License, Version 2.0 (the "License");
            you may not use this file except in compliance with the License.
            You may obtain a copy of the License at

              http://www.apache.org/licenses/LICENSE-2.0

            Unless required by applicable law or agreed to in writing, software
            distributed under the License is distributed on an "AS IS" BASIS,
            WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or 
            implied. See the License for the specific language governing
            permissions and limitations under the License.
*/
/******************************************************************************/

#include "tefs.h"

/** Current bit that represents a free block in the state section. */
static uint32_t state_section_bit				= 0xFFFFFFFF;
/** The size of the state section. */
static uint32_t state_section_size				= 0;
/** The size of the directory. */
static uint16_t directory_size					= 0;
/** The number of pages that the device has. */			
static uint32_t number_of_pages					= 0;
/** The size of a page in bytes. */
static uint16_t page_size						= 0;
/** The number of addresses that can fit into a block. */
static uint32_t addresses_per_block				= 0;
/** The size of a block in pages. */		
static uint16_t block_size						= 0;
/** The size of a key in bytes. */			
static uint8_t	key_size						= 0;
/** The size of an address in bytes. */
static uint8_t	address_size					= 0;
/** Keeps track if the state section is empty or not. */
static uint8_t	is_block_pool_empty				= 0;
/** Determines if continuous writing or reading is in progress. */
static uint8_t	is_read_write_continuous		= 0;
/** The power of 2 exponent for the size of a page (used to improve
	performance). */
static uint8_t	page_size_exponent				= 0;
/** The power of 2 exponent for the number of addresses that can fit into a
	block (used to improve performance). */
static uint8_t	addresses_per_block_exponent	= 0;
/** The power of 2 exponent for the size of a block (used to improve 
	performance). */
static uint8_t	block_size_exponent				= 0;
/** The power of 2 exponent for the size of an address (used to improve
	performance). */
static uint8_t	address_size_exponent			= 0;

/**
@brief	Reserves a block and returns the address to the start of the block.

@param[out]	*block_addres	Reserved block address

@return	An error code as defined by one of the TEFS_ERR_* definitions.
*/
static int8_t
tefs_reserve_device_block(
	uint32_t	*block_address
);

/**
@brief	Releases a block.

@param	block_addres	The address of the block to release.

@return	An error code as defined by one of the TEFS_ERR_* definitions.
*/
static int8_t
tefs_release_device_block(
	uint32_t	block_address
);

/**
@brief	Erases a block and fills it with zeros.

@param	block_addres	The address of the block to erase.

@return	An error code as defined by one of the TEFS_ERR_* definitions.
*/
static int8_t
tefs_erase_block(
	uint32_t	block_address
);

/**
@brief	Finds the next empty block in the block state section.

@param	*state_section_bit	The bit that represents the next empty block.

@return	An error code as defined by one of the TEFS_ERR_* definitions.
*/
static int8_t
tefs_find_next_empty_block(
	uint32_t	*state_section_bit
);

/**
@brief	Finds the bit position from the right for a number that is of power 2.

@param	number	A number that is a power of 2.

@return	The bit position.
*/
static uint8_t
tefs_power_of_two_exponent(
	uint32_t	number
);

/**
@brief	Gets the data from the information section of the card.

@return	An error code as defined by one of the TEFS_ERR_* definitions.
*/
static int8_t
tefs_load_card_data(
	void
);

int8_t
tefs_format_device(
	uint32_t 	num_pages,
	uint16_t 	physical_page_size,
	uint16_t 	logical_block_size,
	uint8_t 	file_name_key_size,
	uint8_t		erase_before_format
)
{
	if (erase_before_format)
	{
#ifdef USE_DATAFLASH
#else /* USE_DATAFLASH */
		if (sd_raw_erase_all())
		{
			return TEFS_ERR_ERASE;
		}
#endif
	}

	if (num_pages < (1 << 16))
	{
		address_size_exponent = 1;
	}
	else
	{
		address_size_exponent = 2;
	}

	page_size_exponent = tefs_power_of_two_exponent(physical_page_size);
	block_size_exponent = tefs_power_of_two_exponent(logical_block_size);

	/* Calculate the directory size and state section size. */
	directory_size = (((1 << address_size_exponent) * (1 << (key_size << 3)) - 1)
					 / physical_page_size) + 1;
	uint32_t state_section_size_in_bytes = (num_pages - (directory_size + 1)) >>
										   (block_size_exponent + 3);
	state_section_size = ((state_section_size_in_bytes - 1) >>
						 page_size_exponent) + 1;

#ifdef USE_DATAFLASH
	/* Reserve the the pages required for the info page, directory, and state
	   section. */
	uint32_t i;
	for (i = 0; i < 1 + directory_size + state_section_size; i++)
	{
		dataflash_get_page();
	}
#endif

	uint16_t current_byte = 0;
	uint8_t data = 0;

	for (current_byte = 0; current_byte < physical_page_size; current_byte++)
	{
		if (device_write(0, &data, 1, current_byte))
		{
			return TEFS_ERR_WRITE;
		}
	}

	data = TEFS_CHECK_FLAG;

	/* Write the check flag. */
	for (current_byte = 0; current_byte < 4; current_byte++)
	{
		if (device_write(0, &data, 1, current_byte))
		{
			return TEFS_ERR_WRITE;
		}
	}

	/* Write the number of pages that the device has. */
	if (device_write(0, &num_pages, 4, current_byte))
	{
		return TEFS_ERR_WRITE;
	}
	current_byte += 4;

	/* Write the physical page size. */
	if (device_write(0, &page_size_exponent, 1, current_byte))
	{
		return TEFS_ERR_WRITE;
	}
	current_byte += 1;

	/* Write the block size. */
	if (device_write(0, &block_size_exponent, 1, current_byte))
	{
		return TEFS_ERR_WRITE;
	}
	current_byte += 1;

	/* Write the key size. */
	if (device_write(0, &file_name_key_size, 1, current_byte))
	{
		return TEFS_ERR_WRITE;
	}
	current_byte += 1;

	/* Write the address size. */
	if (device_write(0, &address_size_exponent, 1, current_byte))
	{
		return TEFS_ERR_WRITE;
	}
	current_byte += 1;

	/* Write the state section size. */
	if (device_write(0, &state_section_size, 4, current_byte))
	{
		return TEFS_ERR_WRITE;
	}
	current_byte += 4;

	/* Write the directory size. */
	if (device_write(0, &directory_size, 2, current_byte))
	{
		return TEFS_ERR_WRITE;
	}

#ifndef USE_DATAFLASH
	/* Write out ones to the state section. */
	if (sd_raw_write_continuous_start(1, 0))
	{
		return TEFS_ERR_WRITE;
	}
#endif /* USE_DATAFLASH */

	data = 0xFF;
	uint32_t current_page;

	for (current_page = 1; current_page < state_section_size + 1; current_page++)
	{
		for (current_byte = 0; current_byte < physical_page_size; current_byte++)
		{
			if (current_page == state_section_size && current_byte ==
				(state_section_size_in_bytes & ((1 << page_size_exponent) - 1)))
			{
				data = 0;
			}

#ifdef USE_DATAFLASH
			if (write_bytes(current_page, &data, 1, current_byte))
			{
				return TEFS_ERR_WRITE;			
			}
#else /* USE_DATAFLASH */
			if (sd_raw_write_continuous(&data, 1, current_byte))
			{
				return TEFS_ERR_WRITE;
			}
#endif
		}

#ifndef USE_DATAFLASH
		if (sd_raw_write_continuous_next())
		{
			return TEFS_ERR_WRITE;
		}
#endif /* USE_DATAFLASH */
	}

	/* Write out zeros to the directory. */
	data = 0;

	for (; current_page < directory_size + state_section_size + 1; current_page++)
	{
		for (current_byte = 0; current_byte < physical_page_size; current_byte++)
		{
#ifdef USE_DATAFLASH
			if (write_bytes(current_page, &data, 1, current_byte))
			{
				return TEFS_ERR_WRITE;
			}
#else /* USE_DATAFLASH */
			if (sd_raw_write_continuous(&data, 1, current_byte))
			{
				return TEFS_ERR_WRITE;
			}
#endif
		}

#ifndef USE_DATAFLASH
		if (current_page < (directory_size + state_section_size + 1) - 1)
		{
			if (sd_raw_write_continuous_next())
			{
				return TEFS_ERR_WRITE;
			}
		}
#endif /* USE_DATAFLASH */
	}

#ifndef USE_DATAFLASH
	if (sd_raw_write_continuous_stop())
	{
		return TEFS_ERR_WRITE;
	}
#endif /* USE_DATAFLASH */

	key_size = 0;

	return TEFS_ERR_OK;
}

int8_t
tefs_open(
	tefs_t 	*file,
	uint16_t 	file_id
)
{
	/* Get info from card if it has not been obtained yet. */
	if (key_size == 0)
	{
		int8_t response;
		if ((response = tefs_load_card_data()))
		{
			return response;
		}
	}

	uint32_t directory_page = (file_id >> (addresses_per_block_exponent -
							   block_size_exponent)) +
							  (1 + state_section_size);
	uint16_t directory_page_byte = address_size_exponent << (file_id &
								   ((addresses_per_block >>
									block_size_exponent) - 1));
	
	file->meta_block_address = 0;
	file->child_meta_block_address = 0;
	file->data_block_address = 0;
	
	/* Load file pointers or create file if it does not exist. */
	if (device_read(directory_page, &(file->meta_block_address), address_size,
					directory_page_byte))
	{
		return TEFS_ERR_READ;
	}

	if (file->meta_block_address == TEFS_EMPTY ||
		file->meta_block_address == TEFS_DELETED)
	{
		/* Write meta block address to directory for file ID. */
		int8_t response;
		if ((response = tefs_reserve_device_block(&(file->meta_block_address))))
		{
			return response;
		}
		
		if ((response = tefs_erase_block(file->meta_block_address)))
		{
			return response;
		}

		if (device_write(directory_page, &(file->meta_block_address),
						 address_size, directory_page_byte))
		{
			return TEFS_ERR_WRITE;
		}

		/* Write first child meta block address to meta block. */
		if ((response = tefs_reserve_device_block(&(file->child_meta_block_address))))
		{
			if (response == TEFS_ERR_DEVICE_FULL)
			{
				tefs_release_device_block(file->meta_block_address);
			}

			return response;
		}

		if ((response = tefs_erase_block(file->meta_block_address)))
		{
			return response;
		}

		if (device_write(file->meta_block_address,
						 &(file->child_meta_block_address), address_size, 0))
		{
			return TEFS_ERR_WRITE;
		}

		/* Write first data block address to first child meta block. */
		if ((response = tefs_reserve_device_block(&(file->data_block_address)))
			 != 0 && response != TEFS_ERR_DEVICE_FULL)
		{
			return response;
		}

		if (device_write(file->child_meta_block_address,
						 &(file->data_block_address), address_size, 0))
		{
			return TEFS_ERR_WRITE;
		}

		if (device_flush())
		{
			return TEFS_ERR_WRITE;
		}
	}
	else
	{
		/* Read the first child meta block address. */
		if (device_read(file->meta_block_address,
						&(file->child_meta_block_address), address_size, 0))
		{
			return TEFS_ERR_READ;
		}

		/* Read the first data block address. */
		if (device_read(file->child_meta_block_address,
						&(file->data_block_address), address_size, 0))
		{
			return TEFS_ERR_READ;
		}
	}

	file->file_id = file_id;
	file->data_block_number = 0xFFFFFFFF;
	file->current_page_number = 0xFFFFFFFF;

	return TEFS_ERR_OK;
}

int8_t
tefs_exists(
	uint16_t 	file_id
)
{
	if (key_size == 0)
	{
		int8_t response;
		if ((response = tefs_load_card_data()))
		{
			return response;
		}
	}

	/* Find file in directory. */
	uint32_t directory_page = (file_id >> (addresses_per_block_exponent -
							   block_size_exponent)) +
							  (1 + state_section_size);
	uint16_t directory_page_byte = address_size_exponent << (file_id &
								   ((addresses_per_block >>
								   	block_size_exponent) - 1));

	uint32_t meta_block_address = 0;
	
	if (device_read(directory_page, &meta_block_address, address_size,
					directory_page_byte))
	{
		return TEFS_ERR_READ;
	}

	if (meta_block_address != TEFS_EMPTY && meta_block_address != TEFS_DELETED)
	{
		return 0;
	}

	return 1;
}

int8_t
tefs_close(
	tefs_t 	*file
)
{
	if (device_flush())
	{
		return TEFS_ERR_WRITE;
	}

	return TEFS_ERR_OK;
}

int8_t
tefs_remove(
	uint16_t 	file_id
)
{
	if (key_size == 0)
	{
		int8_t response;
		if ((response = tefs_load_card_data()))
		{
			return response;
		}
	}

	/* Find file in directory. */
	uint32_t directory_page = (file_id >> (addresses_per_block_exponent -
							   block_size_exponent)) +
							  (1 + state_section_size);
	uint16_t directory_page_byte = address_size_exponent << (file_id &
								   ((addresses_per_block >>
								   	block_size_exponent) - 1));

	uint32_t meta_block_address = 0;
	
	if (device_read(directory_page, &meta_block_address, address_size,
					directory_page_byte))
	{
		return TEFS_ERR_READ;
	}

	if (meta_block_address != TEFS_EMPTY && meta_block_address != TEFS_DELETED)
	{
		/* Release data blocks that are in the file. */
		uint8_t flag = TEFS_EMPTY;
		uint32_t meta_current_byte;
		for (meta_current_byte = 0; meta_current_byte < page_size;
			 meta_current_byte += address_size)
		{
			uint32_t child_meta_block_address = 0;
			if (device_read(meta_block_address, &child_meta_block_address,
							address_size, meta_current_byte))
			{
				return TEFS_ERR_READ;
			}

			if (child_meta_block_address != TEFS_DELETED &&
				child_meta_block_address != TEFS_EMPTY)
			{
				uint32_t child_meta_current_byte;

				for (child_meta_current_byte = 0;
					 child_meta_current_byte < page_size;
					 child_meta_current_byte += address_size)
				{
					uint32_t data_block_address = 0;
					if (device_read(child_meta_block_address,
									&data_block_address, address_size,
									child_meta_current_byte))
					{
						return TEFS_ERR_READ;
					}

					if (data_block_address != TEFS_DELETED &&
						data_block_address != TEFS_EMPTY)
					{
						/* Release data block. */
						int8_t response;
						if ((response = tefs_release_device_block(data_block_address)))
						{
							return response;
						}
					}
				}

				/* Release child meta block. */
				int8_t response;
				if ((response = tefs_release_device_block(child_meta_block_address)))
				{
					return response;
				}
			}
		}

		/* Release meta block. */
		int8_t response;
		if ((response = tefs_release_device_block(meta_block_address)))
		{
			return response;
		}

		/* Delete meta block address from directory for file ID. */
		flag = TEFS_DELETED;
		uint8_t zero = 0;

		if (device_write(directory_page, &flag, 1, directory_page_byte))
		{
			return TEFS_ERR_WRITE;
		}

		directory_page_byte++;

		meta_current_byte = 1;
		while (meta_current_byte < address_size)
		{
			if (device_write(directory_page, &zero, 1, directory_page_byte))
			{
				return TEFS_ERR_WRITE;
			}

			directory_page_byte++;
			meta_current_byte++;
		}

		if (device_flush())
		{
			TEFS_ERR_WRITE;
		}
	}

	return TEFS_ERR_OK;
}

int8_t
tefs_write(
	tefs_t 	*file,
	uint32_t	file_page_address,
	void	 	*data,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
)
{
	/* Check if page address is in the same block as the current data block. 
	   If it is, write the data. Otherwise get the new data block address
	   from the meta block and then write to the page in that new data block. */
	if (file_page_address == file->current_page_number ||
		(file_page_address >> block_size_exponent) ==
		file->data_block_number)
	{
		if (device_write(file->data_block_address +
						 (file_page_address & (block_size - 1)), data,
						 number_of_bytes, byte_offset))
		{
			return TEFS_ERR_WRITE;
		}
	}
	else
	{
		/* Check if the page is in the same child meta block. If not, get the
		   address from the meta block or create a new child meta block if it
		   does not exist. */
		uint16_t child_block_number = (file_page_address >>
									   block_size_exponent >>
									   addresses_per_block_exponent);

		if ((file->data_block_number >> addresses_per_block_exponent) !=
			child_block_number)
		{
			uint16_t page_in_meta = child_block_number >>
									(page_size_exponent - address_size_exponent);
			uint16_t byte_in_meta_page = (child_block_number <<
										  address_size_exponent) &
										  (addresses_per_block - 1);

			/* Make sure that the file has not reached its max capacity. */
			if (page_in_meta >= block_size)
			{
				return TEFS_ERR_FILE_FULL;
			}

			file->child_meta_block_address = 0;

			if (device_read(file->meta_block_address + page_in_meta,
							&(file->child_meta_block_address), address_size,
							byte_in_meta_page))
			{
				return TEFS_ERR_READ;
			}

			if (file->child_meta_block_address == TEFS_EMPTY ||
				file->child_meta_block_address == TEFS_DELETED)
			{
				int8_t response;
				if ((response = tefs_reserve_device_block(&(file->child_meta_block_address))))
				{
					return response;
				}

				if ((response = tefs_erase_block(file->child_meta_block_address)))
				{
					return response;
				}

				if (device_write(file->meta_block_address + page_in_meta,
								 &(file->child_meta_block_address),
								 address_size, byte_in_meta_page))
				{
					return TEFS_ERR_WRITE;
				}
			}
		}

		/* Get the data block from the child meta block. */
		uint32_t block_in_child_meta = (file_page_address >>
										block_size_exponent) &
										(addresses_per_block - 1);
		uint16_t page_in_child_meta = block_in_child_meta >>
									  (page_size_exponent -
									   address_size_exponent);
		uint16_t byte_in_child_meta_page = (block_in_child_meta <<
											address_size_exponent) &
											(page_size - 1);

		file->data_block_address = 0;

		if (device_read(file->child_meta_block_address + page_in_child_meta,
						&(file->data_block_address), address_size,
						byte_in_child_meta_page))
		{
			return TEFS_ERR_READ;
		}

		if (file->data_block_address == TEFS_EMPTY ||
			file->data_block_address == TEFS_DELETED)
		{
			int8_t response;
			if ((response = tefs_reserve_device_block(&(file->data_block_address))))
			{
				return response;
			}

			if (device_write(file->child_meta_block_address + page_in_child_meta,
							 &(file->data_block_address), address_size,
							 byte_in_child_meta_page))
			{
				return TEFS_ERR_WRITE;
			}
		}

		if (device_write(file->data_block_address, data, number_of_bytes,
						 byte_offset))
		{
			return TEFS_ERR_WRITE;
		}

		file->data_block_number = file_page_address >> block_size_exponent;
	}

	file->current_page_number = file_page_address;

	return TEFS_ERR_OK;
}

int8_t
tefs_write_continuous_start(
	tefs_t 	*file,
	uint32_t 	start_file_page_address
)
{
	/* Check if page address is in the same block as the current data block. 
	   If it is, read or write the data. Otherwise get the new data block address
	   from the meta block and then read or write to the page in that new data block. */
	if (start_file_page_address == file->current_page_number ||
		(start_file_page_address >> block_size_exponent) ==
		 file->data_block_number)
	{
#ifndef USE_DATAFLASH
		sd_raw_write_continuous_start(file->data_block_address +
									  (start_file_page_address &
									   (block_size - 1)), block_size);
#endif /* USE_DATAFLASH */
	}
	else
	{
		/* Check if the page is in the same child meta block. If not, get the
		   address from the meta block or create a new child meta block if it
		   doesn't exist. */
		uint16_t child_block_number = (start_file_page_address >>
									   block_size_exponent >>
									   addresses_per_block_exponent);

		if ((file->data_block_number >> addresses_per_block_exponent) !=
			child_block_number)
		{
			uint16_t page_in_meta = child_block_number >>
									(page_size_exponent -
									 address_size_exponent);
			uint16_t byte_in_meta_page = (child_block_number <<
										  address_size_exponent) &
										 (addresses_per_block - 1);

			/* Check if the file has not reached its max capacity. */
			if (page_in_meta >= block_size)
			{
				return TEFS_ERR_FILE_FULL;
			}

			file->child_meta_block_address = 0;

			if (device_read(file->meta_block_address + page_in_meta,
							&(file->child_meta_block_address), address_size,
							byte_in_meta_page))
			{
				return TEFS_ERR_READ;
			}

			if (file->child_meta_block_address == TEFS_EMPTY ||
				file->child_meta_block_address == TEFS_DELETED)
			{
				int8_t response;
				if ((response = tefs_reserve_device_block(&(file->child_meta_block_address))))
				{
					return response;
				}

				if ((response = tefs_erase_block(file->child_meta_block_address)))
				{
					return response;
				}

				if (device_write(file->meta_block_address + page_in_meta,
								 &(file->child_meta_block_address),
								 address_size, byte_in_meta_page))
				{
					return TEFS_ERR_WRITE;
				}
			}
		}

		/* Get the data block from the child meta block. */
		uint32_t block_in_child_meta = (start_file_page_address >>
										block_size_exponent) &
										(addresses_per_block - 1);
		uint16_t page_in_child_meta = block_in_child_meta >>
									  (page_size_exponent -
									  	address_size_exponent);
		uint16_t byte_in_child_meta_page = (block_in_child_meta <<
											address_size_exponent) &
											(page_size - 1);

		file->data_block_address = 0;

		if (device_read(file->child_meta_block_address + page_in_child_meta,
						&(file->data_block_address), address_size,
						byte_in_child_meta_page))
		{
			return TEFS_ERR_READ;
		}

		if (file->data_block_address == TEFS_EMPTY ||
			file->data_block_address == TEFS_DELETED)
		{
			int8_t response;
			if ((response = tefs_reserve_device_block(&(file->data_block_address))))
			{
				return response;
			}

			if (device_write(file->child_meta_block_address + page_in_child_meta,
							 &(file->data_block_address), address_size,
							 byte_in_child_meta_page))
			{
				return TEFS_ERR_WRITE;
			}
		}

#ifndef USE_DATAFLASH
		sd_raw_write_continuous_start(file->data_block_address, block_size);
#endif /* USE_DATAFLASH */

		file->data_block_number = start_file_page_address >> block_size_exponent;
	}

	is_read_write_continuous = 1;
	file->current_page_number = start_file_page_address;

	return TEFS_ERR_OK;
}

int8_t
tefs_write_continuous(
	tefs_t 	*file,
	void	 	*data,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
)
{
#ifdef USE_DATAFLASH
	if (device_write(file->data_block_address + (file->current_page_number &
					 (block_size - 1)), data, number_of_bytes, byte_offset))
	{
		return TEFS_ERR_WRITE;
	}
#else /* USE_DATAFLASH */
	if (sd_raw_write_continuous(data, number_of_bytes, byte_offset))
	{
		return TEFS_ERR_WRITE;
	}
#endif

	return TEFS_ERR_OK;
}

int8_t
tefs_write_continuous_next(
	tefs_t *file
)
{
	/* Check if page address is in the same block as the current data block. 
	   If it is, read or write the data. Otherwise get the new data block
	   address from the meta block and then read or write to the page in that
	   new data block. */
	if ((file->current_page_number >> block_size_exponent) ==
		file->data_block_number)
	{
#ifndef USE_DATAFLASH
		sd_raw_write_continuous_next();
#endif /* USE_DATAFLASH */
	}
	else
	{
#ifndef USE_DATAFLASH
		sd_raw_write_continuous_stop();
#endif /* USE_DATAFLASH */
		is_read_write_continuous = 0;

		/* Check if the page is in the same child meta block. If not, get the
		   address from the meta block or create a new child meta block if it
		   doesn't exist. */
		uint16_t child_block_number = (file->current_page_number >>
									   block_size_exponent >>
									   addresses_per_block_exponent);

		if ((file->data_block_number >> addresses_per_block_exponent) !=
			child_block_number)
		{
			uint16_t page_in_meta = child_block_number >>
									(page_size_exponent -
									 address_size_exponent);
			uint16_t byte_in_meta_page = (child_block_number <<
										  address_size_exponent) &
										 (addresses_per_block - 1);

			/* Check if the file has not reached its max capacity. */
			if (page_in_meta >= block_size)
			{
				return TEFS_ERR_FILE_FULL;
			}

			file->child_meta_block_address = 0;

			if (device_read(file->meta_block_address + page_in_meta,
							&(file->child_meta_block_address), address_size,
							byte_in_meta_page))
			{
				return TEFS_ERR_READ;
			}

			if (file->child_meta_block_address == TEFS_EMPTY ||
				file->child_meta_block_address == TEFS_DELETED)
			{
				int8_t response;
				if ((response = tefs_reserve_device_block(&(file->child_meta_block_address))))
				{
					return response;
				}

				if ((response = tefs_erase_block(file->child_meta_block_address)))
				{
					return response;
				}

				if (device_write(file->meta_block_address + page_in_meta,
								 &(file->child_meta_block_address),
								 address_size, byte_in_meta_page))
				{
					return TEFS_ERR_WRITE;
				}
			}
		}

		/* Get the data block from the child meta block. */
		uint32_t block_in_child_meta = (file->current_page_number >>
										block_size_exponent) &
										(addresses_per_block - 1);
		uint16_t page_in_child_meta = block_in_child_meta >>
									  (page_size_exponent -
									   address_size_exponent);
		uint16_t byte_in_child_meta_page = (block_in_child_meta <<
											address_size_exponent) &
										   (page_size - 1);

		file->data_block_address = 0;

		if (device_read(file->child_meta_block_address + page_in_child_meta,
						&(file->data_block_address), address_size,
						byte_in_child_meta_page))
		{
			return TEFS_ERR_READ;
		}

		if (file->data_block_address == TEFS_EMPTY ||
			file->data_block_address == TEFS_DELETED)
		{
			int8_t response;
			if ((response = tefs_reserve_device_block(&(file->data_block_address))))
			{
				return response;
			}

			if (device_write(file->child_meta_block_address + page_in_child_meta,
							 &(file->data_block_address), address_size,
							 byte_in_child_meta_page))
			{
				return TEFS_ERR_WRITE;
			}
		}

#ifndef USE_DATAFLASH
		sd_raw_write_continuous_start(file->data_block_address, block_size);
#endif /* USE_DATAFLASH */
		is_read_write_continuous = 1;

		file->data_block_number = file->current_page_number >>
								  block_size_exponent;
	}

	(file->current_page_number)++;

	return TEFS_ERR_OK;
}

int8_t
tefs_write_continuous_stop(
	tefs_t *file
)
{
#ifndef USE_DATAFLASH
	if (sd_raw_write_continuous_stop())
	{
		return TEFS_ERR_WRITE;
	}
#endif /* USE_DATAFLASH */

	is_read_write_continuous = 0;

	return TEFS_ERR_OK;
}

int8_t
tefs_flush(
	tefs_t *file
)
{
	if (device_flush())
	{
		return TEFS_ERR_WRITE;
	}

	return TEFS_ERR_OK;
}

int8_t
tefs_read(
	tefs_t 	*file,
	uint32_t 	file_page_address,
	void		*buffer,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
)
{
	/* Check if page address is in the same block as the current data block. 
	   If it is, read the data. Otherwise get the new data block address
	   from the meta block and then read to the page in that new data block. */
	if (file_page_address == file->current_page_number ||
		(file_page_address >> block_size_exponent) == file->data_block_number)
	{
		if (device_read(file->data_block_address + (file_page_address &
						(block_size - 1)), buffer, number_of_bytes, byte_offset))
		{
			return TEFS_ERR_READ;
		}
	}
	else
	{
		/* Check if the page is in the same child meta block. If not, get the
		   address from the meta block or throw an error if it does not
		   exist. */
		uint16_t child_block_number = (file_page_address >>
									   block_size_exponent >>
									   addresses_per_block_exponent);

		if ((file->data_block_number >> addresses_per_block_exponent) !=
			child_block_number)
		{
			uint16_t page_in_meta = child_block_number >>
									(page_size_exponent -
									 address_size_exponent);
			uint16_t byte_in_meta_page = (child_block_number <<
										  address_size_exponent) &
										 (addresses_per_block - 1);

			/* Make sure that the file has not reached its max capacity. */
			if (page_in_meta >= block_size)
			{
				return TEFS_ERR_FILE_FULL;
			}

			file->child_meta_block_address = 0;

			if (device_read(file->meta_block_address + page_in_meta,
							&(file->child_meta_block_address), address_size,
							byte_in_meta_page))
			{
				return TEFS_ERR_READ;
			}

			if (file->child_meta_block_address == TEFS_EMPTY ||
				file->child_meta_block_address == TEFS_DELETED)
			{
				return TEFS_ERR_UNRELEASED_BLOCK;
			}
		}

		/* Get the data block from the child meta block. */
		uint32_t block_in_child_meta = (file_page_address >>
										block_size_exponent) &
									   (addresses_per_block - 1);
		uint16_t page_in_child_meta = block_in_child_meta >>
									  (page_size_exponent -
									   address_size_exponent);
		uint16_t byte_in_child_meta_page = (block_in_child_meta <<
											address_size_exponent) &
										   (page_size - 1);

		file->data_block_address = 0;

		if (device_read(file->child_meta_block_address + page_in_child_meta,
						&(file->data_block_address), address_size,
						byte_in_child_meta_page))
		{
			return TEFS_ERR_READ;
		}

		if (file->data_block_address == TEFS_EMPTY ||
			file->data_block_address == TEFS_DELETED)
		{
			return TEFS_ERR_UNRELEASED_BLOCK;
		}

		if (device_read(file->data_block_address, buffer, number_of_bytes,
						byte_offset))
		{
			return TEFS_ERR_READ;
		}

		file->data_block_number = file_page_address >> block_size_exponent;
	}

	file->current_page_number = file_page_address;

	return TEFS_ERR_OK;
}

int8_t
tefs_read_continuous_start(
	tefs_t 	*file,
	uint32_t 	start_file_page_address
)
{
	/* Check if page address is in the same block as the current data block. 
	   If it is, read or write the data. Otherwise get the new data block address
	   from the meta block and then read or write to the page in that new data block. */
	if (start_file_page_address == file->current_page_number ||
		(start_file_page_address >> block_size_exponent) ==
		file->data_block_number)
	{
#ifndef USE_DATAFLASH
		if (sd_raw_read_continuous_start(file->data_block_address +
										 (start_file_page_address &
										  (block_size - 1))))
		{
			return TEFS_ERR_READ;
		}
#endif /* USE_DATAFLASH */
	}
	else
	{
		/* Check if the page is in the same child meta block. If not, get the
		   address from the meta block or create a new child meta block if it
		   doesn't exist. */
		uint16_t child_block_number = (start_file_page_address >>
									   block_size_exponent >>
									   addresses_per_block_exponent);

		if ((file->data_block_number >> addresses_per_block_exponent) !=
			child_block_number)
		{
			uint16_t page_in_meta = child_block_number >>
									(page_size_exponent -
									 address_size_exponent);
			uint16_t byte_in_meta_page = (child_block_number <<
										  address_size_exponent) &
										 (addresses_per_block - 1);

			/* Check if the file has not reached its max capacity. */
			if (page_in_meta >= block_size)
			{
				return TEFS_ERR_FILE_FULL;
			}

			file->child_meta_block_address = 0;

			if (device_read(file->meta_block_address + page_in_meta,
							&(file->child_meta_block_address), address_size,
							byte_in_meta_page))
			{
				return TEFS_ERR_READ;
			}

			if (file->child_meta_block_address == TEFS_EMPTY ||
				file->child_meta_block_address == TEFS_DELETED)
			{
				return TEFS_ERR_UNRELEASED_BLOCK;
			}
		}

		/* Get the data block from the child meta block. */
		uint32_t block_in_child_meta = (start_file_page_address >>
										block_size_exponent) &
									   (addresses_per_block - 1);
		uint16_t page_in_child_meta = block_in_child_meta >>
									  (page_size_exponent -
									   address_size_exponent);
		uint16_t byte_in_child_meta_page = (block_in_child_meta <<
											address_size_exponent) &
										   (page_size - 1);

		file->data_block_address = 0;

		if (device_read(file->child_meta_block_address + page_in_child_meta,
						&(file->data_block_address), address_size,
						byte_in_child_meta_page))
		{
			return TEFS_ERR_READ;
		}

		if (file->data_block_address == TEFS_EMPTY ||
			file->data_block_address == TEFS_DELETED)
		{
			return TEFS_ERR_UNRELEASED_BLOCK;
		}

#ifndef USE_DATAFLASH
		if (sd_raw_read_continuous_start(file->data_block_address))
		{
			return TEFS_ERR_READ;
		}
#endif /* USE_DATAFLASH */

		file->data_block_number = start_file_page_address >>
								  block_size_exponent;
	}

	is_read_write_continuous = 1;
	file->current_page_number = start_file_page_address;

	return TEFS_ERR_OK;
}

int8_t
tefs_read_continuous(
	tefs_t 	*file,
	void 		*buffer,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
)
{
#ifdef USE_DATAFLASH
	if (device_read(file->data_block_address + (file->current_page_number &
					(block_size - 1)), buffer, number_of_bytes, byte_offset))
	{
		return TEFS_ERR_WRITE;
	}
#else /* USE_DATAFLASH */
	if (sd_raw_read_continuous(buffer, number_of_bytes, byte_offset))
	{
		return TEFS_ERR_READ;
	}
#endif

	return TEFS_ERR_OK;
}

int8_t
tefs_read_continuous_next(
	tefs_t *file
)
{
	/* Check if page address is in the same block as the current data block. 
	   If it is, read or write the data. Otherwise get the new data block
	   address from the meta block and then read or write to the page in that
	   new data block. */
	if ((file->current_page_number >> block_size_exponent) ==
		file->data_block_number)
	{
#ifndef USE_DATAFLASH
		sd_raw_read_continuous_next();
#endif /* USE_DATAFLASH */
	}
	else
	{
#ifndef USE_DATAFLASH
		sd_raw_read_continuous_stop();
#endif /* USE_DATAFLASH */
		is_read_write_continuous = 0;

		/* Check if the page is in the same child meta block. If not, get the
		   address from the meta block or create a new child meta block if it
		   doesn't exist. */
		uint16_t child_block_number = (file->current_page_number >>
									   block_size_exponent >>
									   addresses_per_block_exponent);

		if ((file->data_block_number >> addresses_per_block_exponent) !=
			child_block_number)
		{
			uint16_t page_in_meta = child_block_number >>
									(page_size_exponent -
									 address_size_exponent);
			uint16_t byte_in_meta_page = (child_block_number <<
										  address_size_exponent) &
										 (addresses_per_block - 1);

			/* Check if the file has not reached its max capacity. */
			if (page_in_meta >= block_size)
			{
				return TEFS_ERR_FILE_FULL;
			}

			file->child_meta_block_address = 0;

			if (device_read(file->meta_block_address + page_in_meta,
							&(file->child_meta_block_address), address_size,
							byte_in_meta_page))
			{
				return TEFS_ERR_READ;
			}

			if (file->child_meta_block_address == TEFS_EMPTY ||
				file->child_meta_block_address == TEFS_DELETED)
			{
				return TEFS_ERR_UNRELEASED_BLOCK;
			}
		}

		/* Get the data block from the child meta block. */
		uint32_t block_in_child_meta = (file->current_page_number >>
										block_size_exponent) &
									   (addresses_per_block - 1);
		uint16_t page_in_child_meta = block_in_child_meta >>
									  (page_size_exponent -
									   address_size_exponent);
		uint16_t byte_in_child_meta_page = (block_in_child_meta <<
											address_size_exponent) &
										   (page_size - 1);

		file->data_block_address = 0;

		if (device_read(file->child_meta_block_address + page_in_child_meta,
						&(file->data_block_address), address_size,
						byte_in_child_meta_page))
		{
			return TEFS_ERR_READ;
		}

		if (file->data_block_address == TEFS_EMPTY ||
			file->data_block_address == TEFS_DELETED)
		{
			return TEFS_ERR_UNRELEASED_BLOCK;
		}

#ifndef USE_DATAFLASH
		sd_raw_read_continuous_start(file->data_block_address);
#endif /* USE_DATAFLASH */
		is_read_write_continuous = 1;

		file->data_block_number = file->current_page_number >>
								  block_size_exponent;
	}

	(file->current_page_number)++;

	return TEFS_ERR_OK;
}

int8_t
tefs_read_continuous_stop(
	tefs_t *file
)
{
#ifndef USE_DATAFLASH	
	if (sd_raw_read_continuous_stop())
	{
		return TEFS_ERR_READ;
	}
#endif /* USE_DATAFLASH */

	is_read_write_continuous = 0;

	return TEFS_ERR_OK;
}

int8_t
tefs_release_block(
	tefs_t 	*file,
	uint32_t 	file_block_address
)
{
	uint16_t child_block_number = (file_block_address >>
								   addresses_per_block_exponent);
	uint16_t page_in_meta = child_block_number >>
							(page_size_exponent - address_size_exponent);
	uint16_t byte_in_meta_page = (child_block_number << address_size_exponent) &
								 (addresses_per_block - 1);

	uint32_t block_in_child_meta = (file_block_address >> block_size_exponent) &
								   (addresses_per_block - 1);
	uint16_t page_in_child_meta = block_in_child_meta >>
								  (page_size_exponent - address_size_exponent);
	uint16_t byte_in_child_meta_page = (block_in_child_meta <<
										address_size_exponent) &
									   (page_size - 1);

	if (file_block_address != file->data_block_number)
	{
		/* Check if the block is in the same child meta block. If not, get the
		   address from the meta block. */
		if ((file->data_block_number >> addresses_per_block_exponent) !=
			child_block_number)
		{
			if (device_read(file->meta_block_address + page_in_meta,
							&(file->child_meta_block_address), address_size,
							byte_in_meta_page))
			{
				return TEFS_ERR_READ;
			}

			if (file->child_meta_block_address == TEFS_EMPTY ||
				file->child_meta_block_address == TEFS_DELETED)
			{
				return TEFS_ERR_UNRELEASED_BLOCK;
			}
		}

		/* Get the data block from the child meta block. */
		if (device_read(file->child_meta_block_address + page_in_child_meta,
						&(file->data_block_address), address_size,
						byte_in_child_meta_page))
		{
			return TEFS_ERR_READ;
		}

		if (file->data_block_address == TEFS_EMPTY ||
			file->data_block_address == TEFS_DELETED)
		{
			return TEFS_ERR_UNRELEASED_BLOCK;
		}

		file->data_block_number = file_block_address;
	}

	/* Release block. */
	int8_t response;
	if ((response = tefs_release_device_block(file->data_block_address)))
	{
		return response;
	}

	/* Remove the address from the child meta block. */
	uint8_t byte_buffer = TEFS_DELETED;
	if (device_write(file->child_meta_block_address + page_in_child_meta,
					 &byte_buffer, 1, byte_in_child_meta_page))
	{
		return TEFS_ERR_WRITE;
	}

	byte_buffer = 0;
	uint8_t current_byte = 1;
	while (current_byte < address_size)
	{
		if (device_write(file->child_meta_block_address + page_in_child_meta,
						 &byte_buffer, 1,
						 byte_in_child_meta_page + current_byte))
		{
			return TEFS_ERR_WRITE;
		}
		current_byte++;
	}

	uint8_t contains_blocks = 0;

	/* Scan child meta block to see if it's tefs_empty. */
	for (page_in_child_meta = 0;
		 page_in_child_meta < block_size && !contains_blocks;
		 page_in_child_meta++)
	{
		for (byte_in_child_meta_page = 0;
			 byte_in_child_meta_page < page_size && !contains_blocks;
			 byte_in_child_meta_page++)
		{
			if (device_read(file->child_meta_block_address + page_in_child_meta,
							&byte_buffer, 1, byte_in_child_meta_page))
			{
				return TEFS_ERR_READ;
			}

			if (byte_buffer != TEFS_DELETED && byte_buffer != TEFS_EMPTY)
			{
				contains_blocks = 1;
			}
		}
	}

	/* Remove child meta block from meta block if it's tefs_empty. */
	if (!contains_blocks)
	{
		byte_buffer = TEFS_DELETED;
		if (device_write(file->meta_block_address + page_in_meta, &byte_buffer,
						 1, byte_in_meta_page))
		{
			return TEFS_ERR_WRITE;
		}

		byte_buffer = 0;
		current_byte = 1;
		while (current_byte < address_size)
		{
			if (device_write(file->meta_block_address + page_in_meta,
							 &byte_buffer, 1, byte_in_meta_page + current_byte))
			{
				return TEFS_ERR_WRITE;
			}
			current_byte++;
		}

		if ((response = tefs_release_device_block(file->child_meta_block_address)))
		{
			return response;
		}
	}

	if (device_flush())
	{
		return TEFS_ERR_WRITE;
	}

	return TEFS_ERR_OK;
}

static int8_t
tefs_reserve_device_block(
	uint32_t	*block_address
)
{
	if (is_block_pool_empty)
	{
		return TEFS_ERR_DEVICE_FULL;
	}

	uint8_t byte_buffer = 0;

	/* Read in byte. */
	if (device_read(((state_section_bit >> 3) >> page_size_exponent) + 1,
					&byte_buffer, 1, (state_section_bit >> 3) &
					(page_size - 1)))
	{
		return TEFS_ERR_READ;
	}

	/* Check if the block has already been reserved. */
	if (!(byte_buffer & (1 << (7 - (state_section_bit & 7)))))
	{
		return TEFS_ERR_OK;
	}

	/* Toggle state bit in byte from 1 to 0. */
	byte_buffer &= (uint8_t) ~(1 << (7 - (state_section_bit & 7)));

	/* Write out the byte */
	if (device_write(((state_section_bit >> 3) >> page_size_exponent) + 1,
					 &byte_buffer, 1, (state_section_bit >> 3) &
					 (page_size - 1)))
	{
		return TEFS_ERR_WRITE;
	}

	*block_address = (state_section_bit << block_size_exponent) +
					 (1 + state_section_size + directory_size);
	state_section_bit++;

#ifdef USE_DATAFLASH
	/* Reserve pages on FTL. */
	uint16_t i;
	for (i = 0; i < block_size; i++)
	{
		dataflash_get_page();
	}
#endif /* USE_DATAFLASH */

	/* Find the next unreserved block. */
	int8_t response;
	if ((response = tefs_find_next_empty_block(&state_section_bit)))
	{
		return response;
	}

	if (device_flush())
	{
		return TEFS_ERR_WRITE;
	}

	return TEFS_ERR_OK;
}

static int8_t
tefs_release_device_block(
	uint32_t	block_address
)
{
	/* Start byte for the state section that is correlated to the block address. */
	uint32_t state_bit = (block_address -
						  (1 + state_section_size + directory_size)) >>
						 block_size_exponent;
	uint8_t byte_buffer = 0;

	/* Read in byte. */
	if (device_read(((state_bit >> 3) >> page_size_exponent) + 1, &byte_buffer,
					1, (state_bit >> 3) & (page_size - 1)))
	{
		return TEFS_ERR_READ;
	}

	/* Check if the block has already been released. */
	if (byte_buffer & (1 << (7 - (state_bit & 7))))
	{
		return TEFS_ERR_OK;
	}

	/* Toggle state bit in byte from 0 to 1. */
	byte_buffer |= (uint8_t) (1 << (7 - (state_bit & 7)));

	/* Write out the byte. */
	if (device_write(((state_bit >> 3) >> page_size_exponent) + 1, &byte_buffer,
					 1, (state_bit >> 3) & (page_size - 1)))
	{
		return TEFS_ERR_WRITE;
	}

#ifdef USE_DATAFLASH
	/* Releases pages on FTL. */
	uint16_t i;
	for (i = 0; i < block_size; i++)
	{
		dataflash_return_page();
	}
#endif /* USE_DATAFLASH */

	if (device_flush())
	{
		return TEFS_ERR_WRITE;
	}

	if (state_bit < state_section_bit)
	{
		state_section_bit = state_bit;
	}

	is_block_pool_empty = 0;
	return TEFS_ERR_OK;
}

static int8_t
tefs_erase_block(
	uint32_t	block_address
)
{
	/* Erase pages in meta block. */
	uint16_t current_page;
	uint8_t flag = TEFS_EMPTY;

#ifndef USE_DATAFLASH
	if (sd_raw_write_continuous_start(block_address, block_size))
	{
		return TEFS_ERR_WRITE;
	}
#endif /* USE_DATAFLASH */

	for (current_page = block_address;
		 current_page < block_size + block_address;
		 current_page++)
	{	
		uint16_t current_byte;
		for (current_byte = 0;
			 current_byte < page_size;
			 current_byte++)
		{
#ifdef USE_DATAFLASH
			if (write_bytes(current_page, &flag, 1, current_byte))
			{
				return TEFS_ERR_WRITE;
			}
#else /* USE_DATAFLASH */
			if (sd_raw_write_continuous(&flag, 1, current_byte))
			{
				return TEFS_ERR_WRITE;
			}
#endif
		}

#ifndef USE_DATAFLASH
		if (sd_raw_write_continuous_next())
		{
			return TEFS_ERR_WRITE;
		}
#endif /* USE_DATAFLASH */
	}

#ifndef USE_DATAFLASH
	if (sd_raw_write_continuous_stop())
	{
		return TEFS_ERR_WRITE;
	}
#endif /* USE_DATAFLASH */

	return TEFS_ERR_OK;
}

static int8_t
tefs_find_next_empty_block(
	uint32_t	*state_section_bit
)
{
	uint32_t current_page = (*state_section_bit >> 3) >> page_size_exponent;
	uint16_t current_byte = (*state_section_bit >> 3) & (page_size - 1);
	uint8_t byte_buffer = 0;

#ifndef USE_DATAFLASH
	if (sd_raw_read_continuous_start(current_page + 1))
	{
		return TEFS_ERR_READ;
	}
#endif /* USE_DATAFLASH */

	while (current_page < state_section_size)
	{
		while (current_byte < page_size)
		{
#ifdef USE_DATAFLASH
			if (read_bytes(current_page + 1, &byte_buffer, 1, current_byte))
			{
				return TEFS_ERR_READ;
			}
#else
			if (sd_raw_read_continuous(&byte_buffer, 1, current_byte))
			{
				return TEFS_ERR_READ;
			}		
#endif /* USE_DATAFLASH */

			if (byte_buffer)
			{
				uint8_t mask = 0x80;
				uint8_t count = 0;
				while (mask != 0 && !(byte_buffer & mask))
				{
				    mask >>= 1;
				    count++;
				}

				*state_section_bit = (current_page << page_size_exponent << 3) +
									 (current_byte << 3) + count;

#ifndef USE_DATAFLASH
				if (sd_raw_read_continuous_stop())
				{
					return TEFS_ERR_READ;
				}
#endif /* USE_DATAFLASH */

				return TEFS_ERR_OK;
			}

			current_byte++;
		}

#ifndef USE_DATAFLASH
		if (sd_raw_read_continuous_next())
		{
			return TEFS_ERR_READ;
		}
#endif /* USE_DATAFLASH */

		current_page++;
		current_byte = 0;
	}

#ifndef USE_DATAFLASH
	if (sd_raw_read_continuous_stop())
	{
		return TEFS_ERR_READ;
	}
#endif /* USE_DATAFLASH */

	is_block_pool_empty = 1;

	return TEFS_ERR_OK;
}

static uint8_t
tefs_power_of_two_exponent(
	uint32_t	number
)
{
	uint8_t position = 0;
	while (((number & 1) == 0) && number > 1)
	{
		number >>= 1;
		position++;
	}

	return position;
}

static int8_t
tefs_load_card_data(
	void
)
{
	uint16_t current_byte = 0;
	uint8_t buffer = 0;

	/* Read and verify the check flag. */
	for (current_byte = 0; current_byte < 4; current_byte++)
	{
		if (device_read(0, &buffer, 1, current_byte))
		{
			return TEFS_ERR_READ;
		}

		if (buffer != TEFS_CHECK_FLAG)
		{
			return TEFS_ERR_NOT_FORMATTED;
		}
	}

	/* Read the number of pages that the device has. */
	if (device_read(0, &number_of_pages, 4, current_byte))
	{
		return TEFS_ERR_READ;
	}
	current_byte += 4;

	/* Read the physical page size. */
	if (device_read(0, &page_size_exponent, 1, current_byte))
	{
		return TEFS_ERR_READ;
	}
	current_byte += 1;

	/* Read the block size. */
	if (device_read(0, &block_size_exponent, 1, current_byte))
	{
		return TEFS_ERR_READ;
	}
	current_byte += 1;

	/* Read the key size. */
	if (device_read(0, &key_size, 1, current_byte))
	{
		return TEFS_ERR_READ;
	}
	current_byte += 1;

	/* Read the address size. */
	if (device_read(0, &address_size_exponent, 1, current_byte))
	{
		return TEFS_ERR_READ;
	}
	current_byte += 1;

	/* Read the state section size. */
	if (device_read(0, &state_section_size, 4, current_byte))
	{
		return TEFS_ERR_READ;
	}
	current_byte += 4;

	/* Read the directory size. */
	if (device_read(0, &directory_size, 2, current_byte))
	{
		return TEFS_ERR_READ;
	}

	block_size = 1 << block_size_exponent;
	page_size = 1 << page_size_exponent;
	address_size = 1 << address_size_exponent;

	addresses_per_block = (page_size << block_size_exponent) >>
						  address_size_exponent;
	addresses_per_block_exponent =
		tefs_power_of_two_exponent(addresses_per_block);

	/* Get first byte in state for a free block. */
	state_section_bit = 0;
	int8_t response;
	if ((response = tefs_find_next_empty_block(&state_section_bit)))
	{
		return response;
	}

#ifdef DEBUG
	printf("State section bit: %lu\n", state_section_bit);
	printf("State section size: %lu\n", state_section_size);
	printf("Directory size: %u\n", directory_size);
	printf("Number of pages: %lu\n", number_of_pages);
	printf("Page size: %u\n", page_size);
	printf("Page size exponent: %u\n", page_size_exponent);
	printf("Addresses per block: %lu\n", addresses_per_block);
	printf("Addresses per block exponent: %u\n", addresses_per_block_exponent);
	printf("Block size: %u\n", block_size);
	printf("Block size exponent: %u\n", block_size_exponent);
	printf("Key size: %u\n", key_size);
	printf("Address size: %u\n", address_size);
	printf("Address size exponent: %u\n", address_size_exponent);
#endif /* DEBUG */

	return TEFS_ERR_OK;
}
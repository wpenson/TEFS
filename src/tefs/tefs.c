/******************************************************************************/
/**
@file		tefs.c
@author     Wade Penson
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

#if defined(USE_SD)
/** Current bit that represents a free block in the state section. */
static uint32_t state_section_bit					= 0xFFFFFFFF;
/** The size of the state section. */
static uint32_t state_section_size					= 0;
/** Keeps track if the state section is empty or not. */
static uint8_t	is_block_pool_empty					= 0;
#endif

/** The number of pages that the device has. */
static uint32_t number_of_pages						= 0;
/** The size of a page in bytes. */
static uint16_t page_size							= 0;
/** The number of addresses that can fit into a block. */
static uint32_t addresses_per_block					= 0;
/** The size of a block in pages. */
static uint16_t block_size							= 0;
/** The size of an address in bytes. */
static uint8_t	address_size						= 0;
/** The power of 2 exponent for the size of a page (used to improve
	performance). */
static uint8_t	page_size_exponent					= 0;
/** The power of 2 exponent for the number of addresses that can fit into a
	block (used to improve performance). */
static uint8_t	addresses_per_block_exponent		= 0;
/** The power of 2 exponent for the size of a block (used to improve
	performance). */
static uint8_t	block_size_exponent					= 0;
/** The power of 2 exponent for the size of an address (used to improve
	performance). */
static uint8_t	address_size_exponent				= 0;
/**	The size of a hash value. Either 2 or 4 bytes. */
static uint8_t 	hash_size							= 0;
/** The size of a metadata entry. */
static uint16_t metadata_size						= 0;
/** The max size for a file name. */
static uint16_t max_file_name_size					= 0;
/** Metadata entries file used by the directory. */
static file_t	metadata;
/** Hash entries file used by the directory. */
static file_t	hash_entries;

#if defined(TEFS_CONTINUOUS_SUPPORT)
/** Determines if continuous writing or reading is in progress. */
static uint8_t	is_read_write_continuous			= 0;
#endif

/**
@brief		Finds the directory entry corresponding to the file name. The page
 			address along with the byte in that page is returned for the
 			location in the metadata file. If

@param[in]	file_name			Name of file as char array with null bit.
@param[out]	dir_page_address	Page in the metadata file where the metadata entry is.
@param[out]	dir_byte_in_page	Byte in the page of the file where the metadata entry is.
@param		file_operation		0 == find but return an error if the file was not found
 								1 == find + create file if doesn't exist
 								2 == remove directory entry for the file

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
static int8_t
tefs_find_file_directory_entry(
	char		*file_name,
	uint32_t	*dir_page_address,
	uint16_t	*dir_byte_in_page,
	uint8_t		file_operation
);

/**
@brief		Reserves a block and returns the address to the start of the block.

@param[out]	*block_address	Reserved block address

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
static int8_t
tefs_reserve_device_block(
	uint32_t *block_address
);

/**
@brief	Releases a block.

@param	block_address	The address of the block to release.

@return	An error code as defined by one of the TEFS_ERR_* definitions.
*/
static int8_t
tefs_release_device_block(
	uint32_t block_address
);

/**
@brief	Erases a block and fills it with zeros.

@param	block_address	The address of the block to erase.

@return	An error code as defined by one of the TEFS_ERR_* definitions.
*/
static int8_t
tefs_erase_block(
	uint32_t block_address
);

#if defined(USE_SD)
/**
@brief	Finds the next empty block in the block state section.

@param	*state_section_bit	The bit that represents the next empty block.

@return	An error code as defined by one of the TEFS_ERR_* definitions.
*/
static int8_t
tefs_find_next_empty_block(
	uint32_t *state_section_bit
);
#endif

/**
@brief		This maps a logical page in the file to the location of the
 			pointer in the root index block.

@param		page						The logical page in the file.

@param[out]	*page_in_root_index			The page in the root index where the pointer is.

@param[out]	*byte_in_root_index_page	The byte in the page of the root index where the pointer is.
*/
static void
tefs_map_page_to_root_index_address(
	uint32_t 	page,
	uint16_t 	*page_in_root_index,
	uint16_t 	*byte_in_root_index_page
);

/**
@brief		This maps a logical page in the file to the location of the
 			pointer in the child index block.

@param		page						The logical page in the file.

@param[out]	*page_in_child_index		The page in the child index where the pointer is.

@param[out]	*byte_in_child_index_page	The byte in the page of the child index where the pointer is.
*/
static void
tefs_map_page_to_child_index_address(
	uint32_t 	page,
	uint16_t 	*page_in_child_index,
	uint16_t 	*byte_in_child_index_page
);

/**
@brief	Writes out the file size to the directory entry of the file.

@param	file	A file_t structure.

@return	An error code as defined by one of the TEFS_ERR_* definitions.
*/
static int8_t
tefs_update_file_size(
	file_t *file
);

/**
@brief	Finds the bit position from the right for a number that is of power 2.

@param	number	A number that is a power of 2.

@return	The bit position.
*/
static uint8_t
tefs_power_of_two_exponent(
	uint32_t number
);

/**
@brief	Gets the data from the information section of the card and loads it into memory.

@return	An error code as defined by one of the TEFS_ERR_* definitions.
*/
static int8_t
tefs_load_card_data(
	void
);

/**
@brief	DJB2a hash function that hashes strings.

@param	*str	A string with a null byte at the end.

@return	The hash of the string.
*/
static uint32_t
hash_string(
	char *str
);

int8_t
tefs_format_device(
	uint32_t 	num_pages,
	uint16_t 	physical_page_size,
	uint16_t 	logical_block_size,
	uint8_t		hash_size,
	uint16_t	metadata_size,
	uint16_t 	max_file_name_size,
	uint8_t		erase_before_format
)
{
	if (erase_before_format)
	{
#if defined(USE_SD)
        /* Erase the storage device before formatting. */
		if (sd_spi_erase_all())
		{
			return TEFS_ERR_ERASE;
		}
#endif
        /* TODO: Add erase for dataflash */
	}

	/* Determine the size of an address. */
	if (num_pages < POW_2_TO(16))
	{
		address_size = 2;
		address_size_exponent = 1;
	}
	else
	{
		address_size = 4;
		address_size_exponent = 2;
	}

	page_size = physical_page_size;
	block_size = logical_block_size;
	page_size_exponent = tefs_power_of_two_exponent(physical_page_size);
	block_size_exponent = tefs_power_of_two_exponent(logical_block_size);

#if defined(USE_SD)
	/* Calculate the state section size. */
	/* TODO: Get the number of bits instead of bytes */
	//uint32_t state_section_size_in_bits = ((format_info->number_of_pages - info_section_size) - 1) / (format_info->block_size) + 1;
	uint32_t state_section_size_in_bytes =
		DIV_BY_POW_2_EXP(num_pages - TEFS_INFO_SECTION_SIZE, block_size_exponent + 3);

	state_section_size = DIV_BY_POW_2_EXP(state_section_size_in_bytes - 1, page_size_exponent) + 1;
#endif

#if defined(USE_DATAFLASH) && defined(USE_FTL)
	/* Reserve the pages required for the info page, and directory. */
	uint32_t i;
	for (i = 0; i < TEFS_INFO_SECTION_SIZE + directory_size; i++)
	{
		flare_logical_page_t p;
		flare_GetPage(&ftl, &p);
	}
#endif

	uint16_t current_byte = 0;
	uint8_t data = 0;

	/* Fill the info block with zeros first. */
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

	/* Write the address size. */
	if (device_write(0, &address_size_exponent, 1, current_byte))
	{
		return TEFS_ERR_WRITE;
	}

	current_byte += 1;

	/* Write the size of a hash value. */
	if (device_write(0, &hash_size, 1, current_byte))
	{
		return TEFS_ERR_WRITE;
	}

	current_byte += 1;

	/* Write the size of a metadata record. */
	if (device_write(0, &metadata_size, 2, current_byte))
	{
		return TEFS_ERR_WRITE;
	}

	current_byte += 2;

	/* Write the max size for a file name. */
	if (device_write(0, &max_file_name_size, 2, current_byte))
	{
		return TEFS_ERR_WRITE;
	}

	current_byte += 2;

#if defined(USE_SD)
	/* Write the state section size. */
	if (device_write(0, &state_section_size, 4, current_byte))
	{
		return TEFS_ERR_WRITE;
	}

	current_byte += 4;
#endif

	/* Write out the metadata for the hash entries file and meta data file. */
	uint8_t i;
	for (i = 0; i < 2; i++) {
		int8_t response;

		/* Get the child index block address and the data block address for the file. */
		uint32_t child_index_block_address = MULT_BY_POW_2_EXP((uint32_t) (i * 2), block_size_exponent) +
												(1 + state_section_size);
		uint32_t data_block_address = MULT_BY_POW_2_EXP((uint32_t) (i * 2 + 1), block_size_exponent) +
										(1 + state_section_size);

		/* Set file size to 0 for hash file directory entry. */
		uint32_t zero = 0;
		if (device_write(0, &zero, TEFS_DIR_EOF_PAGE_SIZE, current_byte))
		{
			return TEFS_ERR_WRITE;
		}

		current_byte += TEFS_DIR_EOF_PAGE_SIZE;

		if (device_write(0, &zero, TEFS_DIR_EOF_BYTE_SIZE, current_byte))
		{
			return TEFS_ERR_WRITE;
		}

		current_byte += TEFS_DIR_EOF_BYTE_SIZE;

		/* Write root index block address to the information page. */
		if (device_write(0, &child_index_block_address, address_size, current_byte))
		{
			return TEFS_ERR_WRITE;
		}

		current_byte += 4;

		/* Write the first data block address to the beginning of the root index. */
		if (device_write(child_index_block_address, &data_block_address, address_size, 0))
		{
			return TEFS_ERR_WRITE;
		}
	}

	/* Write out the state section. Set the first four bits as 0 since the */
#if defined(USE_SD)
	uint32_t current_page;
	data = 0xFF;

	for (current_page = TEFS_INFO_SECTION_SIZE;
		 current_page < state_section_size + TEFS_INFO_SECTION_SIZE;
		 current_page++)
	{
		for (current_byte = 0; 
			 current_byte < physical_page_size;
			 current_byte++)
		{
			if (current_page == state_section_size && current_byte ==
				MOD_BY_POW_2(state_section_size_in_bytes, POW_2_TO(page_size_exponent)))
			{
				data = 0;
			}

			if (sd_spi_write(current_page, &data, 1, current_byte))
			{
				return TEFS_ERR_WRITE;
			}
		}
	}

	data = 0x0F;
	if (sd_spi_write(1, &data, 1, 0))
	{
		return TEFS_ERR_WRITE;
	}
#endif

	if (device_flush())
	{
		return TEFS_ERR_WRITE;
	}

	/* Address size is set to 0 so that the data is loaded from the card into memory. */
	address_size = 0;

	return TEFS_ERR_OK;
}

int8_t
tefs_open(
	file_t 		*file,
	char 		*file_name
)
{
	int8_t response;

	/* Load the data from the information page if it has not been previously loaded. */
	if (address_size == 0)
	{
		if ((response = tefs_load_card_data()))
		{
			return response;
		}
	}

	/* Get the size of the file name. Return an error if it exceeds the max length. */
	uint16_t file_name_size = 0;
	while (file_name[file_name_size] != '\0')
	{
		file_name_size++;
	}

	if (file_name_size > max_file_name_size)
	{
		return TEFS_ERR_FILE_NAME_TOO_LONG;
	}

	if ((response = tefs_find_file_directory_entry(file_name, &(file->directory_page), &(file->directory_byte), 1)) !=
		TEFS_NEW_FILE && response != TEFS_ERR_OK)
	{
		return response;
	}

	file->root_index_block_address		= 0;
	file->child_index_block_address		= 0;
	file->data_block_address			= 0;

	uint16_t meta_entry_byte = file->directory_byte;

	if (response == TEFS_NEW_FILE) /* Create a new file */
	{
		uint8_t zero = 0;

		/* Write out zero to the status for now to avoid writing past the end of the file. */
		if ((response = tefs_write(&metadata, file->directory_page, &zero, 1, meta_entry_byte)))
		{
			return response;
		}

		meta_entry_byte++;

		/* Set file size to 0 for directory entry. */
		file->eof_page = 0;
		file->eof_byte = 0;
		if ((response = tefs_write(&metadata, file->directory_page, &(file->eof_page),
								   TEFS_DIR_EOF_PAGE_SIZE, meta_entry_byte)))
		{
			return response;
		}

		meta_entry_byte += TEFS_DIR_EOF_PAGE_SIZE;

		if ((response = tefs_write(&metadata, file->directory_page, &(file->eof_byte),
								   TEFS_DIR_EOF_BYTE_SIZE, meta_entry_byte)))
		{
			return response;
		}

		meta_entry_byte += TEFS_DIR_EOF_BYTE_SIZE;

		/* Write root index block address to directory for file ID. */
		if ((response = tefs_reserve_device_block(&(file->child_index_block_address))))
		{
			return response;
		}

		if ((response = tefs_write(&metadata, file->directory_page, &(file->child_index_block_address),
								   4, meta_entry_byte)))
		{
			return response;
		}

		meta_entry_byte += TEFS_DIR_ROOT_INDEX_ADDRESS_SIZE;

		/* Write file name to directory entry and pad with null chars. */
		if ((response = tefs_write(&metadata, file->directory_page, file_name, file_name_size, meta_entry_byte)))
		{
			return response;
		}

		meta_entry_byte += file_name_size;

		/* Write out the file name and pad it up until the max length. */
		uint8_t i;
		for (i = 0; i < max_file_name_size - file_name_size; i++)
		{
			char null_char = '\0';
			if ((response = tefs_write(&metadata, file->directory_page, &null_char, 1, meta_entry_byte)))
			{
				return response;
			}

			meta_entry_byte++;
		}

		// TODO: Write out metadata

		/* Pad the rest of the metadata entry with 0s. */
		while (MOD_BY_POW_2(meta_entry_byte, metadata_size) != 0)
		{
			if ((response = tefs_write(&metadata, file->directory_page, &zero, 1, meta_entry_byte)))
			{
				return response;
			}

			meta_entry_byte++;
		}

		/* Change status to IN_USE for directory entry. */
		uint8_t status = TEFS_IN_USE;
		if ((response = tefs_write(&metadata, file->directory_page, &status, 1, file->directory_byte)))
		{
			return response;
		}

		/* Write first data block address to first child index block. */
		if ((response = tefs_reserve_device_block(&(file->data_block_address)))
			 != 0 && response != TEFS_ERR_DEVICE_FULL)
		{
			return response;
		}

		if (device_write(file->child_index_block_address, &(file->data_block_address), address_size, 0))
		{
			return TEFS_ERR_WRITE;
		}

		if (device_flush())
		{
			return TEFS_ERR_WRITE;
		}
	}
	else /* Open an existing file. */
	{
		meta_entry_byte += TEFS_DIR_STATUS_SIZE;

		/* Read the file size. */
		if ((response = tefs_read(&metadata, file->directory_page, &(file->eof_page),
								  TEFS_DIR_EOF_PAGE_SIZE, meta_entry_byte)))
		{
			return response;
		}

		meta_entry_byte += TEFS_DIR_EOF_PAGE_SIZE;

		if ((response = tefs_read(&metadata, file->directory_page, &(file->eof_byte),
								  TEFS_DIR_EOF_BYTE_SIZE, meta_entry_byte)))
		{
			return response;
		}

		meta_entry_byte += TEFS_DIR_EOF_BYTE_SIZE;

		/* Read the file pointer. */
		if ((response = tefs_read(&metadata, file->directory_page, &(file->root_index_block_address),
								  address_size, meta_entry_byte)))
		{
			return response;
		}

		if (file->eof_page >= MULT_BY_POW_2_EXP(block_size, page_size_exponent - address_size_exponent))
		{
			/* Read the first child index block address. */
			if (device_read(file->root_index_block_address, &(file->child_index_block_address), address_size, 0))
			{
				return TEFS_ERR_READ;
			}

			/* Read the first data block address. */
			if (device_read(file->child_index_block_address, &(file->data_block_address), address_size, 0))
			{
				return TEFS_ERR_READ;
			}
		}
		else
		{
			file->child_index_block_address = file->root_index_block_address;

			/* Read the first data block address. */
			if (device_read(file->child_index_block_address, &(file->data_block_address), address_size, 0))
			{
				return TEFS_ERR_READ;
			}
		}
	}

	file->data_block_number				= 0;
	file->current_page_number			= 0;
	file->is_file_size_consistent 		= 1;

	return TEFS_ERR_OK;
}

int8_t
tefs_exists(
	char *file_name
)
{
	int8_t response;

	/* Load the data from the information page if it has not been previously loaded. */
	if (address_size == 0)
	{
		if ((response = tefs_load_card_data()))
		{
			return response;
		}
	}

	uint32_t directory_page;
	uint16_t directory_byte;

	if (tefs_find_file_directory_entry(file_name, &directory_page, &directory_byte, 0))
	{
		return 0;
	}

	/* Check if the status for the file's directory entry is set to IN_USE. */
	uint8_t status;
	if ((response = tefs_read(&metadata, directory_page, &status, 1, directory_byte)))
	{
		return response;
	}

	if (status == TEFS_IN_USE)
	{
		return 1;
	}

	return 0;
}

int8_t
tefs_close(
	file_t *file
)
{
	if (tefs_flush(file))
	{
		return TEFS_ERR_WRITE;
	}

	return TEFS_ERR_OK;
}

int8_t
tefs_remove(
	char *file_name
)
{
	int8_t response;

	/* Load the data from the information page if it has not been previously loaded. */
	if (address_size == 0)
	{
		if ((response = tefs_load_card_data()))
		{
			return response;
		}
	}

	uint32_t directory_page;
	uint16_t directory_byte;

	if ((response = tefs_find_file_directory_entry(file_name, &directory_page, &directory_byte, 2)))
	{
		return response;
	}

	uint32_t root_index_block_address = 0;

	/* Get the root index block address for the file. */
	if ((response = tefs_read(&metadata, directory_page, &root_index_block_address, TEFS_DIR_ROOT_INDEX_ADDRESS_SIZE,
							  directory_byte + TEFS_DIR_STATUS_SIZE + TEFS_DIR_EOF_BYTE_SIZE + TEFS_DIR_EOF_PAGE_SIZE)))
	{
		return response;
	}

	uint16_t root_index_current_byte;
	uint16_t root_index_current_page;

	uint32_t eof_page;

	/* Read the file size. */
	if ((response = tefs_read(&metadata, directory_page, &(eof_page), TEFS_DIR_EOF_PAGE_SIZE, directory_byte + TEFS_DIR_STATUS_SIZE)))
	{
		return response;
	}

	uint16_t page_in_root_index;
	uint16_t byte_in_root_index_page;
	tefs_map_page_to_root_index_address(eof_page, &page_in_root_index, &byte_in_root_index_page);

	uint16_t page_in_child_index;
	uint16_t byte_in_child_index_page;
	tefs_map_page_to_child_index_address(eof_page, &page_in_child_index, &byte_in_child_index_page);

	int8_t past_end = 0;

	/* Release all blocks that are in the file. */
	for (root_index_current_page = 0;
		 root_index_current_page <= page_in_root_index && !past_end;
		 root_index_current_page++)
	{
		for (root_index_current_byte = 0;
			 root_index_current_byte < page_size && !past_end;
			 root_index_current_byte += address_size)
		{
			uint32_t child_index_block_address = 0;

			if (eof_page >= MULT_BY_POW_2_EXP(block_size, page_size_exponent - address_size_exponent))
			{
				if (device_read(root_index_block_address + root_index_current_page, &child_index_block_address,
								address_size, root_index_current_byte))
				{
					return TEFS_ERR_READ;
				}
			}
			else
			{
				child_index_block_address = root_index_block_address;
			}

			uint16_t child_index_current_page;
			
			for (child_index_current_page = 0;
				 child_index_current_page < block_size && !past_end;
				 child_index_current_page++)
			{
				uint16_t child_index_current_byte;

				for (child_index_current_byte = 0;
					 child_index_current_byte < page_size && !past_end;
					 child_index_current_byte += address_size)
				{
					uint32_t data_block_address = 0;

					if (device_read(child_index_block_address + child_index_current_page,
									&data_block_address, address_size,
									child_index_current_byte))
					{
						return TEFS_ERR_READ;
					}

					/* Release data block. */
					if ((response = tefs_release_device_block(data_block_address)))
					{
						return response;
					}

					if (root_index_current_page == page_in_root_index && root_index_current_byte == byte_in_root_index_page &&
						child_index_current_page == page_in_child_index && child_index_current_byte == byte_in_child_index_page)
					{
						past_end = 1;
					}
				}
			}

			/* Release child index block. */
			if ((response = tefs_release_device_block(child_index_block_address)))
			{
				return response;
			}
		}
	}

	if (eof_page >= MULT_BY_POW_2_EXP(block_size, page_size_exponent - address_size_exponent))
	{
		/* Release root index block. */
		if ((response = tefs_release_device_block(root_index_block_address)))
		{
			return response;
		}
	}

	/* Change file status to deleted. */
	uint8_t status = TEFS_DELETED;

	if ((response = tefs_write(&metadata, directory_page, &status, 1, directory_byte)))
	{
		return response;
	}

	if (device_flush())
	{
		return TEFS_ERR_WRITE;
	}

	return TEFS_ERR_OK;
}

int8_t
tefs_write(
	file_t 		*file,
	uint32_t	file_page_address,
	void	 	*data,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
)
{
	uint8_t is_new_page = 0;

	if (file_page_address == file->eof_page)
	{
		if (byte_offset > file->eof_byte)
		{
			return TEFS_ERR_WRITE_PAST_END;
		}
		else if (byte_offset + number_of_bytes > file->eof_byte)
		{
			if (file->eof_byte == 0 || sd_spi_current_buffered_block() == file->data_block_address +
																		  MOD_BY_POW_2(file_page_address, block_size))
			{
				is_new_page = 1;
			}

			file->eof_byte = byte_offset + number_of_bytes;
		}

		file->is_file_size_consistent = 0;

		if (file->eof_byte == page_size)
		{
			file->eof_byte = 0;
			file->eof_page++;

			if (file->eof_page == MULT_BY_POW_2_EXP(block_size, page_size_exponent - address_size_exponent + block_size_exponent))
			{
				int8_t response;

				/* Write first child index block address to root index block. */
				if ((response = tefs_reserve_device_block(&(file->root_index_block_address))))
				{
					return response;
				}

				if (device_write(file->root_index_block_address, &(file->child_index_block_address), address_size, 0))
				{
					return TEFS_ERR_WRITE;
				}

				if (file->directory_page == 0xFFFFFFFF)
				{
					if (device_write(0, &(file->root_index_block_address), address_size, file->directory_byte + max_file_name_size + TEFS_DIR_STATUS_SIZE + TEFS_DIR_EOF_PAGE_SIZE + TEFS_DIR_EOF_BYTE_SIZE))
					{
						return TEFS_ERR_WRITE;
					}
				}
				else
				{
					if ((response = tefs_write(&metadata, file->directory_page, &(file->root_index_block_address), address_size, file->directory_byte + max_file_name_size + TEFS_DIR_STATUS_SIZE + TEFS_DIR_EOF_PAGE_SIZE + TEFS_DIR_EOF_BYTE_SIZE)))
					{
						return response;
					}
				}
			}
		}
	}
	else if (file_page_address > file->eof_page)
	{
		return TEFS_ERR_WRITE_PAST_END;
	}

	/* Check if page address is in the same block as the current data block. 
	   If it is, write the data. Otherwise, get the new data block address
	   from the root index block and then write to the page in that new data block. */
	if (file_page_address == file->current_page_number ||
		DIV_BY_POW_2_EXP(file_page_address, block_size_exponent) ==
		file->data_block_number)
	{
		sd_spi_dirty_write = is_new_page;

		if (device_write(file->data_block_address +
						 MOD_BY_POW_2(file_page_address, block_size), data,
						 number_of_bytes, byte_offset))
		{
			return TEFS_ERR_WRITE;
		}

		sd_spi_dirty_write = 0;
	}
	else
	{
		/* Check if the page is in the same child index block. If not, get the
		   address from the root index block or create a new child index block if it
		   does not exist. */
		uint16_t child_block_number = (uint16_t) DIV_BY_POW_2_EXP(file_page_address, block_size_exponent + addresses_per_block_exponent);

		if (DIV_BY_POW_2_EXP(file->data_block_number, addresses_per_block_exponent) != child_block_number)
		{ 
			uint16_t page_in_root_index;
			uint16_t byte_in_root_index_page;
			tefs_map_page_to_root_index_address(file_page_address, &page_in_root_index, &byte_in_root_index_page);

			/* Make sure that the file has not reached its max capacity. */
			if (page_in_root_index >= block_size)
			{
				return TEFS_ERR_FILE_FULL;
			}

			file->child_index_block_address = 0;

			if (file->is_file_size_consistent)
			{
				if (device_read(file->root_index_block_address + page_in_root_index,
								&(file->child_index_block_address), address_size,
								byte_in_root_index_page))
				{
					return TEFS_ERR_READ;
				}
			}
			else
			{
				int8_t response;
				if ((response = tefs_reserve_device_block(&(file->child_index_block_address))))
				{
					return response;
				}

				if (byte_in_root_index_page == 0)
				{
					sd_spi_dirty_write = 1;
				}

				if (device_write(file->root_index_block_address + page_in_root_index,
								 &(file->child_index_block_address),
								 address_size, byte_in_root_index_page))
				{
					return TEFS_ERR_WRITE;
				}

				sd_spi_dirty_write = 0;
			}
		}

		/* Get the data block from the child index block. */
		uint16_t page_in_child_index;
		uint16_t byte_in_child_index_page;
		tefs_map_page_to_child_index_address(file_page_address, &page_in_child_index, &byte_in_child_index_page);

		file->data_block_address = 0;

		if (file->is_file_size_consistent)
		{
			if (device_read(file->child_index_block_address + page_in_child_index,
							&(file->data_block_address), address_size,
							byte_in_child_index_page))
			{
				return TEFS_ERR_READ;
			}
		}
		else
		{
			int8_t response;
			if ((response = tefs_reserve_device_block(&(file->data_block_address))))
			{
				return response;
			}

			if (byte_in_child_index_page == 0)
			{
				sd_spi_dirty_write = 1;
			}

			if (device_write(file->child_index_block_address + page_in_child_index,
							 &(file->data_block_address), address_size,
							 byte_in_child_index_page))
			{
				return TEFS_ERR_WRITE;
			}

			sd_spi_dirty_write = 0;
		}

		sd_spi_dirty_write = is_new_page;

		if (device_write(file->data_block_address, data, number_of_bytes,
						 byte_offset))
		{
			return TEFS_ERR_WRITE;
		}

		sd_spi_dirty_write = 0;
		file->data_block_number = DIV_BY_POW_2_EXP(file_page_address, block_size_exponent);
	}

#if defined(UPDATE_FS_PAGE_CONSISTENCY)
	/* Update file size. */
	if (!file->is_file_size_consistent && file_page_address != file->current_page_number)
	{
		int8_t err;
		if (err = tefs_update_file_size(file))
		{
			return err;
		}

		file->is_file_size_consistent = 1;
	}
#elif defined(UPDATE_FS_RECORD_CONSISTENCY)
	if (!file->is_file_size_consistent)
	{
		int8_t err;
		if (err = tefs_update_file_size(file))
		{
			return err;
		}

		file->is_file_size_consistent = 1;
	}
#endif

	file->current_page_number = file_page_address;

	return TEFS_ERR_OK;
}

int8_t
tefs_flush(
	file_t *file
)
{
	if (device_flush())
	{
		return TEFS_ERR_WRITE;
	}

	/* Update file size. */
	if (!file->is_file_size_consistent)
	{
		int8_t err;
		if ((err = tefs_update_file_size(file)))
		{
			return err;
		}
	}

	return TEFS_ERR_OK;
}

int8_t
tefs_read(
	file_t 		*file,
	uint32_t 	file_page_address,
	void		*buffer,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
)
{
#if defined(UPDATE_FS_PAGE_CONSISTENCY)
	/* Update file size if necessary. */
	if (!file->is_file_size_consistent && file_page_address != file->current_page_number)
	{
		int8_t err;
		if ((err = tefs_update_file_size(file)))
		{
			return err;
		}
	}
#endif

	if (file_page_address == file->eof_page)
	{
		if (byte_offset > file->eof_byte)
		{
			return TEFS_ERR_EOF;
		}
		else if (byte_offset + number_of_bytes > file->eof_byte)
		{
			return TEFS_ERR_EOF;
		}
	}
	else if (file_page_address > file->eof_page)
	{
		return TEFS_ERR_EOF;
	}

	/* Check if page address is in the same block as the current data block. 
	   If it is, read the data. Otherwise, get the new data block address
	   from the root index block and then read to the page in that new data block. */
	if (file_page_address == file->current_page_number ||
		DIV_BY_POW_2_EXP(file_page_address, block_size_exponent) == file->data_block_number)
	{
		if (device_read(file->data_block_address + MOD_BY_POW_2(file_page_address, block_size),
						buffer, number_of_bytes, byte_offset))
		{
			return TEFS_ERR_READ;
		}
	}
	else
	{
		/* Check if the page is in the same child index block. If not, get the
		   address from the root index block or throw an error if it does not
		   exist. */
		uint16_t child_block_number = (uint16_t) DIV_BY_POW_2_EXP(file_page_address, block_size_exponent + addresses_per_block_exponent);

		if (DIV_BY_POW_2_EXP(file->data_block_number, addresses_per_block_exponent) != child_block_number)
		{
			uint16_t page_in_root_index;
			uint16_t byte_in_root_index_page;
			tefs_map_page_to_root_index_address(file_page_address, &page_in_root_index, &byte_in_root_index_page);

			/* Make sure that the file has not reached its max capacity. */
			if (page_in_root_index >= block_size)
			{
				return TEFS_ERR_FILE_FULL;
			}

			file->child_index_block_address = 0;

			if (device_read(file->root_index_block_address + page_in_root_index,
							&(file->child_index_block_address), address_size,
							byte_in_root_index_page))
			{
				return TEFS_ERR_READ;
			}
		}

		/* Get the data block from the child index block. */
		uint16_t page_in_child_index;
		uint16_t byte_in_child_index_page;
		tefs_map_page_to_child_index_address(file_page_address, &page_in_child_index, &byte_in_child_index_page);

		file->data_block_address = 0;

		if (device_read(file->child_index_block_address + page_in_child_index,
						&(file->data_block_address), address_size,
						byte_in_child_index_page))
		{
			return TEFS_ERR_READ;
		}

		if (device_read(file->data_block_address, buffer, number_of_bytes,
						byte_offset))
		{
			return TEFS_ERR_READ;
		}

		file->data_block_number = DIV_BY_POW_2_EXP(file_page_address, block_size_exponent);
	}

	file->current_page_number = file_page_address;

	return TEFS_ERR_OK;
}

int8_t
tefs_release_block(
	file_t 		*file,
	uint32_t 	file_block_address
)
{
	uint16_t child_block_number = (uint16_t) DIV_BY_POW_2_EXP(file_block_address, addresses_per_block_exponent);
	uint16_t page_in_root_index = DIV_BY_POW_2_EXP(child_block_number, page_size_exponent - address_size_exponent);
	uint16_t byte_in_root_index_page = (uint16_t) (MULT_BY_POW_2_EXP(child_block_number, address_size_exponent) & (addresses_per_block - 1));

	uint32_t block_in_child_index = DIV_BY_POW_2_EXP(file_block_address, block_size_exponent) & (addresses_per_block - 1);
	uint16_t page_in_child_index = (uint16_t) DIV_BY_POW_2_EXP(block_in_child_index, page_size_exponent - address_size_exponent);
	uint16_t byte_in_child_index_page = (uint16_t) (MULT_BY_POW_2_EXP(block_in_child_index, address_size_exponent) & (page_size - 1));

	if (file_block_address != file->data_block_number)
	{
		/* Check if the block is in the same child index block. If not, get the
		   address from the root index block. */
		if (DIV_BY_POW_2_EXP(file->data_block_number, addresses_per_block_exponent) !=
			child_block_number)
		{
			if (device_read(file->root_index_block_address + page_in_root_index,
							&(file->child_index_block_address), address_size,
							byte_in_root_index_page))
			{
				return TEFS_ERR_READ;
			}

			if (file->child_index_block_address == TEFS_EMPTY ||
				file->child_index_block_address == TEFS_DELETED)
			{
				return TEFS_ERR_UNRELEASED_BLOCK;
			}
		}

		/* Get the data block from the child index block. */
		if (device_read(file->child_index_block_address + page_in_child_index,
						&(file->data_block_address), address_size,
						byte_in_child_index_page))
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

	/* Remove the address from the child index block. */
	uint8_t byte_buffer = TEFS_DELETED;

	if (device_write(file->child_index_block_address + page_in_child_index,
					 &byte_buffer, 1, byte_in_child_index_page))
	{
		return TEFS_ERR_WRITE;
	}

	byte_buffer = 0;
	uint8_t current_byte = 1;

	while (current_byte < address_size)
	{
		if (device_write(file->child_index_block_address + page_in_child_index,
						 &byte_buffer, 1,
						 byte_in_child_index_page + current_byte))
		{
			return TEFS_ERR_WRITE;
		}

		current_byte++;
	}

	uint8_t contains_blocks = 0;

	/* Scan child index block to see if it's tefs_empty. */
	for (page_in_child_index = 0;
		 page_in_child_index < block_size && !contains_blocks;
		 page_in_child_index++)
	{
		for (byte_in_child_index_page = 0;
			 byte_in_child_index_page < page_size && !contains_blocks;
			 byte_in_child_index_page++)
		{
			if (device_read(file->child_index_block_address + page_in_child_index,
							&byte_buffer, 1, byte_in_child_index_page))
			{
				return TEFS_ERR_READ;
			}

			if (byte_buffer != TEFS_DELETED && byte_buffer != TEFS_EMPTY)
			{
				contains_blocks = 1;
			}
		}
	}

	/* Remove child index block from root index block if it's tefs_empty. */
	if (!contains_blocks)
	{
		byte_buffer = TEFS_DELETED;

		if (device_write(file->root_index_block_address + page_in_root_index, &byte_buffer,
						 1, byte_in_root_index_page))
		{
			return TEFS_ERR_WRITE;
		}

		byte_buffer = 0;
		current_byte = 1;

		while (current_byte < address_size)
		{
			if (device_write(file->root_index_block_address + page_in_root_index,
							 &byte_buffer, 1, byte_in_root_index_page + current_byte))
			{
				return TEFS_ERR_WRITE;
			}

			current_byte++;
		}

		if ((response = tefs_release_device_block(file->child_index_block_address)))
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
tefs_find_file_directory_entry(
	char 		*file_name,
	uint32_t 	*dir_page_address,
	uint16_t 	*dir_byte_in_page,
	uint8_t 	file_operation
)
{
	uint32_t name_hash_value = hash_string(file_name);
	uint16_t small_name_hash = (uint16_t) name_hash_value;

	uint16_t deleted_hash_page = 0xFFFF;
	uint16_t deleted_hash_byte = 0;
	uint32_t deleted_dir_page_address = 0;
	uint16_t deleted_dir_byte_in_page = 0;

	*dir_page_address = 0;
	*dir_byte_in_page = 0;

	int8_t response;
	uint16_t current_page = 0;
	uint16_t current_byte = 0;
	while (1)
	{
		uint32_t entry_hash_value = 0;
		if ((response = tefs_read(&hash_entries, current_page, &entry_hash_value, hash_size, current_byte)) != TEFS_ERR_EOF && response != TEFS_ERR_OK)
		{
			return response;
		}

		if (response == TEFS_ERR_EOF)	/* Reached the end of the hash entries file. */
		{
			if (file_operation == 1)    /* Create file */
			{
				if (deleted_hash_page <= current_page)
				{
					current_page = deleted_hash_page;
					current_byte = deleted_hash_byte;

					*dir_page_address = deleted_dir_page_address;
					*dir_byte_in_page = deleted_dir_byte_in_page;
				}

				/* Write out hash value to hash entries file. */
				if ((response = tefs_write(&hash_entries, current_page,
										   (hash_size == 4) ? &name_hash_value : &small_name_hash, hash_size,
										   current_byte)))
				{
					return response;
				}

				return TEFS_NEW_FILE;
			}
			else if (response == TEFS_ERR_EOF)
			{
				return TEFS_ERR_FILE_NOT_FOUND;
			}
		}
		else if (entry_hash_value == name_hash_value)	/* Found file hash */
		{
			int8_t status;
			if ((response = tefs_read(&metadata, *dir_page_address, &status, 1, *dir_byte_in_page)))
			{
				return response;
			}

			if (status != TEFS_EMPTY || status != TEFS_DELETED)
			{
				/* Check if it is the actual file by comparing file names. */
				int16_t count = -1;
				char c = ' ';

				while (count == -1 || (c != '\0' && file_name[count] != '\0' && c == file_name[count] && count < max_file_name_size))
				{
					if ((response = tefs_read(&metadata, *dir_page_address, &c, 1, *dir_byte_in_page + TEFS_DIR_STATIC_DATA_SIZE + (uint16_t) (count + 1))))
					{
						return response;
					}

					count++;
				}

				if (c == file_name[count])
				{
					if (file_operation == 2)	/* Remove file */
					{
						entry_hash_value = 0;
						if ((response = tefs_write(&hash_entries, current_page, &entry_hash_value, hash_size, current_byte)))
						{
							return response;
						}
					}

					return TEFS_ERR_OK;
				}
			}
		}
		else if (file_operation == 1 && entry_hash_value == 0 && deleted_hash_page == 0xFFFF) /* Found a deleted entry. */
		{
			deleted_hash_page = current_page;
			deleted_hash_byte = current_byte;

			deleted_dir_page_address = *dir_page_address;
			deleted_dir_byte_in_page = *dir_byte_in_page;
		}

		if ((uint32_t)*dir_byte_in_page + metadata_size >= page_size)
		{
			(*dir_page_address)++;
			*dir_byte_in_page = 0;
		}
		else
		{
			*dir_byte_in_page += metadata_size;
		}

		current_byte += hash_size;

		if (current_byte >= page_size)
		{
			current_page++;
			current_byte = 0;
		}
	}
}

static int8_t
tefs_write_metadata(
	file_t		file,
	void		*data,
	uint16_t 	number_of_byte
)
{
	return TEFS_ERR_OK;
}

static int8_t
tefs_reserve_device_block(
	uint32_t *block_address
)
{
#if defined(USE_SD)
	if (is_block_pool_empty)
	{
		return TEFS_ERR_DEVICE_FULL;
	}

	uint8_t byte_buffer = 0;

	/* Read in byte. */
	if (device_read(DIV_BY_POW_2_EXP(DIV_BY_POW_2_EXP(state_section_bit, 3), page_size_exponent) + 1, &byte_buffer, 1, (uint16_t) MOD_BY_POW_2(DIV_BY_POW_2_EXP(state_section_bit, 3), page_size)))
	{
		return TEFS_ERR_READ;
	}

	/* Check if the block has already been reserved. */
	if (!(byte_buffer & POW_2_TO(7 - MOD_BY_POW_2(state_section_bit, 8))))
	{
		return TEFS_ERR_OK;
	}

	/* Toggle state bit in byte from 1 to 0. */
	byte_buffer &= (uint8_t) ~POW_2_TO(7 - MOD_BY_POW_2(state_section_bit, 8));

	/* Write out the byte */
	if (device_write(DIV_BY_POW_2_EXP(DIV_BY_POW_2_EXP(state_section_bit, 3), page_size_exponent) + 1, &byte_buffer, 1, (uint16_t) MOD_BY_POW_2(DIV_BY_POW_2_EXP(state_section_bit, 3), page_size)))
	{
		return TEFS_ERR_WRITE;
	}

	*block_address = MULT_BY_POW_2_EXP(state_section_bit, block_size_exponent) + (1 + state_section_size);
	state_section_bit++;

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
#elif defined(USE_DATAFLASH) && defined(USE_FTL)
	/* Reserve page on FTL. */

	if (block_size == 1)
	{
		flare_logical_page_t p;
		flare_GetPage(&ftl, &p);
	}
	else if (block_size == 2)
	{

	}
#endif

	return TEFS_ERR_OK;
}

static int8_t
tefs_release_device_block(
	uint32_t block_address
)
{
#if defined(USE_SD)
	/* Start byte for the state section that is correlated to the block
	   address. */
	uint32_t state_bit = DIV_BY_POW_2_EXP(block_address - (1 + state_section_size), block_size_exponent);
	uint8_t byte_buffer = 0;

	/* Read in byte. */
	if (device_read(DIV_BY_POW_2_EXP(DIV_BY_POW_2_EXP(state_bit, 3), page_size_exponent) + 1, &byte_buffer, 1, (uint16_t) MOD_BY_POW_2(DIV_BY_POW_2_EXP(state_bit, 3), page_size)))
	{
		return TEFS_ERR_READ;
	}

	/* Check if the block has already been released. */
	if (byte_buffer & POW_2_TO(7 - MOD_BY_POW_2(state_bit, 8)))
	{
		return TEFS_ERR_OK;
	}

	/* Toggle state bit in byte from 0 to 1. */
	byte_buffer |= (uint8_t) POW_2_TO(7 - MOD_BY_POW_2(state_bit, 8));

	/* Write out the byte. */
	if (device_write(DIV_BY_POW_2_EXP(DIV_BY_POW_2_EXP(state_bit, 3), page_size_exponent) + 1, &byte_buffer, 1, (uint16_t) MOD_BY_POW_2(DIV_BY_POW_2_EXP(state_bit, 3), page_size)))
	{
		return TEFS_ERR_WRITE;
	}

	if (device_flush())
	{
		return TEFS_ERR_WRITE;
	}

	if (state_bit < state_section_bit)
	{
		state_section_bit = state_bit;
	}

	is_block_pool_empty = 0;
#elif defined(USE_DATAFLASH) && defined(USE_FTL)
	/* Release block from FTL. */
	if (block_size == 1)
	{
		flare_ReturnPage(&ftl, block_address);
	}
	else if (block_size == 8)
	{

	}
#endif

	return TEFS_ERR_OK;
}

static int8_t
tefs_erase_block(
	uint32_t block_address
)
{
	/* Erase pages in root index block. */
	uint32_t current_page;
	uint8_t flag = TEFS_EMPTY;

#if defined(USE_SD)
	if (sd_spi_write_continuous_start(block_address, block_size))
	{
		return TEFS_ERR_WRITE;
	}
#endif

	for (current_page = block_address;
		 current_page < block_size + block_address;
		 current_page++)
	{
		uint16_t current_byte;

		for (current_byte = 0;
			 current_byte < page_size;
			 current_byte++)
		{
#if defined(USE_SD)
			if (sd_spi_write_continuous(&flag, 1, current_byte))
			{
				return TEFS_ERR_WRITE;
			}
#else
			if (device_write(current_page, &flag, 1, current_byte))
			{
				return TEFS_ERR_WRITE;
			}
#endif
		}

#if defined(USE_SD)
		if (sd_spi_write_continuous_next())
		{
			return TEFS_ERR_WRITE;
		}
#endif
	}

#if defined(USE_SD)
	if (sd_spi_write_continuous_stop())
	{
		return TEFS_ERR_WRITE;
	}
#endif

	return TEFS_ERR_OK;
}

#if defined(USE_SD)
static int8_t
tefs_find_next_empty_block(
	uint32_t *state_section_bit
)
{
	uint32_t current_page = DIV_BY_POW_2_EXP(DIV_BY_POW_2_EXP(*state_section_bit, 3), page_size_exponent);
	uint16_t current_byte = (uint16_t) MOD_BY_POW_2(DIV_BY_POW_2_EXP(*state_section_bit, 3), page_size);
	uint8_t byte_buffer = 0;

	while (current_page < state_section_size)
	{
		while (current_byte < page_size)
		{
			if (sd_spi_read(current_page + 1, &byte_buffer, 1, current_byte))
			{
				return TEFS_ERR_READ;
			}

			if (byte_buffer)
			{
				uint8_t mask = 0x80;
				uint8_t count = 0;

				while (mask != 0 && !(byte_buffer & mask))
				{
				    mask >>= 1;
				    count++;
				}

				*state_section_bit = MULT_BY_POW_2_EXP(current_page, page_size_exponent + 3) +
									 MULT_BY_POW_2_EXP(current_byte, 3) + count;

				return TEFS_ERR_OK;
			}

			current_byte++;
		}

		current_page++;
		current_byte = 0;
	}

	is_block_pool_empty = 1;
	return TEFS_ERR_OK;
}
#endif

static void
tefs_map_page_to_root_index_address(
	uint32_t 	page,
	uint16_t 	*page_in_root_index,
	uint16_t 	*byte_in_root_index_page
)
{
	uint32_t child_block_number = DIV_BY_POW_2_EXP(DIV_BY_POW_2_EXP(page, block_size_exponent), addresses_per_block_exponent);

	*page_in_root_index = (uint16_t) DIV_BY_POW_2_EXP(child_block_number, page_size_exponent - address_size_exponent);

	*byte_in_root_index_page = (uint16_t) MOD_BY_POW_2(MULT_BY_POW_2_EXP(child_block_number, address_size_exponent), addresses_per_block);
}

static void
tefs_map_page_to_child_index_address(
	uint32_t 	page,
	uint16_t 	*page_in_child_index,
	uint16_t 	*byte_in_child_index_page
)
{
	uint32_t block_in_child_index = MOD_BY_POW_2(DIV_BY_POW_2_EXP(page, block_size_exponent), addresses_per_block);

	*page_in_child_index = (uint16_t) DIV_BY_POW_2_EXP(block_in_child_index, page_size_exponent - address_size_exponent);

	*byte_in_child_index_page = (uint16_t) MOD_BY_POW_2(MULT_BY_POW_2_EXP(block_in_child_index, address_size_exponent), page_size);
}

static int8_t
tefs_update_file_size(
	file_t *file
)
{
	int8_t response;

	if (file->directory_page == 0xFFFFFFFF)
	{
		if (device_write(0, &(file->eof_page), TEFS_DIR_EOF_PAGE_SIZE, file->directory_byte + TEFS_DIR_STATUS_SIZE))
		{
			return TEFS_ERR_WRITE;
		}

		if (device_write(0, &(file->eof_byte), TEFS_DIR_EOF_BYTE_SIZE, file->directory_byte + TEFS_DIR_STATUS_SIZE + TEFS_DIR_EOF_PAGE_SIZE))
		{
			return TEFS_ERR_WRITE;
		}
	}
	else
	{
		if ((response = tefs_write(&metadata, file->directory_page, &(file->eof_page), TEFS_DIR_EOF_PAGE_SIZE, file->directory_byte + TEFS_DIR_STATUS_SIZE)))
		{
			return response;
		}

		if ((response = tefs_write(&metadata, file->directory_page, &(file->eof_byte), TEFS_DIR_EOF_BYTE_SIZE, file->directory_byte + TEFS_DIR_STATUS_SIZE + TEFS_DIR_EOF_PAGE_SIZE)))
		{
			return response;
		}
	}

	file->is_file_size_consistent = 1;
	return TEFS_ERR_OK;
}

static uint8_t
tefs_power_of_two_exponent(
	uint32_t number
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
	uint16_t 	current_byte 	= 0;
	uint8_t		buffer 			= 0;

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

	/* Read the address size. */
	if (device_read(0, &address_size_exponent, 1, current_byte))
	{
		return TEFS_ERR_READ;
	}

	current_byte += 1;

	/* Read the size of a hash. */
	if (device_read(0, &hash_size, 1, current_byte))
	{
		return TEFS_ERR_READ;
	}

	current_byte += 1;

	/* Read the size of a metadata record. */
	if (device_read(0, &metadata_size, 2, current_byte))
	{
		return TEFS_ERR_WRITE;
	}

	current_byte += 2;

	/* Write the max size for a file name. */
	if (device_read(0, &max_file_name_size, 2, current_byte))
	{
		return TEFS_ERR_WRITE;
	}

	current_byte += 2;

#if defined(USE_SD)
	/* Read the state section size. */
	if (device_read(0, &state_section_size, 4, current_byte))
	{
		return TEFS_ERR_READ;
	}

	current_byte += 4;
#endif

	block_size 		= (uint16_t) POW_2_TO(block_size_exponent);
	page_size 		= (uint16_t) POW_2_TO(page_size_exponent);
	address_size 	= (uint8_t) POW_2_TO(address_size_exponent);

	addresses_per_block = DIV_BY_POW_2_EXP(MULT_BY_POW_2_EXP(page_size, block_size_exponent), address_size_exponent);
	addresses_per_block_exponent = tefs_power_of_two_exponent(addresses_per_block);

	int8_t response;
	int8_t i;
	file_t *temp_file = &hash_entries;
	for (i = 0; i < 2; i++)
	{
		/* Set file size to 0 for hash file directory entry. */
		if (device_read(0, &(temp_file->eof_page), TEFS_DIR_EOF_PAGE_SIZE, current_byte))
		{
			return TEFS_ERR_READ;
		}

		current_byte += TEFS_DIR_EOF_PAGE_SIZE;

		if (device_read(0, &(temp_file->eof_byte), TEFS_DIR_EOF_BYTE_SIZE, current_byte))
		{
			return TEFS_ERR_READ;
		}

		current_byte += TEFS_DIR_EOF_BYTE_SIZE;

		/* Read root index block address from the information page. */
		if (device_read(0, &(temp_file->root_index_block_address), address_size, current_byte))
		{
			return TEFS_ERR_WRITE;
		}

		current_byte += 4;

		if (temp_file->eof_page >= MULT_BY_POW_2_EXP(block_size, page_size_exponent - address_size_exponent))
		{
			/* Read the first child index block address. */
			if (device_read(temp_file->root_index_block_address, &(temp_file->child_index_block_address), address_size, 0))
			{
				return TEFS_ERR_READ;
			}

			/* Read the first data block address. */
			if (device_read(temp_file->child_index_block_address, &(temp_file->data_block_address), address_size, 0))
			{
				return TEFS_ERR_READ;
			}
		}
		else
		{
			temp_file->child_index_block_address = temp_file->root_index_block_address;

			/* Read the first data block address. */
			if (device_read(temp_file->child_index_block_address, &(temp_file->data_block_address), address_size, 0))
			{
				return TEFS_ERR_READ;
			}
		}

		temp_file->directory_page 			= 0xFFFFFFFF;
		temp_file->directory_byte 			= current_byte;
		temp_file->current_page_number 		= 0;
		temp_file->data_block_number 		= 0;
		temp_file->is_file_size_consistent 	= 1;

		temp_file = &metadata;
	}

#if defined(USE_SD)
	/* Get first byte in state for a free block. */
	state_section_bit = 0;

	if ((response = tefs_find_next_empty_block(&state_section_bit)))
	{
		return response;
	}
#endif

// #if DEBUG == 1
// #if defined(USE_SD)
// 	printf("State section bit: %u\n", state_section_bit);
// 	printf("State section size: %u\n", state_section_size);
// #endif
// 	printf("Directory size: %u\n", directory_size);
// 	printf("Number of pages: %u\n", number_of_pages);
// 	printf("Page size: %u\n", page_size);
// 	printf("Page size exponent: %u\n", page_size_exponent);
// 	printf("Addresses per block: %u\n", addresses_per_block);
// 	printf("Addresses per block exponent: %u\n", addresses_per_block_exponent);
// 	printf("Block size: %u\n", block_size);
// 	printf("Block size exponent: %u\n", block_size_exponent);
// 	printf("Address size: %u\n", address_size);
// 	printf("Address size exponent: %u\n", address_size_exponent);
// #endif

	return TEFS_ERR_OK;
}

static uint32_t
hash_string(
	char *str
)
{
	uint32_t hash = 5381;
	int8_t c;

	while ((c = *str++))
	{
		hash = (uint32_t)((hash << 5) + hash) ^ c; /* hash * 33 ^ c */
	}

	if (hash == 0)
	{
		hash = 1;
	}

	if (hash_size == 4)
	{
		return hash;
	}
	else
	{
		return hash % 65521;
	}
}
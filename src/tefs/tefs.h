/******************************************************************************/
/**
@file		tefs.h
@author     Wade Penson
@date		July, 2015
@brief      TEFS (Tiny Embedded File System) header.
@details	TEFS is a file system designed specifically for embedded
			devices that have little RAM. It provides an interface to read
			and write to pages (the smallest physical unit of addressable
			memory); opening, closing, and removing a file; and formatting the
			storage device.

@copyright  Copyright 2015 Wade Penson

		    Licensed under the Apache License, Version 2.0 (the "License");
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

#ifndef TEFS_H_
#define TEFS_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include "string.h"
#include "../tefs_configuration.h"

/* TODO: Add repository that has dataflash code */
#if defined(USE_DATAFLASH) && !defined(USE_FTL)
#include "dataflash/dataflash_raw.h"
#elif defined(USE_DATAFLASH) && defined(USE_FTL)
#include "dataflash/ftl/ftl_api.h"
volatile flare_ftl_t ftl;
#endif

/* Note: SD_BUFFER must be defined in SD SPI. */
#include "sd_spi/src/sd_spi.h"

/* Variable to toggle reading before writing to a page.  */
extern uint8_t sd_spi_dirty_write;

#if defined(USE_DATAFLASH) && defined(USE_FTL)
#define device_write(page, data, length, offset) \
		flare_WriteBytes(&ftl, page + 16, data, offset, length, 1)
#define device_read(page, buffer, length, offset) \
		flare_ReadBytes(&ftl, page + 16, buffer, offset, length)
#define device_flush() 1==0//df_flush()
#elif defined(USE_SD)
#define device_write(page, data, length, offset) \
		sd_spi_write(page, data, length, offset)
#define device_read(page, buffer, length, offset) \
		sd_spi_read(page, buffer, length, offset)
#define device_flush() sd_spi_flush()
#endif

#define POW_2_TO(exponent) 						(((uint32_t) 1) << (exponent))
#define MULT_BY_POW_2_EXP(expression, exponent) ((expression) << (exponent))
#define DIV_BY_POW_2_EXP(expression, exponent) 	((expression) >> (exponent))
#define MOD_BY_POW_2(expression, constant) 		((expression) & ((constant) - 1))

// TODO: Finish implementing continuous support
//#define TEFS_CONTINUOUS_SUPPORT

#define TEFS_INFO_SECTION_SIZE				((uint8_t) 1)

#define TEFS_DIR_STATUS_SIZE				((uint8_t) 1)
#define TEFS_DIR_EOF_PAGE_SIZE				((uint8_t) 4)
#define TEFS_DIR_EOF_BYTE_SIZE				((uint8_t) 2)
#define TEFS_DIR_ROOT_INDEX_ADDRESS_SIZE	((uint8_t) 4)
#define TEFS_DIR_STATIC_DATA_SIZE			((uint8_t) 11)

/** Used in information section to verify if the storage device has been formatted. */
#define TEFS_CHECK_FLAG		0xFC

/** Indicates if an entry has not been previously allocated. */
#define TEFS_EMPTY		0x00

/** Indicates if an entry had been previously allocated but has been
	deleted since. */
#define TEFS_DELETED	0x01

/** Indicates if an entry is allocated. */
#define TEFS_IN_USE 	0x02

/**
@defgroup tefs_err_codes	Error codes for TEFS.
@{
*/
#define TEFS_ERR_OK					0
#define TEFS_ERR_READ				1
#define TEFS_ERR_WRITE				2
#define TEFS_ERR_ERASE				3
#define TEFS_ERR_DEVICE_FULL		4
#define TEFS_ERR_FILE_FULL			5
#define TEFS_ERR_FILE_NOT_FOUND		6
#define TEFS_ERR_UNRELEASED_BLOCK	7
#define TEFS_ERR_NOT_FORMATTED		8
#define TEFS_ERR_WRITE_PAST_END		9
#define TEFS_ERR_EOF				10
#define TEFS_ERR_FILE_NAME_TOO_LONG	11
/** @} End of group tefs_err_codes */

/* Return code used internally that indicates if a new file has been created. */
#define TEFS_NEW_FILE	    		12

/** File structure. */
typedef struct file
{
	/** Address for the root index block. */
	uint32_t	root_index_block_address;
	/** Address for the child index block that was previously read from or written to. */
	uint32_t	child_index_block_address;
	/** Address for the current data block that was previously read from or written to. */
	uint32_t 	data_block_address;
	/** The block in the file that was previously read from or written to. */
	uint32_t	data_block_number;
	/** The page in the file that was previously read from or written to. */
	uint32_t 	current_page_number;
	/** The page in the metadata file where the metadata is stored for the file. */
	uint32_t	directory_page;
	/** The byte in the page in the metadata file where the metadata is stored for the file. */
	uint16_t	directory_byte;
	/** The last page of the file. (For keeping track of the file size) */
	uint32_t	eof_page;
	/** The last byte in the last page of the file. (For keeping track of the file size) */
	uint16_t	eof_byte;
	/** Keeps track if the file size has been written out to the directory entry after more data is written. */
	uint8_t 	is_file_size_consistent;
} file_t;

/**
@brief		Formats the storage device with TEFS.

@param		number_of_pages		The number of physical pages that the storage device
								has.
@param		physical_page_size	The size of the pages in bytes. The page size 
								must be a power of two and it can range from
								2^0 (1) to 2^15 (32768).
@param		block_size			The size of a block in terms of pages (not bytes).
								The block size must be a power of two and it can
								range from 2^5 (32) to 2^15 (32768).
@param		hash_size			The size of a hash in bytes for the directory entries.
 								The size can be either 2 or 4 bytes.
@param		metadata_size		The size of each metadata entry associated with a file.
 								This includes the size of the file name.
@param		max_file_name_size	This is the max size of a file name for each file
 								and it is fixed. It must be less than
 								(metadata entry size - 10) bytes.
@param		erase_before_format	A 1 if the whole device is to be erased and a 0
								to do a quick formatting without pre-erasing the
								card.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_format_device(
	uint32_t 	number_of_pages,
	uint16_t 	physical_page_size,
	uint16_t 	block_size,
	uint8_t		hash_size,
	uint16_t	metadata_size,
	uint16_t 	max_file_name_size,
	uint8_t		erase_before_format
);

/**
@brief		Opens an existing file given the file name. If the file does
			not exist, a new file with the given name will be created.

@param		file		A file_t structure.
@param		file_name	File name

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_open(
	file_t 		*file,
	char 		*file_name
);

/**
@brief		Checks if a file exists given the file name.

@param		file_name	File name

@return		If the file exists, 1 is returned. If the file does not exist, 0 is
			returned.
*/
int8_t
tefs_exists(
	char *file_name
);

/**
@brief		Closes an already opened file.
@details	This flushes the buffer out to the card.

@param		file	 A file_t structure.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_close(
	file_t *file
);

/**
@brief		Deletes the file with the given file name.

@param		file_name	File name

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_remove(
	char *file_name
);

/**
@brief		Releases a block from the file.
@details	The given block address must be the starting address of a
 			block (or the address of the first page in the block).

@param		file				A file_t structure.
@param		file_block_address	The address of the block in the file.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_release_block(
	file_t 		*file,
	uint32_t 	file_block_address
);

/**
@brief		Writes data to a page in the file.
@details	If the page does not already exist at the end of a file,
 			a new data block will be allocated. The data will only be
 			written out to disk when a new page is accessed or
 			tefs_flush() is called.

@param		file				A file_t structure.
@param		file_page_address	The address of the logical page in the file.
@param[in]	data				An array of data / an address to the data in
								memory.
@param		number_of_bytes		The size of the data in bytes.
@param		byte_offset			The byte offset of where to start writing in the
								page.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_write(
	file_t 		*file,
	uint32_t	file_page_address,
	void	 	*data,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
);

// TODO: Finish implementing continuous writing.
#if defined(TEFS_CONTINUOUS_SUPPORT)
/**
@brief		Notifies the card to prepare for sequential writing starting at the
			specified page address in the file.
@details	A sequential write must be stopped by calling
			tefs_write_continuous_stop(). To write from pages,
			tefs_write_continuous() in conjunction with tefs_write_next().

@param		file						A file_t structure.
@param		start_file_page_address		The address of the first page in the
										sequence.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_write_continuous_start(
	file_t 		*file,
	uint32_t 	start_file_page_address
);

/**
@brief		Writes data to the current page in the sequence.
@details	The data will be stored in the buffer which has been cleared with
			zeros (hence, current data in the page on the card is not
			pre-buffered). The data will be only written out to the card
			when tefs_write_continuous_next() or tefs_flush() is explicitly
			called.

@param		file				A file_t structure.
@param[in]	data				An array of data / an address to the data in
								memory.
@param		number_of_bytes		The size of the data in bytes.
@param		byte_offset			The byte offset of where to start writing in the
								page.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_write_continuous(
	file_t 		*file,
	void	 	*data,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
);

/**
@brief		Writes out the data in the buffer to the card for continuous writing
			and advancing to the next page in the sequence.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_write_continuous_next(
	file_t *file
);

/**
@brief		Notifies the card to stop sequential writing and flushes the buffer
			to the card.
@details	This may take some time since it waits for the card to complete the
			write.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_write_continuous_stop(
	file_t *file
);
#endif

/**
@brief		Flushes the data in the buffer out to the device.
@details	This will only flush the data if it has not already been flushed.

@param		file	A file_t structure.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_flush(
	file_t *file
);

/**
@brief		Reads data from a page in the file.
@details	If the location being read from is past the end of the file,
 			an error will be returned.

@param		file				A file_t structure.
@param		file_page_address	The address of the logical page in the file.
@param[out]	data_buffer			A location in memory to write the data to.
@param		number_of_bytes		The number of bytes to read.
@param		byte_offset			The byte offset of where to start reading in the
								page.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_read(
	file_t 		*file,
	uint32_t 	file_page_address,
	void		*buffer,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
);

// TODO: Finish implementing continuous reading.
#if defined(TEFS_CONTINUOUS_SUPPORT)
/**
@brief		Notifies the card to prepare for sequential reading starting at the
			specified page address in the file.
@details	A sequential read must be stopped by calling
			tefs_read_continuous_stop(). To read from pages, use
			tefs_read_continuous() in conjunction with tefs_read_next().

@param		file						A file_t structure.
@param		start_file_page_address		The address of the first page in the
										sequence.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_read_continuous_start(
	file_t 		*file,
	uint32_t 	start_file_page_address
);

/**
@brief		Reads in data from the current page in the sequence.
@details	To advance to the next page, tefs_read_continuous_next() must
			be explicitly called.

@param		file				A file_t structure.
@param[out]	data_buffer			A location in memory to write the data to.
@param		number_of_bytes		The number of bytes to read.
@param		byte_offset			The byte offset of where to start reading in the
								block.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_read_continuous(
	file_t 		*file,
	void 		*buffer,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
);

/**
@brief		Advances to the next block in the sequence.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_read_continuous_next(
	file_t *file
);

/**
@brief		Notifies the card to stop sequential reading.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_read_continuous_stop(
	file_t *file
);
#endif

#ifdef  __cplusplus
}
#endif

#endif /* TEFS_H_ */
/******************************************************************************/
/**
@file		tefs.h
@author     Wade H. Penson
@date		July, 2015
@brief      TEFS (Tiny Embedded File System) header.
@details	TEFS is a custom file system designed specifically for embedded
			devices that have little RAM. It provides an interface to read
			and write to pages (the smallest physical unit of addressable
			memory); opening, closing, and removing a file; and formatting the
			storage device.

			The first part of the file system includes the information section.
			This is located at block 0 and it contains info about the storage
			device. The fields are listed with the inclusive range for the
			location in bytes:
			
				- [0:3] Check flag. This is used to verify if the device has
						been formatted
 				- [4:7] Number of pages
				- [8:8] Physical page size in bytes
				- [9:9] Logical block size in pages
				- [10:10] File lookup key size in bytes
				- [11:11] Address size in bytes
				- [12:15] State section size in pages
				- [16:17] Directory size in pages

			The second section is the block state section. The size of this
			section is determined by the number of pages the device has. A bit
			that is 1 indicates that a block is in use and a bit of 0 indicates
			that a block is free. The location of a bit correlates to the
			location of a block on the device.

			The third section is the directory for the files. There are two
			possible sizes for the directory based on the key size. A key size
			of 1 allows 256 keys and a key size of 2 allows 65536 keys. When a
			file is created, the address to the file meta block is stored in
			the appropriate location based on the given key.

			Files consist of meta blocks, child meta blocks, and data blocks.
			A file contains only a single root meta block and it contains 
			addresses that point to child meta blocks. Child meta blocks
			contain the addresses that point to data blocks.

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

#ifndef TEFS_H_
#define TEFS_H_

#ifdef  __cplusplus
extern "C" {
#endif

/* SD_BUFFER must be defined in SD raw. */
#ifdef ARDUINO
#include "sd_spi.h"
#else /* ARDUINO */
#include "sd_spi_file.h"
#endif

#ifdef USE_DATAFLASH
#define device_write(page, data, length, offset) (write_bytes(page, data, length, offset, 1))
#define device_read(page, buffer, length, offset) (read_bytes(page, buffer, length, offset))
#define device_flush() (flush())
#else /* USE_DATAFLASH */
#define device_write(page, data, length, offset) (sd_raw_write(page, data, length, offset))
#define device_read(page, buffer, length, offset) (sd_raw_read(page, buffer, length, offset))
#define device_flush() (sd_raw_flush())
#endif

/** Used in info section to verify if card has been formatted. */
#define TEFS_CHECK_FLAG	0xFC

/** Indicates if a location not have a block pointer. */
#define TEFS_EMPTY			0

/** Indicates if a location had a previous block pointer but has been deleted. */
#define TEFS_DELETED		1

/** File structure. */
typedef struct tefs {
	/** File key / ID. */
	uint16_t 	file_id;
	/** Pointer to root meta block. */
	uint32_t	meta_block_address;
	/** Pointer to current child meta block. */
	uint32_t	child_meta_block_address;
	/** Pointer to current data block. */
	uint32_t 	data_block_address;
	/** The logical address of the file for the current data block. */
	uint32_t	data_block_number;
	/** The current page address for continuous reading or writing. */
	uint32_t 	current_page_number;
} tefs_t;

#define TEFS_ERR_OK				0
#define TEFS_ERR_READ				1
#define TEFS_ERR_WRITE				2
#define TEFS_ERR_ERASE				3
#define TEFS_ERR_DEVICE_FULL		4
#define TEFS_ERR_FILE_FULL			5
#define TEFS_ERR_UNRELEASED_BLOCK	6
#define TEFS_ERR_NOT_FORMATTED		7

/**
@brief		Formats the device with TEFS.

@param		number_of_pages		The number of physical pages that the device has.
@param		physical_page_size	The size of the pages in bytes. The page size 
								must be a power of two and it can range from
								2^0 (1) to 2^15 (32768).
@param		block_size			The size of a logical block or cluster in pages. 
								The block size must be a power of two and it can
								range from 2^5 (32) to 2^15 (32768).
@param		key_size			The key size can be 1 or 2 bytes. A key size of
								1 allows for 256 addresses and a key size of 2
								allows for 65536 addresses.
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
	uint8_t 	key_size,
	uint8_t		erase_before_format
);

/**
@brief		Opens an existing file given the lookup key / ID. If the file does
			not exist, a new file with the key will be created.

@param		file		An tefs_t structure.
@param		file_id		An key / ID within the range defined by key size.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_open(
	tefs_t 	*file,
	uint16_t 	file_id
);

/**
@brief		Checks if a file exists given the lookup key / ID.

@param		file_id		An key / ID within the range defined by key size.

@return		If the file exists, 1 is returned. If the file does not exist, 0 is
			returned.
*/
int8_t
tefs_exists(
	uint16_t 	file_id
);

/**
@brief		Closes an already open file.
@details	This only flushes the buffer out to the card.

@param		file	 An tefs_t structure.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_close(
	tefs_t 	*file
);

/**
@brief		Deletes the file with the given key / ID.

@param		file_id		An key / ID within the range defined by key size.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_remove(
	uint16_t 	file_id
);

/**
@brief		Releases a block from the file.
@details	The given block address must be a multiple of block size.

@param		file				An tefs_t structure.
@param		file_block_address	The address of the logical block in the file.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_release_block(
	tefs_t 	*file,
	uint32_t 	file_block_address
);

/**
@brief		Writes data to a page in the file.
@details	If the page does not already exist, a new data block will be created
			with the containing page. The data will only be written out to disk
			when a new page is accessed or tefs_flush() is called.

@param		file				An tefs_t structure.
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
	tefs_t 	*file,
	uint32_t	file_page_address,
	void	 	*data,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
);

/**
@brief		Notifies the card to prepare for sequential writing starting at the
			specified page address in the file.
@details	A sequential write must be stopped by calling
			tefs_write_continuous_stop(). To write from pages,
			tefs_write_continuous() in conjunction with tefs_write_next().

@param		file						An tefs_t structure.
@param		start_file_page_address		The address of the first page in the
										sequence.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_write_continuous_start(
	tefs_t 	*file,
	uint32_t 	start_file_page_address
);

/**
@brief		Writes data to the current page in the sequence.
@details	The data will be stored in the buffer which has been cleared with
			zeros (hence, current data in the page on the card is not
			pre-buffered). The data will be only written out to the card
			when tefs_write_continuous_next() or tefs_flush() is explicitly
			called.

@param		file				An tefs_t structure.
@param[in]	data				An array of data / an address to the data in
								memory.
@param		number_of_bytes		The size of the data in bytes.
@param		byte_offset			The byte offset of where to start writing in the
								page.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_write_continuous(
	tefs_t 	*file,
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
	tefs_t 	*file
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
	tefs_t 	*file
);

/**
@brief		Flushes the data in the buffer out to the device.
@details	This will only flush the data if it has not already been flushed.

@param		file	An tefs_t structure.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_flush(
	tefs_t 	*file
);

/**
@brief		Reads data from a page in the file.
@details	If the page does not already exist, an error will be returned.

@param		file				An tefs_t structure.
@param		file_page_address	The address of the logical page in the file.
@param[out]	data_buffer			A location in memory to write the data to.
@param		number_of_bytes		The number of bytes to read.
@param		byte_offset			The byte offset of where to start reading in the
								page.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_read(
	tefs_t 	*file,
	uint32_t 	file_page_address,
	void		*buffer,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
);

/**
@brief		Notifies the card to prepare for sequential reading starting at the
			specified page address in the file.
@details	A sequential read must be stopped by calling
			tefs_read_continuous_stop(). To read from pages, use
			tefs_read_continuous() in conjunction with tefs_read_next().

@param		file						An tefs_t structure.
@param		start_file_page_address		The address of the first page in the
										sequence.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_read_continuous_start(
	tefs_t 	*file,
	uint32_t 	start_file_page_address
);

/**
@brief		Reads in data from the current page in the sequence.
@details	To advance to the next page, tefs_read_continuous_next() must
			be explicitly called.

@param		file				An tefs_t structure.
@param[out]	data_buffer			A location in memory to write the data to.
@param		number_of_bytes		The number of bytes to read.
@param		byte_offset			The byte offset of where to start reading in the
								block.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_read_continuous(
	tefs_t 	*file,
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
	tefs_t 	*file
);

/**
@brief		Notifies the card to stop sequential reading.

@return		An error code as defined by one of the TEFS_ERR_* definitions.
*/
int8_t
tefs_read_continuous_stop(
	tefs_t 	*file
);

#ifdef  __cplusplus
}
#endif

#endif /* TEFS_H_ */
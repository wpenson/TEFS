/******************************************************************************/
/**
@file		sd_spi.h
@author     Wade H. Penson
@date		June, 2015
@brief      SD RAW library header.
@details	This library supports MMC, SD1, SD2, and SDHC/SDXC type cards.
			By defining SD_BUFFER, the library also implements a 512 byte buffer
			for reading and writing. The library currently uses the
			SPI library built into the Arduino core for SPI communication.

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

@todo 		Support for AVR and SAMX AVR without using the Arduino SPI library.
@todo 		Support for using Software SPI (bit banging).
@todo 		Use CMD6 during initialization to switch card to high speed mode if
			it supports it. This will be helpful for the due since the SPI
			speed can be up to 84MHz.
@todo 		Allow setting CS high when card is in busy state for operations
			instead of waiting. However, if an error is thrown for the
			operation, the host will get that error on the next command it
			sends.
@todo 		Send_status should be sent after all busy signals
			(look at ch 4.3.7).
@todo 		Send stop_transmission if there was an error during
			write continuous (and read continuous??).
@todo 		Get the number of well written blocks for sequential writing just in
			case if there was an error.
@todo 		Card reads take up to 100ms and writes take up to 500ms. Make
			timeouts reflect this.
@todo 		Reduce the number of error codes.
*/
/******************************************************************************/

#ifndef SD_SPI_H_
#define SD_SPI_H_

#ifdef  __cplusplus
extern "C" {
#endif

#define SD_SPI_BUFFER

#include <string.h>
#include <stdint.h>
#include "sd_spi_info.h"

/** State variables used by the SD raw library. */
typedef struct sd_spi_card {
	/** Digital pin for setting CS high or low. */
	uint8_t chip_select_pin: 			8;
	/** The speed of the SPI bus. Use 0 for 25KHz and 1 for 25MHz. */
	uint8_t spi_speed: 					3;
	/** Determines if the card is MMC, SD1, SD2, or SDHC/SDXC. */
	uint8_t card_type: 					3;
	/** Flag to see if CS has already been set to high or low. */
	uint8_t is_chip_select_high: 		1;
	/** True if currently reading continually and false otherwise. */
	uint8_t is_read_write_continuous:	1;
	/** True if currently writing continually and false otherwise. */
	uint8_t is_write_continuous:		1;
	/** If the card is being read or written to continually, this keeps track
		of the block being read or written to. */
	uint32_t continuous_block_address;

#ifdef SD_SPI_BUFFER
	/** Buffer for SD blocks. */
	uint8_t sd_spi_buffer[512];
	/** The address of the block currently being buffered. */
	uint32_t buffered_block_address;
	/** Keeps track of the consistency of the buffer for reading. */
	uint8_t is_buffer_current:			1;
	/** Keeps track if the buffer has been flushed or not. */
	uint8_t is_buffer_written:			1;
#endif /* SD_SPI_BUFFER */
} sd_spi_card_t;

/**
@defgroup sd_spi_timeouts	SD Timeouts
@brief                      Timeouts are used so that the SD does not freeze on
                            a task if a failure occurs.
@{
*/
#define SD_INIT_TIMEOUT		5000
#define SD_WRITE_TIMEOUT	5000
#define SD_READ_TIMEOUT		5000
#define SD_ERASE_TIMEOUT	500000

/** @} End of group sd_spi_timeouts */

/**
@defgroup sd_spi_card_types     SD/MMC Card Types
@brief                          Types of cards.
@{
*/
#define SD_CARD_TYPE_UNKNOWN	0
#define SD_CARD_TYPE_SD1		1
#define SD_CARD_TYPE_SD2		2
#define SD_CARD_TYPE_SDHC		3
#define SD_CARD_TYPE_MMC		4

/** @} End of group sd_spi_card_types */

/**
@defgroup sd_spi_error_codes	SD Error Codes
@brief							Error codes for the SD library.
@{
*/
#define SD_ERR_OK								0

#define SD_ERR_ALREADY_INITIALIZED				1
#define SD_ERR_NOT_INITIALIZED					2
#define SD_ERR_UNKNOWN_CARD_TYPE				3
#define SD_ERR_INIT_TIMEOUT						4
#define SD_ERR_COMMAND_TIMEOUT					5
#define SD_ERR_OUTSIDE_VOLTAGE_RANGE			6
#define SD_ERR_SEND_IF_COND_WRONG_TEST_PATTERN	7
#define SD_ERR_OCR_REGISTER						8
#define SD_ERR_SETTING_BLOCK_LENGTH				9

#define SD_ERR_ILLEGAL_COMMAND					10
#define SD_ERR_COMMUNICATION_CRC				11
#define SD_ERR_ILLEGAL_ADDRESS					12
#define SD_ERR_ILLEGAL_PARAMETER				13
#define SD_ERR_CARD_IS_LOCKED					14
#define SD_ERR_CARD_CONTROLLER					15
#define SD_ERR_CARD_ECC_FAILURE					16
#define SD_ERR_ARGUMENT_OUT_OF_RANGE			17
#define SD_ERR_GENERAL							18

#define SD_ERR_WRITE_FAILURE					19
#define SD_ERR_WRITE_TIMEOUT					20
#define SD_ERR_WRITE_OUTSIDE_OF_BLOCK			21
#define SD_ERR_WRITE_DATA_REJECTED				22
#define SD_ERR_WRITE_DATA_CRC_REJECTED			23
#define SD_ERR_WRITE_PRE_ERASE					24
#define SD_ERR_WRITE_PROTECTION_VIOLATION		25

#define SD_ERR_READ_FAILURE						26
#define SD_ERR_READ_TIMEOUT						27
#define SD_ERR_READ_OUTSIDE_OF_BLOCK			28

#define SD_ERR_READ_WRITE_CONTINUOUS			29

#define SD_ERR_ERASE_FAILURE					30
#define SD_ERR_ERASE_TIMEOUT					31
#define SD_ERR_ERASE_RESET						32
#define SD_ERR_WRITE_PROTECTION_ERASE_SKIP		33
#define SD_ERR_ERASE_PARAMETER					34
#define SD_ERR_ERASE_SEQUENCE					35

#define SD_ERR_READ_REGISTER					36

/** @} End of group sd_spi_error_codes */
/* R1 token responses */
#define SD_IN_IDLE_STATE						0x01
#define SD_ERASE_RESET							0x02
#define SD_ILLEGAL_COMMAND						0x04
#define SD_COMMUNICATION_CRC_ERR				0x08
#define SD_ERASE_SEQUENCE_ERR					0x10
#define SD_ADDRESS_ERR							0x20
#define SD_PARAMETER_ERR						0x40

/* R2 token responses */
#define SD_CARD_IS_LOCKED						0x01
#define SD_WP_ERASE_SKIP						0x02
#define SD_GENERAL_ERR							0x04
#define SD_CC_ERR								0x08
#define SD_CARD_ECC_FAILURE						0x10
#define SD_WP_VIOLATION							0x20
#define SD_ERASE_PARAM							0x40
#define SD_OUT_OF_RANGE							0x80

/* Reading and writing tokens */
#define SD_TOKEN_START_BLOCK					0xFE
#define SD_TOKEN_MULTIPLE_WRITE_START_BLOCK		0xFC
#define SD_TOKEN_MULTIPLE_WRITE_STOP_TRANSFER	0xFD
#define SD_TOKEN_DATA_ACCEPTED					0x05
#define SD_TOKEN_DATA_REJECTED_CRC				0x0B
#define SD_TOKEN_DATA_REJECTED_WRITE_ERR		0x0D

/**
@brief		Initializes the card with the given chip select pin.
@details	The card is put into a state where it can be sent read, write, and
			other commands.

@param		chip_select_pin		The digital pin connected to the CS on the card.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_init(
	uint8_t chip_select_pin
);

/**
@brief		Writes data to a block on the card.
@details	If buffering is enabled, the card will read the block on the card
			into the buffer. The passed in data will then be stored in
			the buffer. The data will be only written out to the card when a 
			different block is written to, read is called, write is called, 
			erase is called, or sd_spi_flush() is explicitly called.
			If buffering is not enabled, the card will write out the data and
			pad the rest of the block with zeros if number_of_bytes is less
			than 512.

@param		block_address		The address of the block on the card.
@param[in]	data				An array of data / an address to the data in
								memory.
@param		number_of_bytes		The size of the data in bytes.
@param		byte_offset			The byte offset of where to start writing in the
								block.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_write(
	uint32_t 	block_address,
	void	 	*data,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
);

/**
@brief		Writes a block of data to a block on the card.
@details	When buffering is enabled, this is faster than using sd_spi_write
			since it does not buffer in the block from the card.

@param		block_address	The address of the block on the card.
@param[in]	data			An array of data / an address to the data in
							memory.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_write_block(
	uint32_t 	block_address,
	void	 	*data
);

/**
@brief		Writes out the data in the buffer to the card if it has not been 
			flushed already. If writing continually, this will also advance to
			the next block in the sequence.
@details	This is to be used in conjunction with sd_spi_write or
			sd_spi_continuous_write.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_flush(
	void
);

/**
@brief		Notifies the card to prepare for sequential writing starting at the
			specified block address.
@details	A sequential write must be stopped by calling
			sd_spi_write_continuous_stop(). When buffering is enabled, use
			sd_spi_write_continuous() in conjunction with sd_spi_write_next().
			If buffering is disabled, use sd_spi_write_continuous() to write out
			the data and it will advance to the next block in the sequence 
			automatically.

@param		start_block_address		The address of the first block in the
									sequence.
@param		num_blocks_pre_erase	(Optional) The number of block to pre-erase
									to potentially speed up writing. It can be
									set to 0 if the number if the number of
									blocks is unknown or an estimate can be
									given since it does not have to be exact.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_write_continuous_start(
	uint32_t 	start_block_address,
	uint32_t 	num_blocks_pre_erase
);

/**
@brief		Writes data to the current block in the sequence on the card.
@details	If buffering is enabled, the data will be stored in the buffer which
			has been cleared with zeros (current data in the block on the card
			is not pre-buffered). The data will be only written out to the card
			when sd_spi_write_continuous_next() or sd_spi_flush() is explicitly
			called. If the buffering is not enabled, the card will write out the
			data and advance to the next block in the sequence. If the number of
			bytes given is less than 512, the rest of the block will be padded
			with zeros.

@param[in]	data				An array of data / an address to the data in
								memory.
@param		number_of_bytes		The size of the data in bytes.
@param		byte_offset			The byte offset of where to start writing in the
								block.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_write_continuous(
	void	 	*data,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
);

/**
@brief		Used only when buffering is enabled to write out the data in the
			buffer to the card for continuous writing and advancing to the
			next block in the sequence.
@details	If buffering is not enabled, only use sd_spi_write_continuous().

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_write_continuous_next(
	void
);

/**
@brief		Notifies the card to stop sequential writing and flushes the buffer
			to the card if buffering is enabled.
@details	This may take some time since it waits for the card to complete the
			write.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_write_continuous_stop(
	void
);

/**
@brief		Reads data from a block on the card.
@details	If buffering is enabled, this will write in the block to the buffer.
			Otherwise, the device will have to read in the data from the card
			on every call (it has to read a 512 byte block only the relevant
			data will be kept).

@param		block_address		The address of the block on the card.
@param[out]	data_buffer			A location in memory to write the data to.
@param		number_of_bytes		The number of bytes to read.
@param		byte_offset			The byte offset of where to start reading in the
								block.

@return 	An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_read(
	uint32_t 	block_address,
	void		*data_buffer,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
);

/**
@brief		Notifies the card to prepare for sequential reading starting at the
			specified block address.
@details	A sequential read must be stopped by calling
			sd_spi_read_continuous_stop(). When buffering is enabled, use
			sd_spi_read_continuous() in conjunction with sd_spi_read_next().
			If buffering is disabled, use sd_spi_read_block() to read in data
			and advance to the next block in the sequence.

@param		start_block_address		The address of the first block in the
									sequence.

@return 	An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_read_continuous_start(
	uint32_t start_block_address
);

/**
@brief		Reads in data from the current block in the sequence on the card.
@details	If buffering is enabled, the block will be stored in the buffer.
			To advance to the next block, sd_spi_read_continuous_next() must
			be explicitly called. If the buffering is not enabled, the card will
			read in the data and advance to the next block in the sequence.

@param[out]	data_buffer			A location in memory to write the data to.
@param		number_of_bytes		The number of bytes to read.
@param		byte_offset			The byte offset of where to start reading in the
								block.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_read_continuous(
	void 		*data_buffer,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
);

/**
@brief		Used only when buffering is enabled to advancing to the
			next block in the sequence.
@details	If buffering is not enabled, only use sd_spi_read_continuous().

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_read_continuous_next(
	void
);

/**
@brief		Notifies the card to stop sequential reading.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_read_continuous_stop(
	void
);

/**
@brief		Erases all the blocks on the card.
@details	All of the bits will be set with the default value of 0 or 1
			depending on the card.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_erase_all(
	void
);

/**
@brief		Erases the given sequence of blocks on the card.
@details	All of the bits will be set with the default value of 0 or 1
			depending on the card.

@param 		start_block_address		The address of the first block in the
									sequence.
@param 		end_block_address		The address of the last block in the
									sequence.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_erase_blocks(
	uint32_t 	start_block_address,
	uint32_t 	end_block_address
);

/**
@brief		Gets the number of blocks the card has. Blocks are 512 bytes.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
uint32_t
sd_spi_card_size(
	void
);

/**
@brief		Reads the Card Identification (CID) register on the card.
@details	Information is stored in the sd_spi_cid_t structure. The details of
			the structure can be found in sd_spi_info.h.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_read_cid_register(
	sd_spi_cid_t *cid
);

/**
@brief		Reads the Card Specific Data (CSD) register on the card.
@details	Information is stored in the sd_spi_csd_t structure. The details of
			the structure can be found in sd_spi_info.h.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_read_csd_register(
	sd_spi_csd_t *csd
);

/**
@brief		Returns the first error code (if any) found in the R2 response on
			from the card.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
int8_t
sd_spi_card_status(
	void
);

#ifdef  __cplusplus
}
#endif

#endif /* SD_SPI_H_ */

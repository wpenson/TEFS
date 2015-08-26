/******************************************************************************/
/**
@file		sd_spi.cpp
@author     Wade H. Penson
@date		June, 2015
@brief      SD SPI library implementation.
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

#if defined(ARDUINO) && !defined(USE_HARDWARE_SPI)
#include <Arduino.h>
#include <SPI.h>
#endif

#include "sd_spi.h"
#include "sd_spi_commands.h"

/* An sd_spi_card_t structure for internal state. */
static sd_spi_card_t card;

/**
@brief		Clears the buffer and sets the values to 0.

@param[in]	card	An sd_spi_card_t which has been initialized.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
static void
sd_spi_clear_buffer(
	void
);

/**
@brief		Performs the direct write to the card.

@param		block_address		The address of the block on the card.
@param[in]	data				An array of data / an address to the data in
								memory.
@param		number_of_bytes		The size of the data in bytes.
@param		byte_offset			The byte offset of where to start writing in the
								block.

@return		An error code as defined by one of the SD_ERR_* definitions.	
*/
static int8_t
sd_spi_write_out_data(
	uint32_t	block_address,
	void	 	*data,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
);

/**
@brief		Performs the direct read from the card.

@param		block_address		The address of the block on the card.
@param[out]	data_buffer			A location in memory to write the data to.
@param		number_of_bytes		The number of bytes to read.
@param		byte_offset			The byte offset of where to start reading in the
								block.

@return		An error code as defined by one of the SD_ERR_* definitions.
*/
static int8_t
sd_spi_read_in_data(
	uint32_t 	block_address,
	void	 	*data_buffer,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
);

/**
@brief	Sends a command to the card.

@param	command		The command.
@param	argument	A four byte argument that depends on the command.

@return	The R1 response from the card.
*/
static uint8_t
sd_spi_send_command(
	uint8_t 	command,
	uint32_t 	argument
);

/**
@brief	Sends an application command to the card.

@param	command		The command.
@param	argument	A four byte argument that depends on the command.

@return	The R1 response from the card.
*/
static uint8_t
sd_spi_send_app_command(
	uint8_t 	command,
	uint32_t 	argument
);

/**
@brief	Waits for card to complete specific operations.

@param	max_time_to_wait	The number of ms when to timeout.

@return	A 1 on timeout and 0 otherwise.
*/
static int8_t
sd_spi_wait_if_busy(
	uint16_t max_time_to_wait
);

/**
@brief	Get the first flag in the R1 register from the card that indicates an
		error.

@param	r1_response	The byte from the R1 register.

@return	An error code as defined by one of the SD_ERR_* definitions.
*/
static int8_t
sd_spi_r1_error(
	uint8_t r1_response
);

/**
@brief	Get the first flag in the R2 register from the card that indicates an
		error.

@param	r2_response	The two bytes from the R2 register.

@return	An error code as defined by one of the SD_ERR_* definitions.
*/
static int8_t
sd_spi_r2_error(
	uint16_t r2_response
);

/**
@brief	Asserts the chip select pin for the card.
*/
static void
sd_spi_select_card(
	void
);

/**
@brief	De-asserts the chip select pin for the card.
*/
static void
sd_spi_unselect_card(
	void
);

/**
@brief	Sends a byte over the SPI bus.
*/
static void
sd_spi_spi_send(
	uint8_t b
);

/**
@brief	Receives a byte from the SPI bus.
*/
static uint8_t
sd_spi_spi_receive(
	void
);

int8_t
sd_spi_init(
	uint8_t chip_select_pin
)
{
	card.spi_speed = 0;
	card.card_type = SD_CARD_TYPE_UNKNOWN;
	card.chip_select_pin = chip_select_pin;
	card.is_chip_select_high = 1;
	card.is_read_write_continuous = 0;
	card.continuous_block_address = 0;

#ifdef SD_SPI_BUFFER
	card.buffered_block_address = 0;
	card.is_buffer_current = 0;
	card.is_buffer_written = 1;
#endif /* SD_SPI_BUFFER */

#ifdef USE_HARDWARE_SPI

#else /* USE_HARDWARE_SPI */
	/* Set CS high. */
  	pinMode(chip_select_pin, OUTPUT);
  	digitalWrite(chip_select_pin, HIGH);

  	SPI.begin();
	SPI.beginTransaction(SPISettings(250000, MSBFIRST, SPI_MODE0));

	/* Send at least 74 clock pulses to enter the native operating mode
	   (80 in this case). */
	uint8_t i;
  	for (i = 0; i < 10; i++)
  	{
  		sd_spi_spi_send(0xFF);
  	}

  	SPI.endTransaction();
#endif

  	/* Record start time to check for initialization timeout. */
	uint16_t init_start_time =  millis();

	/* Send CMD0 to put card in SPI mode. The card will respond with 0x01. */
	while (sd_spi_send_command(SD_CMD_GO_IDLE_STATE, 0) != SD_IN_IDLE_STATE)
	{
		if (((uint16_t) millis() - init_start_time) > SD_INIT_TIMEOUT)
		{
			sd_spi_unselect_card();

			return SD_ERR_INIT_TIMEOUT;
		}
  	}

  	/* Check SD card type. First send CMD8 with the argument for the voltage
  	   range of 2.7 - 3.6 and a test pattern AA. If the card doesn't support
  	   this command, then it is of type SD1 or MMC */
  	if ((sd_spi_send_command(SD_CMD_SEND_IF_COND, 0x1AA) & SD_ILLEGAL_COMMAND)
  		 == 0)
  	{
  		/* Discard first 2 bytes of R7. */
  		sd_spi_spi_receive();
  		sd_spi_spi_receive();

  		/* Check if voltage range is accepted. */
    	if ((sd_spi_spi_receive() & 0x01) == 0)
    	{
    		sd_spi_unselect_card();

			return SD_ERR_OUTSIDE_VOLTAGE_RANGE;
		}

		/* Check if the test pattern is the same. */
    	if (sd_spi_spi_receive() != 0xAA)
    	{
    		sd_spi_unselect_card();

      		return SD_ERR_SEND_IF_COND_WRONG_TEST_PATTERN;
    	}

    	card.card_type = SD_CARD_TYPE_SD2;
  	}

  	init_start_time = millis();

  	/* Initialize card. */
	if (card.card_type == SD_CARD_TYPE_SD2)
	{
		while (sd_spi_send_app_command(SD_ACMD_SEND_OP_COND, 0x40000000) != 0)
		{
			if (((uint16_t) millis() - init_start_time) > SD_INIT_TIMEOUT)
			{
				sd_spi_unselect_card();

				return SD_ERR_INIT_TIMEOUT;
			}
		}

		/* Check to see if card is of type SDHC (or SDXC) by reading OCR. */
		if (sd_spi_send_command(SD_CMD_READ_OCR, 0))
		{
			sd_spi_unselect_card();

			return SD_ERR_OCR_REGISTER;
    	}

		if ((sd_spi_spi_receive() & 0x40) != 0)
		{
			card.card_type = SD_CARD_TYPE_SDHC;
		}

		/* Discard rest of OCR. */
		sd_spi_spi_receive();
		sd_spi_spi_receive();
		sd_spi_spi_receive();
	}
	else
	{
		/* Send AMCD41 to try initializing card and if card doesn't support this
		   command, it is most likely of type MMC or an earlier version of SD. */
		while (sd_spi_send_app_command(SD_ACMD_SEND_OP_COND, 0) != 0)
		{
			if (((uint16_t) millis() - init_start_time) > 500)
			{
				/* If sending CMD1 times out, the card is of unknown type. */
				while (sd_spi_send_command(SD_CMD_SEND_OP_COND, 0) !=
					   SD_IN_IDLE_STATE)
				{
					if (((uint16_t) millis() - init_start_time) >
						SD_INIT_TIMEOUT + 500)
					{
						sd_spi_unselect_card();

						return SD_ERR_UNKNOWN_CARD_TYPE;
					}
			  	}

			  	card.card_type = SD_CARD_TYPE_MMC;
			  	break;
			}
		}

		if (card.card_type != SD_CARD_TYPE_MMC)
		{
			card.card_type = SD_CARD_TYPE_SD1;
		}
	}

	/* Set block size to 512 bytes. */
    if (sd_spi_send_command(SD_CMD_SET_BLOCKLEN, 512))
    {
    	sd_spi_unselect_card();

    	return SD_ERR_SETTING_BLOCK_LENGTH;
    }

	card.spi_speed = 1;
	sd_spi_unselect_card();

	return SD_ERR_OK;
}

int8_t
sd_spi_write(
	uint32_t 	block_address,
	void	 	*data,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
)
{
	/* Check to make sure that data is within page bounds. */
	if (number_of_bytes + byte_offset > 512)
	{
		return SD_ERR_WRITE_OUTSIDE_OF_BLOCK;
	}

#ifdef SD_SPI_BUFFER
	/* Write a whole block out if it is 512 bytes, otherwise read block into
	   buffer for partial writing. */
	if (number_of_bytes == 512 && !card.is_read_write_continuous)
	{
		int8_t response = sd_spi_write_block(block_address, data);
		sd_spi_unselect_card();

		return response;
	}
	
	if (!card.is_read_write_continuous && (card.buffered_block_address !=
		block_address || !card.is_buffer_current))
	{
		int8_t response;

		if ((response = sd_spi_flush()))
		{
			return response;
		}

		if ((response = sd_spi_read_in_data(block_address, card.sd_spi_buffer,
											512, 0)))
		{
			return response;
		}
	}

	card.is_buffer_written = 0;
	memcpy(card.sd_spi_buffer + byte_offset, data, number_of_bytes);

	return SD_ERR_OK;
#else /* SD_SPI_BUFFER */
	int8_t response = sd_spi_write_out_data(block_address, data,
										 	number_of_bytes, byte_offset);

	sd_spi_unselect_card();

	return response;
#endif
}

int8_t
sd_spi_write_block(
	uint32_t 	block_address,
	void	 	*data
)
{
	int8_t response;

#ifdef SD_SPI_BUFFER
	if ((response = sd_spi_flush()))
	{
		return response;
	}
#endif /* SD_SPI_BUFFER */

	response = sd_spi_write_out_data(block_address, data, 512, 0);

#ifdef SD_SPI_BUFFER
	if (block_address == card.continuous_block_address)
	{
		memcpy(card.sd_spi_buffer, data, 512);
	}

	card.is_buffer_written = 1;
	card.buffered_block_address = block_address;
#endif /* SD_SPI_BUFFER */

	sd_spi_unselect_card();

	return response;
}

int8_t
sd_spi_flush(
	void
)
{
#ifdef SD_SPI_BUFFER
	if (card.is_buffer_written)
	{
		return SD_ERR_OK;
	}

	int8_t response;

	if ((response = sd_spi_write_out_data(card.buffered_block_address,
										  card.sd_spi_buffer, 512, 0)))
	{
		return response;
	}

	if (!card.is_read_write_continuous)
	{
		card.is_buffer_current = 1;
	}
	
	card.is_buffer_written = 1;
	sd_spi_unselect_card();
#endif /* SD_SPI_BUFFER */

	return SD_ERR_OK;
}

int8_t
sd_spi_write_continuous_start(
	uint32_t start_block_address,
	uint32_t num_blocks_pre_erase
)
{
#ifdef SD_SPI_BUFFER
	int8_t response;

	if ((response = sd_spi_flush()))
	{
		return response;
	}
#endif /* SD_SPI_BUFFER */

	/* Keep track of block address for error checking and buffering. */
	card.continuous_block_address = start_block_address;

	/* SD cards 2GB or less address by bytes so multiply by 512 to address
	   by blocks. */
	if (card.card_type != SD_CARD_TYPE_SDHC)
	{
		start_block_address <<= 9;
	}

	/* Optionally pre-erase blocks for faster writing. */
	if (num_blocks_pre_erase != 0)
	{
		if (sd_spi_send_app_command(SD_ACMD_SET_WR_BLK_ERASE_COUNT,
									num_blocks_pre_erase))
		{
			sd_spi_unselect_card();

			return SD_ERR_WRITE_PRE_ERASE;
		}
	}

	/* Start multiple block write. */
	if (sd_spi_send_command(SD_CMD_WRITE_MULTIPLE_BLOCK, start_block_address))
	{
		sd_spi_unselect_card();

		return SD_ERR_WRITE_FAILURE;
	}

	card.is_read_write_continuous = 1;

#ifdef SD_SPI_BUFFER
	sd_spi_clear_buffer();
	card.buffered_block_address = card.continuous_block_address;
#endif /* SD_SPI_BUFFER */

	sd_spi_unselect_card();

	return SD_ERR_OK;
}

int8_t
sd_spi_write_continuous(
	void		*data,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
)
{
	return sd_spi_write(card.continuous_block_address, data, number_of_bytes,
						byte_offset);
}

int8_t
sd_spi_write_continuous_next(
	void
)
{
#ifdef SD_SPI_BUFFER
	int8_t response = sd_spi_flush();
	sd_spi_clear_buffer();

	return response;
#else /* SD_SPI_BUFFER */
	return SD_ERR_OK;
#endif
}

int8_t
sd_spi_write_continuous_stop(
	void
)
{
#ifdef SD_SPI_BUFFER
	int8_t response;

	/* Flush buffer if it hasn't been written. */
	if ((response = sd_spi_flush()))
	{
		return response;
	}
#endif /* SD_SPI_BUFFER */

	sd_spi_select_card();

	/* Wait for card to complete the write. */
	if (sd_spi_wait_if_busy(SD_WRITE_TIMEOUT))
	{
		sd_spi_unselect_card();

		return SD_ERR_WRITE_TIMEOUT;
	}

	/* Token is sent to signal card to stop multiple block writing. */
  	sd_spi_spi_send(SD_TOKEN_MULTIPLE_WRITE_STOP_TRANSFER);

	card.is_read_write_continuous = 0;

  	/* Wait for card to complete the write. */
	if (sd_spi_wait_if_busy(SD_WRITE_TIMEOUT))
	{
		sd_spi_unselect_card();

		return SD_ERR_WRITE_TIMEOUT;
	}

	return sd_spi_card_status();
}

int8_t
sd_spi_read(
	uint32_t 	block_address,
	void	 	*data_buffer,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
)
{
#ifdef SD_SPI_BUFFER
	int8_t response;

	if ((response = sd_spi_flush()))
	{
		return response;
	}

	/* Check to make sure that data is within page bounds. */
	if (number_of_bytes + byte_offset > 512)
	{
    	return SD_ERR_READ_OUTSIDE_OF_BLOCK;
  	}

	if (card.buffered_block_address != block_address ||
		!card.is_buffer_current)
	{
  		int8_t response;

		/* Read block into buffer. */
		if ((response = sd_spi_read_in_data(block_address, card.sd_spi_buffer,
											512, 0)))
		{
			return response;
		}
	}

	memcpy(data_buffer, card.sd_spi_buffer + byte_offset, number_of_bytes);

	return SD_ERR_OK;
#else /* SD_SPI_BUFFER */
	return sd_spi_read_in_data(block_address, data_buffer, number_of_bytes,
							   byte_offset);
#endif
}

int8_t
sd_spi_read_continuous_start(
	uint32_t start_block_address
)
{
#ifdef SD_SPI_BUFFER
	int8_t response;

	if ((response = sd_spi_flush()))
	{
		return response;
	}
#endif /* SD_SPI_BUFFER */

	card.continuous_block_address = start_block_address;

	/* SD cards 2GB or less address by bytes so multiply by 512 to address
	   by blocks. */
	if (card.card_type != SD_CARD_TYPE_SDHC)
	{
		start_block_address <<= 9;
	}

	/* Start multiple block reading. */
	if (sd_spi_send_command(SD_CMD_READ_MULTIPLE_BLOCK, start_block_address))
	{
		sd_spi_unselect_card();

		return SD_ERR_READ_FAILURE;
	}

	card.is_read_write_continuous = 1;

#ifdef SD_SPI_BUFFER
	if ((response = sd_spi_read_in_data(card.continuous_block_address,
										card.sd_spi_buffer, 512, 0)))
	{
		sd_spi_unselect_card();

		return response;
	}

	card.continuous_block_address--;
	card.buffered_block_address = card.continuous_block_address;
#endif /* SD_SPI_BUFFER */

	sd_spi_unselect_card();

	return SD_ERR_OK;
}

int8_t
sd_spi_read_continuous(
	void	 	*data_buffer,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
)
{
#ifdef SD_SPI_BUFFER
	return sd_spi_read(card.continuous_block_address, data_buffer,
					   number_of_bytes, byte_offset);
#else /* SD_SPI_BUFFER */
	return sd_spi_read_in_data(card.continuous_block_address, data_buffer,
							   number_of_bytes, byte_offset);
#endif
}

int8_t
sd_spi_read_continuous_next(
	void
)
{
#ifdef SD_SPI_BUFFER
	return sd_spi_read_in_data(card.continuous_block_address, card.sd_spi_buffer,
							   512, 0);
#else /* SD_SPI_BUFFER */
	return SD_ERR_OK;
#endif
}

int8_t
sd_spi_read_continuous_stop(
	void
)
{
	sd_spi_select_card();

	uint16_t timeout_start = millis();

	/* Since the stop command is not sent during a read, it must be sent when a
	   read token is received. */
	while (sd_spi_spi_receive() != SD_TOKEN_START_BLOCK)
	{
		if (((uint16_t) millis() - timeout_start) > SD_READ_TIMEOUT)
		{
			return sd_spi_card_status();
	    }
	}

	timeout_start = millis();

	/* Send command to stop continuous reading. */
	if ((sd_spi_send_command(SD_CMD_STOP_TRANSMISSION, 0) & 0x08) != 0)
	{
		while ((sd_spi_spi_receive() & 0x08) != 0)
		{
			if (((uint16_t) millis() - timeout_start) > SD_READ_TIMEOUT)
			{
				sd_spi_unselect_card();

				return SD_ERR_READ_FAILURE;
			}
		}
	}

	card.is_read_write_continuous = 0;
	sd_spi_unselect_card();

	return SD_ERR_OK;
}

int8_t
sd_spi_erase_all(
	void
)
{
	return sd_spi_erase_blocks(0, sd_spi_card_size());
}

int8_t
sd_spi_erase_blocks(
	uint32_t start_block_address,
	uint32_t end_block_address
)
{
#ifdef SD_SPI_BUFFER
	int8_t response;

	if ((response = sd_spi_flush()))
	{
		return response;
	}
	
	if (card.buffered_block_address > start_block_address &&
		card.buffered_block_address < end_block_address)
	{
		sd_spi_clear_buffer();
		card.is_buffer_written = 1;
		card.is_buffer_current = 1;
	}
#endif /* SD_SPI_BUFFER */

	/* SD cards 2GB or less address by bytes so multiply by 512 to address
	   by blocks. */
	if (card.card_type != SD_CARD_TYPE_SDHC)
	{
		start_block_address <<= 9;
		end_block_address <<= 9;
	}

	/* The start and end address of the blocks to be erased must be sent to the
	   SD and then the erase command is called. */
	if (sd_spi_send_command(SD_CMD_ERASE_WR_BLK_START, start_block_address) ||
		sd_spi_send_command(SD_CMD_ERASE_WR_BLK_END, end_block_address) ||
		sd_spi_send_command(SD_CMD_ERASE, 0))
	{
		sd_spi_unselect_card();

		return SD_ERR_ERASE_FAILURE;
	}

	if (sd_spi_wait_if_busy(SD_ERASE_TIMEOUT))
	{
		sd_spi_unselect_card();

		return SD_ERR_ERASE_TIMEOUT;
	}

	sd_spi_unselect_card();

	return SD_ERR_OK;
}

uint32_t
sd_spi_card_size(
	void
)
{
	if (sd_spi_send_command(SD_CMD_SEND_CSD, 0))
	{
		sd_spi_unselect_card();

		return SD_ERR_READ_REGISTER;
	}

	uint16_t timeout_start = millis();

	while (sd_spi_spi_receive() != SD_TOKEN_START_BLOCK)
	{
		if (((uint16_t) millis() - timeout_start) > SD_READ_TIMEOUT)
		{
			return sd_spi_card_status();
	    }
	}

	uint8_t c_size_high;
	uint8_t c_size_mid;
	uint8_t c_size_low;
	uint32_t number_of_blocks;

	uint8_t b = sd_spi_spi_receive();
	uint8_t csd_structure = b >> 6;

	uint8_t current_byte;

	for (current_byte = 0; current_byte < 5; current_byte++)
	{
		sd_spi_spi_receive();
	}

	b = sd_spi_spi_receive();
	uint8_t max_read_bl_len = b & 0x0F;

	if (csd_structure == 0)
	{
		c_size_high = (b << 2) & 0x0C;
		b = sd_spi_spi_receive();
		c_size_high |= b >> 6;
		c_size_low = b << 2;
		b = sd_spi_spi_receive();
		c_size_low |= b >> 6;
		sd_spi_spi_receive();
		
		uint8_t c_size_mult;
		c_size_mult = (b << 1) & 0x06;
		b = sd_spi_spi_receive();
		c_size_mult |= b >> 7;

		number_of_blocks = (uint32_t) ((c_size_high << 8 | c_size_low) + 1) *
	    						 		(1 << (c_size_mult + 2));
		number_of_blocks *= (uint32_t) (1 << max_read_bl_len) / 512;
	}
	else
	{
		b = sd_spi_spi_receive();
		c_size_high = b;
		b = sd_spi_spi_receive();
		c_size_mid = b;
		b = sd_spi_spi_receive();
		c_size_low = b;

		number_of_blocks = (uint32_t) (c_size_high << 16 | c_size_mid << 8 |
									   c_size_low) + 1;
		number_of_blocks <<= 10;
	}

	for (current_byte = 11; current_byte < 16; current_byte++)
	{
		sd_spi_spi_receive();
	}

	/* Discard CRC. */
	sd_spi_spi_receive();
	sd_spi_spi_receive();

	sd_spi_unselect_card();

	return number_of_blocks;
}

int8_t
sd_spi_read_cid_register(
	sd_spi_cid_t *cid
)
{
	if (sd_spi_send_command(SD_CMD_SEND_CID, 0))
	{
		sd_spi_unselect_card();

		return SD_ERR_READ_REGISTER;
	}

	uint16_t timeout_start = millis();

	while (sd_spi_spi_receive() != SD_TOKEN_START_BLOCK)
	{
		if (((uint16_t) millis() - timeout_start) > SD_READ_TIMEOUT)
		{
			return sd_spi_card_status();
	    }
	}

    /* See SD Specifications for more information. */
	cid->mid = sd_spi_spi_receive();

	uint8_t i;
	for (i = 0; i < 2; i++)
	{
		cid->oid[i] = sd_spi_spi_receive();
	}

	for (i = 0; i < 5; i++)
	{
		cid->pnm[i] = sd_spi_spi_receive();
	}

	i = sd_spi_spi_receive();
	cid->prv_n = i >> 4;
	cid->prv_m = i;

	cid->psn_high = sd_spi_spi_receive();
	cid->psn_mid_high = sd_spi_spi_receive();
	cid->psn_mid_low = sd_spi_spi_receive();
	cid->psn_low = sd_spi_spi_receive();

	i = sd_spi_spi_receive();
	cid->mdt_year = (i << 4) & 0xF0;
	i = sd_spi_spi_receive();
	cid->mdt_year |= i >> 4;
	cid->mdt_month = i;
	cid->crc = sd_spi_spi_receive() >> 1;

	/* Discard CRC */
	sd_spi_spi_receive();
	sd_spi_spi_receive();

	sd_spi_unselect_card();

	return SD_ERR_OK;
}

int8_t
sd_spi_read_csd_register(
	sd_spi_csd_t *csd
)
{
	if (sd_spi_send_command(SD_CMD_SEND_CSD, 0))
	{
		sd_spi_unselect_card();

		return SD_ERR_READ_REGISTER;
	}

	uint16_t timeout_start = millis();

	while (sd_spi_spi_receive() != SD_TOKEN_START_BLOCK)
	{
		if (((uint16_t) millis() - timeout_start) > SD_READ_TIMEOUT)
		{
			return sd_spi_card_status();
	    }
	}

    /* See SD Specification for more information. */
    uint8_t b = sd_spi_spi_receive();
	csd->csd_structure = b >> 6;
	b = sd_spi_spi_receive();
	csd->taac = b;
	b = sd_spi_spi_receive();
	csd->nsac = b;
	b = sd_spi_spi_receive();
	csd->tran_speed = b;
	b = sd_spi_spi_receive();
	csd->ccc_high = b >> 4;
	csd->ccc_low |= b << 4;
	b = sd_spi_spi_receive();
	csd->ccc_low |= b >> 4;
	csd->max_read_bl_len = b;
	b = sd_spi_spi_receive();
	csd->read_bl_partial = b >> 7;
	csd->write_bl_misalign = b >> 6;
	csd->read_bl_misalign = b >> 5;
	csd->dsr_imp = b >> 4;

	if (csd->csd_structure == 0)
	{
		csd->cvsi.v1.c_size_high = (b << 2) & 0x0C;
		b = sd_spi_spi_receive();
		csd->cvsi.v1.c_size_high |= b >> 6;
		csd->cvsi.v1.c_size_low = b << 2;
		b = sd_spi_spi_receive();
		csd->cvsi.v1.c_size_low |= b >> 6;
		csd->cvsi.v1.vdd_r_curr_min = b >> 3;
		csd->cvsi.v1.vdd_r_curr_max = b;
		b = sd_spi_spi_receive();
		csd->cvsi.v1.vdd_w_curr_min = b >> 5;
		csd->cvsi.v1.vdd_w_curr_max = b >> 2;
		csd->cvsi.v1.c_size_mult = (b << 1) & 0x06;
		b = sd_spi_spi_receive();
		csd->cvsi.v1.c_size_mult |= b >> 7;
	}
	else
	{
		b = sd_spi_spi_receive();
		csd->cvsi.v2.c_size_high = b;
		b = sd_spi_spi_receive();
		csd->cvsi.v2.c_size_mid = b;
		b = sd_spi_spi_receive();
		csd->cvsi.v2.c_size_low = b;
		b = sd_spi_spi_receive();
	}

	csd->erase_bl_en = b >> 6;
	csd->erase_sector_size = (b << 1) & 0x7E;
	b = sd_spi_spi_receive();
	csd->erase_sector_size |= b >> 7;
	csd->wp_grp_size = b << 1;
	b = sd_spi_spi_receive();
	csd->wp_grp_enable = b >> 7;
	csd->r2w_factor = b >> 2;
	csd->write_bl_len = (b << 2) & 0x0C;
	b = sd_spi_spi_receive();
	csd->write_bl_len |= b >> 6;
	csd->write_bl_partial = b >> 5;
	b = sd_spi_spi_receive();
	csd->file_format_grp = b >> 7;
	csd->copy = b >> 6;
	csd->perm_write_protect = b >> 5;
	csd->tmp_write_protect = b >> 4;
	csd->file_format = b >> 2;
	b = sd_spi_spi_receive();
	csd->crc = b >> 1;

	/* Discard CRC. */
	sd_spi_spi_receive();
	sd_spi_spi_receive();

	sd_spi_unselect_card();

	return SD_ERR_OK;
}

int8_t
sd_spi_card_status(
	void
)
{
	int8_t response = sd_spi_r2_error((sd_spi_send_command(SD_CMD_SEND_STATUS, 0)
									  << 8) | sd_spi_spi_receive());

	sd_spi_unselect_card();

	return response;
}

static void
sd_spi_clear_buffer(
	void
)
{
#ifdef SD_SPI_BUFFER
	uint16_t i;
	for (i = 0; i < 512; i++) {
		card.sd_spi_buffer[i] = 0;
	}

	card.is_buffer_written = 0;
	card.is_buffer_current = 0;
#endif /* SD_SPI_BUFFER */
}

static int8_t
sd_spi_write_out_data(
	uint32_t	block_address,
	void	 	*data,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
)
{
	sd_spi_select_card();

	if (card.is_read_write_continuous)
	{
		/* Wait for card to complete the previous write. */
	  	if (sd_spi_wait_if_busy(SD_WRITE_TIMEOUT))
	  	{
	  		sd_spi_unselect_card();

	  		return SD_ERR_WRITE_TIMEOUT;
	  	}

		/* Send token for multiple block write. */
	  	sd_spi_spi_send(SD_TOKEN_MULTIPLE_WRITE_START_BLOCK);
	}
	else
	{
		/* SD cards 2GB or less address by bytes so multiply by 512 to address
	  	   by blocks. */
		if (card.card_type != SD_CARD_TYPE_SDHC)
		{
			block_address <<= 9;
		}

		/* Send the command to start writing a single block. */
		if (sd_spi_send_command(SD_CMD_SET_WRITE_BLOCK, block_address))
		{
			sd_spi_unselect_card();

			return SD_ERR_WRITE_FAILURE;
		}

		/* Send token for single block write. */
		sd_spi_spi_send(SD_TOKEN_START_BLOCK);
	}

	uint16_t i;

	/* Pad data with 0. */
	for (i = 0; i < byte_offset; i++)
	{
		sd_spi_spi_send(0);
	}

	/* Write block. */
	for (i = byte_offset; i < byte_offset + number_of_bytes; i++)
	{
		sd_spi_spi_send(((uint8_t *) data)[i - byte_offset]);
	}

	/* Pad data with 0. */
	for (i = byte_offset + number_of_bytes; i < 512; i++)
	{
		sd_spi_spi_send(0);
	}	

	/* Send dummy CRC. */
	sd_spi_spi_send(0xFF);
	sd_spi_spi_send(0xFF);

	/* Check if write was successful. */
	switch (sd_spi_spi_receive() & 0x0F)
	{
		case SD_TOKEN_DATA_REJECTED_CRC:
			return SD_ERR_WRITE_DATA_CRC_REJECTED;
		case SD_TOKEN_DATA_REJECTED_WRITE_ERR:
			return SD_ERR_WRITE_DATA_REJECTED;
		case SD_TOKEN_DATA_ACCEPTED:
			break;
		default:
			return SD_ERR_WRITE_FAILURE;
	}

	if (card.is_read_write_continuous)
	{
		card.continuous_block_address++;

#ifdef SD_SPI_BUFFER
		card.buffered_block_address = card.continuous_block_address;
#endif /* SD_SPI_BUFFER */
	}
	else {
		/* Wait for card to complete the write. */
	  	if (sd_spi_wait_if_busy(SD_WRITE_TIMEOUT))
	  	{
	  		sd_spi_unselect_card();

	  		return SD_ERR_WRITE_TIMEOUT;
	  	}

	  	return sd_spi_card_status();
	}

	return SD_ERR_OK;
}

static int8_t
sd_spi_read_in_data(
	uint32_t 	block_address,
	void	 	*data_buffer,
	uint16_t 	number_of_bytes,
	uint16_t 	byte_offset
)
{
	sd_spi_select_card();

	if (!card.is_read_write_continuous)
	{
		/* SD cards 2GB or less address by bytes so multiply by 512 to address
		   by blocks. */
		if (card.card_type != SD_CARD_TYPE_SDHC)
		{
			block_address <<= 9;
		}

		/* Start single block reading. */
	    if (sd_spi_send_command(SD_CMD_READ_SINGLE_BLOCK, block_address))
	    {
	    	sd_spi_unselect_card();

	    	return SD_ERR_READ_FAILURE;
	    }
	}

	uint16_t timeout_start = millis();

    /* Must wait for read token from card before reading. */
	while (sd_spi_spi_receive() != SD_TOKEN_START_BLOCK)
	{
		if (((uint16_t) millis() - timeout_start) > SD_READ_TIMEOUT)
		{
	    	return sd_spi_card_status();
	    }
	}

	uint16_t i;

#ifdef SD_SPI_BUFFER	/* Read block into sd_spi_buffer if it is defined. */
	/* Read in the bytes to the buffer. */
	for (i = 0; i < 512; i++)
	{
		card.sd_spi_buffer[i] = sd_spi_spi_receive();
	}

	/* Throw out CRC. */
	sd_spi_spi_receive();
	sd_spi_spi_receive();

	if (card.is_read_write_continuous)
	{
		card.continuous_block_address++;
		card.buffered_block_address = card.continuous_block_address;
	}
	else
	{
		if (card.card_type != SD_CARD_TYPE_SDHC)
		{
			block_address >>= 9;
		}

		card.buffered_block_address = block_address;
	}

	card.is_buffer_current = 1;
#else /* SD_SPI_BUFFER */
    /* Throw out the bytes until the offset is reached. */
	for (i = 0; i < byte_offset; i++)
	{
		sd_spi_spi_receive();
	}

	/* Read in the bytes to the buffer. */
	for (i = 0; i < number_of_bytes; i++)
	{
		((uint8_t *) data_buffer)[i] = sd_spi_spi_receive();
	}

	/* Throw out any remaining bytes in the page plus CRC. */
    for (i = 0; number_of_bytes + byte_offset + i < 514; i++)
    {
    	sd_spi_spi_receive();
    }
#endif

	return SD_ERR_OK;
}

static uint8_t
sd_spi_send_command(
	uint8_t 	command,
	uint32_t 	argument
)
{
	sd_spi_select_card();

	sd_spi_spi_receive();

	/* Send command with transmission bit. */
	sd_spi_spi_send(0x40 | command);

	/* Send argument. */
	sd_spi_spi_send(argument >> 24);
	sd_spi_spi_send(argument >> 16);
	sd_spi_spi_send(argument >> 8);
	sd_spi_spi_send(argument >> 0);

	/* Send CRC. */
	switch(command)
	{
		/* CRC for CMD0 with arg 0. */
		case SD_CMD_GO_IDLE_STATE: 
			sd_spi_spi_send(0x95);
			break; 
		/* CRC for CMD8 with arg 0X1AA. */
		case SD_CMD_SEND_IF_COND:
			sd_spi_spi_send(0x87);
			break; 
		default:
			sd_spi_spi_send(0xFF);
			break;
	}

	/* Wait for response. Can take up to 64 clock cycles. */
	uint8_t i;
	uint8_t response;
	for (i = 0; i < 8; i++)
	{
		response = sd_spi_spi_receive();

		if (response != 0xFF)
		{
			break;
		}
	}

	return response;
}

static uint8_t
sd_spi_send_app_command(
	uint8_t 	command,
	uint32_t 	argument
)
{
	sd_spi_send_command(SD_CMD_APP, 0);

	return sd_spi_send_command(command, argument);
}

static int8_t
sd_spi_wait_if_busy(
	uint16_t max_time_to_wait
)
{
	uint16_t timeout_start = millis();

	while (sd_spi_spi_receive() != 0xFF)
	{
		if (((uint16_t) millis() - timeout_start) > max_time_to_wait)
		{
	    	return 1;
	    }
	}

	return 0;
}

static int8_t
sd_spi_r1_error(
	uint8_t r1_response
)
{
	if (r1_response == 0x00)
	{
		return SD_ERR_OK;
	}
	else if (r1_response & SD_IN_IDLE_STATE)
	{
		return SD_ERR_NOT_INITIALIZED;
	}
	else if (r1_response & SD_ERASE_RESET)
	{
		return SD_ERR_ERASE_RESET;
	}
	else if (r1_response & SD_ILLEGAL_COMMAND)
	{
		return SD_ERR_ILLEGAL_COMMAND;
	}
	else if (r1_response & SD_COMMUNICATION_CRC_ERR)
	{
		return SD_ERR_COMMUNICATION_CRC;
	}
	else if (r1_response & SD_ERASE_SEQUENCE_ERR)
	{
		return SD_ERR_ERASE_SEQUENCE;
	}
	else if (r1_response & SD_ADDRESS_ERR)
	{
		return SD_ERR_ILLEGAL_ADDRESS;
	}
	else if (r1_response & SD_PARAMETER_ERR)
	{
		return SD_ERR_ILLEGAL_PARAMETER;
	}

	return SD_ERR_OK;
}

static int8_t
sd_spi_r2_error(
	uint16_t r2_response
)
{
	if (sd_spi_r1_error(r2_response >> 8))
	{
		return sd_spi_r1_error(r2_response >> 8);
	}
	else
	{
		if (r2_response == 0x00)
		{
			return SD_ERR_OK;
		}
		else if (r2_response & SD_CARD_IS_LOCKED)
		{
			return SD_ERR_CARD_IS_LOCKED;
		}
		else if (r2_response & SD_WP_ERASE_SKIP)
		{
			return SD_ERR_WRITE_PROTECTION_ERASE_SKIP;
		}
		else if (r2_response & SD_GENERAL_ERR)
		{
			return SD_ERR_GENERAL;
		}
		else if (r2_response & SD_CC_ERR)
		{
			return SD_ERR_CARD_CONTROLLER;
		}
		else if (r2_response & SD_CARD_ECC_FAILURE)
		{
			return SD_ERR_CARD_ECC_FAILURE;
		}
		else if (r2_response & SD_WP_VIOLATION)
		{
			return SD_ERR_WRITE_PROTECTION_VIOLATION;
		}
		else if (r2_response & SD_ERASE_PARAM)
		{
			return SD_ERR_ERASE_PARAMETER;
		}
		else if (r2_response & SD_OUT_OF_RANGE)
		{
			return SD_ERR_ARGUMENT_OUT_OF_RANGE;
		}
	}

	return SD_ERR_OK;
}

static void
sd_spi_select_card(
	void
)
{
#ifdef USE_HARDWARE_SPI

#else /* USE_HARDWARE_SPI */
	digitalWrite(card.chip_select_pin, LOW);

	if (card.is_chip_select_high)
	{
    	card.is_chip_select_high = 0;

    	if (card.spi_speed == 0)
    	{
    		SPI.beginTransaction(SPISettings(250000, MSBFIRST, SPI_MODE0));
    	}
    	else
    	{
    		SPI.beginTransaction(SPISettings(25000000, MSBFIRST, SPI_MODE0));
    	}
	}
#endif
}

static void
sd_spi_unselect_card(
	void
)
{
#ifdef USE_HARDWARE_SPI

#else /* USE_HARDWARE_SPI */
	/* Host has to wait 8 clock cycles after a command. */
	sd_spi_spi_receive();

	digitalWrite(card.chip_select_pin, HIGH);

	if (!card.is_chip_select_high)
	{
    	card.is_chip_select_high = 1;
    	SPI.endTransaction();
	}
#endif
}

static void
sd_spi_spi_send(
	uint8_t b
)
{
#ifdef USE_HARDWARE_SPI

#else /* USE_HARDWARE_SPI */
	SPI.transfer(b);
#endif
}

static uint8_t
sd_spi_spi_receive(
	void
)
{
#ifdef USE_HARDWARE_SPI

#else /* USE_HARDWARE_SPI */
	return SPI.transfer(0xFF);
#endif
}

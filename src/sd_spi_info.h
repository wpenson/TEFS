/******************************************************************************/
/**
@file		sd_spi_info.h
@author     Wade H. Penson
@date		June, 2015
@brief      Structures to store information from CID (Card Identification)
			and CSD (Card Specific Data) registers for SD cards.
@details	Information about the fields can be found in the simplified
			physical SD specifications from the SD Association.
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

#ifndef SD_SPI_INFO_H_
#define SD_SPI_INFO_H_

#ifdef  __cplusplus
extern "C" {
#endif

/**
@brief CID register information
*/
typedef struct sd_spi_cid
{
	/** Manufacturer ID. */
	unsigned int	mid:			8;
	/** OEM/Application ID. Composed of 2 characters. */
	char			oid[2];
	/** Product name. Composed of 5 characters. */
	char			pnm[5];
	/** n part of the product revision number, which is in the form of n.m */
	unsigned int	prv_n:			4;
	/** m part of the product revision number, which is in the form of n.m */
	unsigned int	prv_m:			4;
	/** Bits [31:24] of the 4 byte product serial number. */
	unsigned int	psn_high:		8;
	/** Bits [23:16] of the 4 byte product serial number. */
	unsigned int	psn_mid_high:	8;
	/** Bits [15:8] of the 4 byte product serial number. */
	unsigned int	psn_mid_low:	8;
	/** Bits [7:0] of the 4 byte product serial number. */
	unsigned int	psn_low:		8;
	/** Year manufactured. */
	unsigned int	mdt_year:		8;
	/** Month manufactured. */
	unsigned int	mdt_month:		4;
	/* Bitfield padding */
	unsigned int:					4;
	/** Checksum for the CID. */
	unsigned int	crc:			7;
	/* Bitfield padding */
	unsigned int:					1;
} sd_spi_cid_t;

/**
@brief		Information exclusive to CSD register version 1.

@details 	The following formula is used to compute the card's size:<br><br>
			Card size in MB = Number of blocks * 512 / 1000000<br>
			Number of blocks = ((c_size + 1) * 2<SUP>c_size_mult + 2</SUP>) * 
			(2<SUP>max_read_bl_len</SUP> / 512)<br>
*/
typedef struct sd_spi_csd_v1
{
	/** Low bits of c_size. Used to compute the card's size. */
	unsigned int	c_size_low:		8;
	/** High bits of c_size. Used to compute the card's size. */
	unsigned int	c_size_high:	4;
	/** Max read current for minimal supply voltage. */
	unsigned int	vdd_r_curr_min:	3;
	/* Bitfield padding */
	unsigned int:					1;
	/** Max read current for max supply voltage. */
	unsigned int	vdd_r_curr_max:	3;
	/** Max write current for minimal supply voltage. */
	unsigned int	vdd_w_curr_min:	3;
	/* Bitfield padding */
	unsigned int:					2;
	/** Max write current for max supply voltage. */
	unsigned int	vdd_w_curr_max:	3;
	/** Multiplier used to compute the card's size. */
	unsigned int 	c_size_mult:	3;
	/* Bitfield padding */
	unsigned int:					2;
} sd_spi_csd_v1_t;

/**
@brief		Information exclusive to CSD register version 2

@details 	The card size is computed as follows:<br><br>
			Card size in MB = (c_size + 1) * 512 / 1000
*/
typedef struct sd_spi_csd_v2
{
	/** Low bits of c_size. Used to compute the card's size. */
	unsigned int	c_size_low:		8;
	/** Mid bits of c_size. Used to compute the card's size. */
	unsigned int	c_size_mid:		8;
	/** High bits of c_size. Used to compute the card's size. */
	unsigned int	c_size_high:	6;
	/* Bitfield padding */
	unsigned int:					2;
} sd_spi_csd_v2_t;

/**
@brief 	The CSD register is either version 1 or version 2. Space is saved by
		unioning the specific version information for version 1 and 2. The
		csd_v1 structure is for version 1 and the csd_v2 is for version 2.
*/
typedef union sd_spi_csd_v_info
{
	sd_spi_csd_v1_t v1;
	sd_spi_csd_v2_t v2;
} sd_spi_csd_v_info_t;

/**
@brief		CSD register information
@details 	The CSD version is determined by the csd_structure value. If the
			value is 0, the CSD is version 1 so the csvi will contain the
			v1 type specific information. Otherwise, the CSD is version 2 so 
			the csvi will contain the v2 type specific information. Further
			details about the fields can be found in the SD specifications.
*/
typedef struct sd_spi_csd
{
	/** Union of the CSD version specific info. */
	sd_spi_csd_v_info_t		cvsi;
	/** CSD version. */
	unsigned int 			csd_structure:		2;
	/** The file format of the card. */
	unsigned int			file_format:		2;
	/* Bitfield padding */
	unsigned int:								4;
	/** Asynchronous part of data access time. Table of the corresponding
		values are found in the SD specifications. */
	unsigned int			taac:				8;
	/** Worst case for the clock dependent factor of the data access time.*/
	unsigned int			nsac:				8;
	/** Max data transfer speed for a single data line. */
	unsigned int			tran_speed:			8;
		/** Low bits of the ccc; the command class of the card. */
	unsigned int			ccc_low:			8;
	/** High bits of the ccc; the command class of the card. */
	unsigned int			ccc_high:			4;
	/** Max block length. */
	unsigned int			max_read_bl_len:	4;
	/** Defines if the card can read partial blocks. */
	unsigned int			read_bl_partial:	1;
	/** Defines if a data block can be spanned over a block boundary when
	    written. */
	unsigned int			write_bl_misalign:	1;
	/** Defines if a data can be read over block boundaries. */
	unsigned int			read_bl_misalign:	1;
	/** Defines if the configurable driver stage is implemented in the card. */
	unsigned int			dsr_imp:			1;
	/** Defines the unit size of data being erased. */
	unsigned int			erase_bl_en:		1;
	/** Ratio of read to write time. */
	unsigned int			r2w_factor:			3;
	/** The size of an erasable sector. */
	unsigned int			erase_sector_size:	7;
	/** Defines if write group protection is possible. */
	unsigned int			wp_grp_enable:		1;
	/** The size of a write protected group. */
	unsigned int			wp_grp_size:		7;
	/** Defines if the card can write partial blocks. */
	unsigned int			write_bl_partial:	1;
	/** The max write block length. */
	unsigned int			write_bl_len:		4;
	/** The file format group of the card. */
	unsigned int			file_format_grp:	1;
	/** Defines if the cards content is original or is a copy. */
	unsigned int			copy:				1;
	/** Determines if the card is permanently write protected or not. */
	unsigned int			perm_write_protect:	1;
	/** Determines if the card is temporarily write protected or not. */
	unsigned int			tmp_write_protect:	1;
	/** Checksum for the CSD. */
	unsigned int			crc:				7;
	/* Bitfield padding */
	unsigned int:								1;
} sd_spi_csd_t;

#ifdef  __cplusplus
}
#endif

#endif /* SD_SPI_INFO_H_ */
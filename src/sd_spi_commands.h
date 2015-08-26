/******************************************************************************/
/**
@file       sd_spi_commands.h
@author     Wade H. Penson
@date       June, 2015
@brief      SPI bus commands for SD/MMC.
@details    The set of commands can be found in the simplified physical
            SD specifications from the SD Association.
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

#ifndef SD_SPI_COMMANDS_H_
#define SD_SPI_COMMANDS_H_

#ifdef  __cplusplus
extern "C" {
#endif

/**
@brief    CMD0: Resets card.
@param    [31:0] Stuff bits
@return   Register R1
*/
#define SD_CMD_GO_IDLE_STATE 0x00

/**
@brief  CMD1: Host sends HCS (Host Capacity Support) information. The card then
        initializes.
@param  [31] Zero bit
@param  [30] HCS
@param  [29:0] Zero bits
@return Register R1
*/
#define SD_CMD_SEND_OP_COND 0x01

/**
@brief  CMD8: Host sends supply voltage and card replies with whether the card
        can operate based on the voltage or not.
@param  [31:12] Zero bits
@param  [11:8] Supply voltage range
@param  [7:0] Check pattern
@return Register R7
*/
#define SD_CMD_SEND_IF_COND 0x08

/**
@brief  CMD9: Get the card specific data (CSD).
@param  [31:0] Stuff bits
@return Register R1
*/
#define SD_CMD_SEND_CSD 0x09

/**
@brief  CMD10: Get the card identification (CID).
@param  [31:0] Stuff bits
@return Register R1
*/
#define SD_CMD_SEND_CID 0x0A

/**
@brief  CMD12: Stops the transmission for the multiple block read operation.
@param  [31:0] Stuff bits
@return Register R1b (R1 with busy signal after)
*/
#define SD_CMD_STOP_TRANSMISSION 0x0C

/**
@brief  CMD13: Get status from status register.
@param  [31:0] Stuff bits
@return Register R2
*/
#define SD_CMD_SEND_STATUS 0x0D

/**
@brief  CMD16: Sets the block length, in bytes, for read and write commands of a
        standard capacity card. However, the standard capacity cards usually 
        only support this command for the read commands. SDHC and SHXC only
        support reading and writing blocks of size 512 bytes.
@param  [31:0] Block length
@return Register R1
*/
#define SD_CMD_SET_BLOCKLEN 0x10

/**
@brief  CMD17: Read a single block. The size can be set using CMD_SET_BLOCKLEN
        for standard capacity cards.
@param  [31:0] Data address. The address is determined by bytes for standard sd
               cards and by blocks (512 bytes) for SDHC and SHXC cards.
@return Register R1
*/
#define SD_CMD_READ_SINGLE_BLOCK 0x11

/**
@brief  CMD18: Read blocks continuously and end transmission by sending
        CMD_STOP_TRANSMISSION.
@param  [31:0] Data address. The address is determined by bytes for standard
              sd cards and by blocks (512 bytes) for SDHC and SHXC cards.
@return Register R1
*/
#define SD_CMD_READ_MULTIPLE_BLOCK 0x12

/**
@brief  CMD24: Write a single block.
@param  [31:0] Data address. The address is determined by bytes for standard
              sd cards and by blocks (512 bytes) for SDHC and SHXC cards.
@return Register R1
*/
#define SD_CMD_SET_WRITE_BLOCK 0x18

/**
@brief  CMD25: Write blocks continuously and end transmission by sending
        STOP_TRAN_TOKEN.
@param  [31:0] Data address. The address is determined by bytes for standard
              sd cards and by blocks (512 bytes) for SDHC and SHXC cards.
@return Register R1
*/
#define SD_CMD_WRITE_MULTIPLE_BLOCK 0x19

/**
@brief  CMD27: Program the programmable bits of the CSD. These include being
        able to write multiple times to the TMP_WRITE_PROTECT and CRC fields
        and writing a single time to the FILE_FORMAT_GRP, COPY,
        PERM_WRITE_PROTECT, and FILE_FORMAT fields.
@param  [31:0] Stuff bits
@return Register R1
*/
#define SD_CMD_PROGRAM_CSD 0x1B

/**
@brief  CMD28: Sets write protection for the addressed group for standard
        capacity cards that support write protected features. WP_GRP_ENABLE
        in the CSD register can be used to determine if this command is
        supported and WP_GROUP_SIZE determines the size of a group.
@param  [31:0] Data address
@return Register R1b (R1 with busy signal after)
*/
#define SD_CMD_SET_WRITE_PROT 0x1C

/**
@brief  CMD29: Clears write protection for the addressed group for standard
        capacity cards that support write protected features. WP_GRP_ENABLE
        in the CSD register can be used to determine if this command is
        supported and WP_GROUP_SIZE determines the size of a group.
@param  [31:0] Data address
@return Register R1b (R1 with busy signal after)
*/
#define SD_CMD_CLR_WRITE_PROT 0x1D

/**
@brief  CMD30: Send status of the addressed group for standard capacity cards
        that support write protected features. WP_GRP_ENABLE in the CSD register
        can be used to determine if this command is supported and WP_GROUP_SIZE
        determines the size of a group.
@param  [31:0] Data address
@return Register R1
*/
#define SD_CMD_SEND_WRITE_PROT 0x1E

/**
@brief  CMD32: Sets the address of the first block for writing to be erased.
@param  [31:0] Data address. The address is determined by bytes for standard
        sd cards and by blocks (512 bytes) for SDHC and SHXC cards.
@return Register R1
*/
#define SD_CMD_ERASE_WR_BLK_START 0x20

/**
@brief  CMD33: Sets the address of the last block for writing to be erased.
@param  [31:0] Data address. The address is determined by bytes for standard
        sd cards and by blocks (512 bytes) for SDHC and SHXC cards.
@return Register R1
*/
#define SD_CMD_ERASE_WR_BLK_END 0x21

/**
@brief  CMD38: Erase the blocks selected by CMD_ERASE_WR_BLK_START and
        CMD_ERASE_WR_BLK_END.
@param  [31:0] Stuff bits
@return Register R1b (R1 with busy signal after)
*/
#define SD_CMD_ERASE 0x26

/**
@brief  CMD58: Reads OCR register. CCS bit is assigned to OCR[30].
@param  [31:0] Stuff bits
@return Register R3
*/
#define SD_CMD_READ_OCR 0x3A

/**
@brief  CMD59: Turns CRC on or off.
@param  [31:1] Stuff bits
@param  [0:0] A '1' to turn CRC on and '0' to turn CRC off
@return Register R1
*/
#define SD_CMD_CRC_ON_OFF 0x3B

/**
@brief  CMD55: Tells the card that the next command is an application specific
        command (ACMD).
@param  [31:0] Stuff bits
@return Register R1
*/
#define SD_CMD_APP 0x37

/**
@brief  CMD56: For application specific commands, transfer a datablock to or
        from the card.
@param  [31:1] Stuff bits
@param  [0:0] A '1' for reading data and '0' for writing data
@return Register R1
*/
#define SD_CMD_GEN 0x38

/**
@brief  ACMD13: Get the SD status.
@param  [31:0] Stuff bits
@return Register R2
*/
#define SD_ACMD_STATUS 0x0D

/**
@brief  ACMD22: Get the number of blocks that were written without errors.
@param  [31:0] Stuff bits
@return Register R1
*/
#define SD_ACMD_SEND_NUM_WR_BLOCKS 0x16

/**
@brief  ACMD23: Set the number of blocks to be erased before writing.
@param  [31:23] Stuff bits
@param  [22:0] Number of blocks
@return Register R1
*/
#define SD_ACMD_SET_WR_BLK_ERASE_COUNT 0x17

/**
@brief  ACMD41: Host sends HCS (Host Capacity Support) information. The card
        then initializes.
@param  [31] Zero bit
@param  [30] HCS
@param  [29:0] Zero bits
@return Register R1
*/
#define SD_ACMD_SEND_OP_COND 0x29

/**
@brief  ACMD42: Connects or disconnects the pull-up resistor on the CS pin of
        the card. May be used for card detection.
@param  [31:1] Stuff bits
@param  [0:0] A '1' to connect pull-up and '0' to disconnect pull-up
@return Register R1
*/
#define SD_ACMD_SET_CLR_CARD_DETECT 0x2A

/**
@brief  ACMD51: Reads SCR (SD configuration) register.
@param  [31:0] Stuff bits
@return Register R1
*/
#define SD_ACMD_SEND_SCR 0x33

#ifdef  __cplusplus
}
#endif

#endif /* SD_SPI_COMMANDS_H_ */

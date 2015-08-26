# TEFS - Tiny Embedded File System

TEFS is a file system desgined for the Arduino written in C. It has a small SRAM and program space footprint and utilizes a custom SD/MMC SPI communications library to write and read data to and from an SD card. It also includes support for Adesto Dataflash memory using an FTL library provided by Scott R. Fazackerley.

**Why use TEFS instead of the FAT libary that is already provided in the Arduino core?**

Faster and smaller memory footprint?

## Current Release

Link to stable release here.

## Usage

First download a stable release of TEFS and extract the files to the project folder that contains your project's `.ino` file. TEFS options can be found in the `tefs_config.h` configuration file.

TEFS can be used either as a block level interface or as an interface similar to the standard input/output file operations in C. If you wish to just use the block level interface exclusively, delete `tefs_fo.h` and `tefs_fo.c` from your project folder.

#### Formatting

Your SD or dataflash must first be formatted for TEFS to work. Modify and upload the following sketch to your Arduino to format:
```C
#include <SPI.h>
#include "tefs.h"

#define CHIP_SELECT_PIN 4   /* Set this to your chip select (CS) pin. */

void setup()
{
	Serial.begin(115200);

	if (sd_raw_init(CHIP_SELECT_PIN))
	{
		Serial.print(F("SD card failed to initialize.\n"));
		return;
	}

	Serial.print(F("SD card initialized successfully!\n"));
	Serial.print(F("Formatting card with TEFS.\n"));
	
	/* Page size of 512 bytes, 64 page block or cluster size (32KB per block or cluster), 
	   key size of 1 (allows up to 255 files), and pre-erase card before format. */
	tefs_format_device(sd_raw_card_size(), 512, 64, 1, 1);
	
	Serial.print(F("Format Successful!\n"));
}
```

#### Opening, Closing, and Deleting a file
```C
#include <SPI.h>
#include "tefs.h"
#include "tefs_fo.h"    /* Include if you wish to use the standard input/output file operations. */

void setup()
{
  /* Create a tefs_t file struture and then open a file with the ID 1.
     If the file doesn't already exist, it will be created. */
  tefs_t file;
  tefs_open(&file, 1);
  
  /* Write a string of 13 characters in length. */
  char str[13] = "Hello, File!";
  tefs_fwrite(str, sizeof(char), sizeof(str), &file);
  
  tefs_close(&file);    /* Close the file. */
  tefs_remove(1);  /* Delete the file with ID 1. */
}
```
Other than fopen(), fclose(), and fremove(), the standard file operations like fwrite(), fread(), etcetera are the same as they are in `stdio.h` for C.

## Features

* Support for SD, MMC, and Adesto Dataflash
* Program size of ??? KB and memory usage of ??? bytes. A memory usage of ??? for each additional open file.
* Max file size depends on page size and block size. For example, on the SD, the max file size is ??? when the block size is 16 pages or 8KB. The max size is ??? when the block size is 64 pages.
* SD cards that have 
* Uses IDs for instead of file names to speed up file lookup.

## Author

Wade Penson

wpenson@alumni.ubc.ca

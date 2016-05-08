# TEFS - Tiny Embedded File System
TEFS is a lightweight, alternative file system designed for microcontrollers that is not FAT. It is written in C and it has a small RAM and program space footprint. It utilizes a custom SD/MMC SPI communications library to write and read data to and from an SD card. It works with Arduino out of the box and can easily be ported to different platforms. Specifically, it can be adapted to work with block based storage device that have an FTL such as SD cards, etc.

TEFS uses a tree index structure instead a structure such as the File Allocation Table so it has some trade-offs compared to FAT. From comparing benchmarks of TEFS against FAT filesystems on microcontrollers, FAT performs better when the are fewer files, smaller files, and files are created and removed frequently. However, TEFS performs better when there are many files, larger files, and files are opened and closed often but not created and removed as often. TEFS also has a smaller code size and can have long file names as well as custom metadata for each file.

## Usage
If you want to use TEFS with the Arduino IDE, you must put the required source files in a single folder with your `.ino` file. A python flatting script is provided that does this for you.

If you are not using the Arduino IDE, you must add the code specific for your platform in the `src/tefs/sd_spi/src/device/sd_spi_platform_dependencies.c` file if you are using an SD card.

TEFS can be used either as a block level interface or as an interface similar to the standard input/output file operations in C. If you wish to just use the block level interface exclusively, delete `tefs_stdio.h` and `tefs_stdio.c` from your project folder.

Precompiler options for TEFS can be found in the `tefs_configuration.h` file.

## Examples
#### Formatting
You storage device must be formatted with TEFS to work. Here is a sketch for the Arduino that demonstrates how to format your storage device:
```C
#include <SPI.h>
#include "tefs.h"

#define CHIP_SELECT_PIN 4   // Set this to your chip select (CS) pin.

void setup()
{
	Serial.begin(115200);

	if (sd_spi_init(CHIP_SELECT_PIN) != SD_ERR_OK)
	{
		Serial.println(F("SD card failed to initialize."));
		return;
	}

	Serial.println(F("SD card initialized successfully!"));
	Serial.println(F("Formatting card with TEFS."));
	
	// Page size of 512 bytes, 64 page cluster size (32KB per cluster), hash size of 4 bytes, 32 byte meta data entry size, and file size that is 12 bytes max.
	if (tefs_format_device(sd_spi_card_size(), 512, 64, 4, 32, 12) != TEFS_ERR_OK) {
	    Serial.print(F("Format failed.\n"));
	}
	
	Serial.print(F("Format Successful!\n"));
}

void loop() {}
```

#### Opening, Closing, and Deleting Files Using the Faster Page Interface
```C
#include <SPI.h>
#include "tefs.h"

void setup()
{
    // Error checking and formatting removed for brevity.

    // Create a T_FILE file struture pointer and then open a file with the name "file.txt". If the file doesn't already exist, it will be created.
    file_t file;
    tefs_open(&file, "file.txt");
  
    char str[13] = "Hello World!";
    tefs_write(&file, 0, str, sizeof(str) * sizeof(char), 0); // Write a string of 13 bytes in length to the first page and first byte in that page for the file.
  
    tefs_close(&file); // Close the file.
    tefs_remove("file.txt"); // Delete the file.
}

void loop() {}
```

#### Opening, Closing, and Deleting Files Using the C File Interface
```C
#include <SPI.h>
#include "tefs.h"
#include "tefs_stdio.h" // Include if you wish to use the C standard input/output file

void setup()
{
    // Error checking and formatting removed for brevity.

    // Create a T_FILE file struture pointer and then open a file with the name "file.txt". If the file doesn't already exist, it will be created.
    T_FILE *file;
    file = t_fopen("file.txt", "wb");
  
    char str[13] = "Hello World!";
    t_fwrite(str, sizeof(char), sizeof(str), file); // Write a string of 13 characters in length.
  
    t_fclose(file); // Close the file.
    t_remove("file.txt"); // Delete the file.
}

void loop() {}
```

## Support
If there are any bugs, create a new issue with a detailed description of your bug. If you have any questions or suggestions, please send me an email. Feel free to contribute to this repository. Any improvements will be greatly appreciated.

## Author
Wade Penson

wpenson@alumni.ubc.ca

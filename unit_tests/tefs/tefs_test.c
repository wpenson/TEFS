#include "../CuTest.h"
#include "tefs.h"

#define NUMBER_OF_PAGES 62500
#define CHIP_SELECT_PIN 4

uint8_t data[512];

void
populate_data_array_1(
	void
)
{
	long int i;

	for (i = 0; i < 26; i++) {
		data[i] = 'a' + i;
	}

	for (i = 26; i < 512; i++) {
		data[i] = '.';
	}
}

void
populate_data_array_2(
	void
)
{
	long int i;

	for (i = 0; i < 26; i++) {
		data[i] = 'A' + i;
	}

	for (i = 26; i < 512; i++) {
		data[i] = '!';
	}
}

static void
format_device(
	void
)
{
	sd_raw_init(CHIP_SELECT_PIN);

	int physical_page_size = 512;
	int block_size = 8;
	int key_size = 1;

	tefs_format_device(NUMBER_OF_PAGES, physical_page_size,
						block_size, key_size, 1);
}

void
test_tefs_format_device(
	CuTest *tc
)
{
	sd_raw_init(CHIP_SELECT_PIN);

	int physical_page_size = 512;
	int block_size = 8;
	int key_size = 1;

	uint8_t byte_buffer = 0;
	uint16_t current_byte;
	for (current_byte = 0; current_byte < 512; current_byte++)
	{
		sd_raw_write(1, &byte_buffer, 1, current_byte);
	}

	sd_raw_flush();

	CuAssertIntEquals(tc, 0, tefs_format_device(NUMBER_OF_PAGES,
												 physical_page_size,
						   						 block_size, key_size, 0));

	for (current_byte = 0; current_byte < 512; current_byte++)
	{
		sd_raw_read(1, &byte_buffer, 1, current_byte);
		CuAssertIntEquals(tc, 0xFF, byte_buffer);
	}

	/* Read and verify the check flag */
	for (current_byte = 0; current_byte < 4; current_byte++)
	{
		sd_raw_read(0, &byte_buffer, 1, current_byte);
		CuAssertIntEquals(tc, TEFS_CHECK_FLAG, byte_buffer);
	}

	uint32_t buffer = 0;

	/* Read the number of pages that the device has */
	sd_raw_read(0, &buffer, 4, current_byte);
	current_byte += 4;

	CuAssertIntEquals(tc, NUMBER_OF_PAGES, buffer);

	/* Read the physical page size */
	sd_raw_read(0, &byte_buffer, 1, current_byte);
	current_byte += 1;

	CuAssertIntEquals(tc, 9, byte_buffer);

	/* Read the block size */
	sd_raw_read(0, &byte_buffer, 1, current_byte);
	current_byte += 1;

	CuAssertIntEquals(tc, 3, byte_buffer);

	/* Read the key size */
	sd_raw_read(0, &byte_buffer, 1, current_byte);
	current_byte += 1;

	CuAssertIntEquals(tc, 1, byte_buffer);

	/* Read the address size */
	sd_raw_read(0, &byte_buffer, 1, current_byte);
	current_byte += 1;

	CuAssertIntEquals(tc, 1, byte_buffer);

	/* Read the state section size */
	sd_raw_read(0, &buffer, 4, current_byte);
	current_byte += 4;

	CuAssertIntEquals(tc, 2, buffer);

	/* Read the directory size */
	buffer = 0;
	sd_raw_read(0, &buffer, 2, current_byte);
	current_byte += 2;

	CuAssertIntEquals(tc, 1, buffer);
}

void
test_tefs_format_device_with_erase(
	CuTest *tc
)
{
	sd_raw_init(CHIP_SELECT_PIN);

	int physical_page_size = 512;
	int block_size = 8;
	int key_size = 1;

	uint8_t byte_buffer = 0;
	uint16_t current_byte;
	for (current_byte = 0; current_byte < 512; current_byte++)
	{
		sd_raw_write(1, &byte_buffer, 1, current_byte);
	}

	sd_raw_flush();

	CuAssertIntEquals(tc, 0, tefs_format_device(NUMBER_OF_PAGES,
												 physical_page_size,
						   						 block_size, key_size, 1));

	for (current_byte = 0; current_byte < 512; current_byte++)
	{
		sd_raw_read(1, &byte_buffer, 1, current_byte);
		CuAssertIntEquals(tc, 0xFF, byte_buffer);
	}

	/* Read and verify the check flag */
	for (current_byte = 0; current_byte < 4; current_byte++)
	{
		sd_raw_read(0, &byte_buffer, 1, current_byte);

		CuAssertIntEquals(tc, TEFS_CHECK_FLAG, byte_buffer);
	}

	uint32_t buffer = 0;

	/* Read the number of pages that the device has */
	sd_raw_read(0, &buffer, 4, current_byte);
	current_byte += 4;

	CuAssertIntEquals(tc, NUMBER_OF_PAGES, buffer);

	/* Read the physical page size */
	sd_raw_read(0, &byte_buffer, 1, current_byte);
	current_byte += 1;

	CuAssertIntEquals(tc, 9, byte_buffer);

	/* Read the block size */
	sd_raw_read(0, &byte_buffer, 1, current_byte);
	current_byte += 1;

	CuAssertIntEquals(tc, 3, byte_buffer);

	/* Read the key size */
	sd_raw_read(0, &byte_buffer, 1, current_byte);
	current_byte += 1;

	CuAssertIntEquals(tc, 1, byte_buffer);

	/* Read the address size */
	sd_raw_read(0, &byte_buffer, 1, current_byte);
	current_byte += 1;

	CuAssertIntEquals(tc, 1, byte_buffer);

	/* Read the state section size */
	sd_raw_read(0, &buffer, 4, current_byte);
	current_byte += 4;

	CuAssertIntEquals(tc, 2, buffer);

	/* Read the directory size */
	buffer = 0;
	sd_raw_read(0, &buffer, 2, current_byte);
	current_byte += 2;

	CuAssertIntEquals(tc, 1, buffer);
}

void test_tefs_open(
	CuTest *tc
)
{
	tefs_t file;
	format_device();
	CuAssertIntEquals(tc, 0, tefs_open(&file, 2));

	uint32_t buffer = 0;

	/* Check the first meta block address */
	sd_raw_read(3, &buffer, 2, 4);
	CuAssertIntEquals(tc, 4, buffer);

	uint8_t byte_buffer = 0;

	/* Check first 3 bits from status section to make sure they are reserved */
	sd_raw_read(1, &byte_buffer, 1, 0);
	CuAssertIntEquals(tc, 0x1F, byte_buffer);

	uint16_t i;
	for (i = 1; i < 512; i++)
	{
		sd_raw_read(1, &byte_buffer, 1, i);
		CuAssertIntEquals(tc, 0xFF, byte_buffer);
	}

	for (i = 0; i < 464; i++)
	{
		sd_raw_read(2, &byte_buffer, 1, i);
		CuAssertIntEquals(tc, 0xFF, byte_buffer);
	}

	/* Check first child meta block address */
	sd_raw_read(4, &buffer, 2, 0);
	CuAssertIntEquals(tc, 12, buffer);

	/* Check first data block address */
	sd_raw_read(12, &buffer, 2, 0);
	CuAssertIntEquals(tc, 20, buffer);
}

void test_tefs_remove(
	CuTest *tc
)
{
	tefs_t file;
	format_device();
	CuAssertIntEquals(tc, 0, tefs_open(&file, 2));
	CuAssertIntEquals(tc, 0, tefs_remove(2));

	uint32_t buffer = 0;

	/* Check directory to see if address for file ID has been removed */
	sd_raw_read(3, &buffer, 4, 4);
	CuAssertIntEquals(tc, TEFS_DELETED, buffer);

	/* Check state section to see if pages were released */
	sd_raw_read(1, &buffer, 4, 0);
	CuAssertIntEquals(tc, 0xFFFFFFFF, buffer);
}

void test_tefs_write(
	CuTest *tc
)
{
	tefs_t file;
	format_device();
	CuAssertIntEquals(tc, 0, tefs_open(&file, 2));

	populate_data_array_1();

	/* Write a single page and check if it has been written */
	CuAssertIntEquals(tc, 0, tefs_write(&file, 0, data, 512, 0));

	uint8_t buffer[27];
	CuAssertIntEquals(tc, 0, sd_raw_read(20, buffer, 27, 2));

	uint32_t i;
	for (i = 0; i < 27; i++)
	{
		CuAssertIntEquals(tc, data[i + 2], buffer[i]);
	}

	/* Write multiple pages but stay within a single meta block */
	for (i = 1; i < 50; i++)
	{
		CuAssertIntEquals(tc, 0, tefs_write(&file, i, data, 512, 0));
		CuAssertIntEquals(tc, 0, sd_raw_read(20 + i, buffer, 27, 2));

		uint8_t j;
		for (j = 0; j < 27; j++)
		{
			CuAssertIntEquals(tc, data[j + 2], buffer[j]);
		}
	}

	/* Write multiple pages with meta block overflow */
	for (i = 50; i < 16385 * 2; i++)
	{
		CuAssertIntEquals(tc, 0, tefs_write(&file, i, data, 512, 0));
		CuAssertIntEquals(tc, 0, sd_raw_read(20 + i + (i / (16384)) * 8, buffer, 27, 2));

		uint8_t j;
		for (j = 0; j < 27; j++)
		{
			CuAssertIntEquals(tc, data[j + 2], buffer[j]);
		}
	}
}

void test_tefs_read_after_write(
	CuTest *tc
)
{
	tefs_t file;
	format_device();
	CuAssertIntEquals(tc, 0, tefs_open(&file, 2));

	populate_data_array_1();

	/* Write a single page and read it */
	CuAssertIntEquals(tc, 0, tefs_write(&file, 0, data, 512, 0));

	uint8_t buffer[27];
	CuAssertIntEquals(tc, 0, tefs_read(&file, 0, buffer, 27, 2));

	uint16_t i;
	for (i = 0; i < 27; i++)
	{
		CuAssertIntEquals(tc, data[i + 2], buffer[i]);
	}

	/* Write and read multiple pages but stay within a single meta block */
	for (i = 1; i < 50; i++)
	{
		CuAssertIntEquals(tc, 0, tefs_write(&file, i, data, 512, 0));
		CuAssertIntEquals(tc, 0, tefs_read(&file, i, buffer, 27, 2));

		uint8_t j;
		for (j = 0; j < 27; j++)
		{
			CuAssertIntEquals(tc, data[j + 2], buffer[j]);
		}
	}

	/* Write and read multiple pages with meta block overflow */
	for (i = 50; i < 16385 * 2; i++)
	{
		CuAssertIntEquals(tc, 0, tefs_write(&file, i, data, 512, 0));
		CuAssertIntEquals(tc, 0, tefs_read(&file, i, buffer, 27, 2));

		uint8_t j;
		for (j = 0; j < 27; j++)
		{
			CuAssertIntEquals(tc, data[j + 2], buffer[j]);
		}
	}
}

void test_tefs_release_block(
	CuTest *tc
)
{
	tefs_t file;
	format_device();
	CuAssertIntEquals(tc, 0, tefs_open(&file, 2));

	uint16_t i;
	populate_data_array_1();

	/* Write and read multiple pages */
	for (i = 0; i < 100; i++)
	{
		CuAssertIntEquals(tc, 0, tefs_write(&file, i, data, 512, 0));
	}

	/* Release the pages */
	for (i = 0; i < 2; i++)
	{
		CuAssertIntEquals(tc, 0, tefs_release_block(&file, i * 8));
	}
	
	/* Check if the state section has the right values as well as the meta block */
	uint8_t buffer;
	CuAssertIntEquals(tc, 0, sd_raw_read(1, &buffer, 1, 0));
	CuAssertIntEquals(tc, 0x30, buffer);
	CuAssertIntEquals(tc, 0, sd_raw_read(1, &buffer, 1, 1));
	CuAssertIntEquals(tc, 0x01, buffer);
	CuAssertIntEquals(tc, 0, sd_raw_read(1, &buffer, 1, 2));
	CuAssertIntEquals(tc, 0xFF, buffer);

	uint16_t address;
	CuAssertIntEquals(tc, 0, sd_raw_read(12, &address, 2, 0));
	CuAssertIntEquals(tc, TEFS_DELETED, address);

	/* Release the rest of the pages */
	for (i = 2; i < (100 / 8) + 1; i++)
	{
		CuAssertIntEquals(tc, 0, tefs_release_block(&file, i * 8));
	}

	/* Check if state section, meta block, and child meta block are correct */
	CuAssertIntEquals(tc, 0, sd_raw_read(1, &buffer, 1, 0));
	CuAssertIntEquals(tc, 0x7F, buffer);
	CuAssertIntEquals(tc, 0, sd_raw_read(1, &buffer, 1, 1));
	CuAssertIntEquals(tc, 0xFF, buffer);
	CuAssertIntEquals(tc, 0, sd_raw_read(1, &buffer, 1, 2));
	CuAssertIntEquals(tc, 0xFF, buffer);

	CuAssertIntEquals(tc, 0, sd_raw_read(4, &address, 2, 0));
	CuAssertIntEquals(tc, TEFS_DELETED, address);
}

void test_tefs_multiple_files(
	CuTest *tc
)
{
	tefs_t file1;
	tefs_t file2;
	format_device();

	CuAssertIntEquals(tc, 0, tefs_open(&file1, 1));
	CuAssertIntEquals(tc, 0, tefs_open(&file2, 2));

	/* Check directory for the files */
	uint16_t address_buffer = 0;

	sd_raw_read(3, &address_buffer, 2, 2);
	CuAssertIntEquals(tc, 4, address_buffer);
	sd_raw_read(3, &address_buffer, 2, 4);
	CuAssertIntEquals(tc, 28, address_buffer);

	/* Check first 6 bits from status section to make sure they are reserved */
	uint8_t byte_buffer = 0;

	sd_raw_read(1, &byte_buffer, 1, 0);
	CuAssertIntEquals(tc, 0x03, byte_buffer);

	uint16_t i;
	for (i = 1; i < 512; i++)
	{
		sd_raw_read(1, &byte_buffer, 1, i);
		CuAssertIntEquals(tc, 0xFF, byte_buffer);
	}

	/* Write multiple pages to file 1 */
	populate_data_array_1();

	for (i = 0; i < 100; i++)
	{
		CuAssertIntEquals(tc, 0, tefs_write(&file1, i, data, 512, 0));
	}

	populate_data_array_2();

	/* Write multiple pages to file 2 */
	for (i = 0; i < 100; i++)
	{
		CuAssertIntEquals(tc, 0, tefs_write(&file2, i, data, 512, 0));
	}

	uint8_t buffer[27];
	populate_data_array_1();

	/* Read the pages */
	for (i = 0; i < 100; i++)
	{
		CuAssertIntEquals(tc, 0, tefs_read(&file1, i, buffer, 27, 2));

		uint8_t j;
		for (j = 0; j < 27; j++)
		{
			CuAssertIntEquals(tc, data[j + 2], buffer[j]);
		}
	}

	populate_data_array_2();

	for (i = 0; i < 100; i++)
	{
		CuAssertIntEquals(tc, 0, tefs_read(&file2, i, buffer, 27, 2));

		uint8_t j;
		for (j = 0; j < 27; j++)
		{
			CuAssertIntEquals(tc, data[j + 2], buffer[j]);
		}
	}

	/* Remove files */
	CuAssertIntEquals(tc, 0, tefs_remove(1));
	CuAssertIntEquals(tc, 0, tefs_remove(2));

	sd_raw_read(3, &address_buffer, 2, 2);
	CuAssertIntEquals(tc, 1, address_buffer);
	sd_raw_read(3, &address_buffer, 2, 4);
	CuAssertIntEquals(tc, 1, address_buffer);
}

void test_tefs_sequential_read_and_write(
	CuTest *tc
)
{
	tefs_t file;
	format_device();
	CuAssertIntEquals(tc, 0, tefs_open(&file, 2));

	populate_data_array_1();

	CuAssertIntEquals(tc, 0, tefs_write_continuous_start(&file, 0));

	uint8_t buffer[27];
	uint16_t i;

	/* Write and read multiple pages */
	for (i = 0; i < 16385 * 2; i++)
	{
		CuAssertIntEquals(tc, 0, tefs_write_continuous(&file, data, 512, 0));
		CuAssertIntEquals(tc, 0, tefs_write_continuous_next(&file));
	}

	CuAssertIntEquals(tc, 0, tefs_write_continuous_stop(&file));

	CuAssertIntEquals(tc, 0, tefs_read_continuous_start(&file, 0));

	for (i = 0; i < 16385 * 2; i++)
	{
		CuAssertIntEquals(tc, 0, tefs_read_continuous(&file, buffer, 27, 2));

		uint8_t j;
		for (j = 0; j < 27; j++)
		{
			CuAssertIntEquals(tc, data[2 + j], buffer[j]);
		}

		CuAssertIntEquals(tc, 0, tefs_read_continuous_next(&file));
	}

	CuAssertIntEquals(tc, 0, tefs_read_continuous_stop(&file));
}

CuSuite*
tefs_getsuite(
	void
)
{
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_tefs_format_device);
	SUITE_ADD_TEST(suite, test_tefs_format_device_with_erase);
	SUITE_ADD_TEST(suite, test_tefs_open);
	SUITE_ADD_TEST(suite, test_tefs_remove);
	SUITE_ADD_TEST(suite, test_tefs_write);
	SUITE_ADD_TEST(suite, test_tefs_read_after_write);
	SUITE_ADD_TEST(suite, test_tefs_release_block);
	SUITE_ADD_TEST(suite, test_tefs_multiple_files);
	SUITE_ADD_TEST(suite, test_tefs_sequential_read_and_write);

	return suite;
}

void
runalltests_tefs(
	void
)
{
	CuString	*output	= CuStringNew();
	CuSuite		*suite	= tefs_getsuite();

	if (sd_raw_init(CHIP_SELECT_PIN))
	{
		printf("SD Card (file) initialization failure.");
		return;
	}

	CuSuiteRun(suite);
	CuSuiteSummary(suite, output);
	CuSuiteDetails(suite, output);
	printf("%s\n", output->buffer);

	CuSuiteDelete(suite);
	CuStringDelete(output);
}

int
main(
	void
)
{
	runalltests_tefs();
	return 0;
}
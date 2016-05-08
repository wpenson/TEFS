/******************************************************************************/
/**
@file		test_tefs.c
@author     Wade Penson
@date		June, 2015
@brief      Unit tests for TEFS.

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

#include "planck_unit/src/planckunit.h"
#include "../src/tefs/tefs.h"

#define CHIP_SELECT_PIN 4

static uint8_t	data[512];
static uint8_t	buffer[512];
static uint8_t	address_size;
static uint32_t state_section_size;
static uint32_t state_section_size_in_bytes;
static uint8_t	info_section_size;
static uint32_t i;
static uint32_t j;
static uint32_t four_byte_buffer;
static uint8_t	byte_buffer;
static uint16_t current_byte;
static uint32_t current_page;
static char		*memory_error = "Ran out of memory";

typedef struct {
	char	*name;
	int32_t hash;
	file_t	*file;
} file_info_t;

static file_info_t files[] =
{
	{"test.aaa", -530280420},
	{"test.bbb", 0}
};

typedef struct {
	uint32_t	num_pages;
	uint16_t	page_size;
	uint16_t	block_size;
	uint8_t		hash_size;
	uint16_t	meta_data_size;
	uint16_t	max_file_name_size;
} format_info_t;

static format_info_t *format_info;

#if defined(USE_SD)
static format_info_t format_info_arr[] =
{
	{62500, 512, 8, 4, 32, 12},
//	{62500, 512, 64, 4, 32, 12},
//	{1000, 512, 1, 4, 16, 10}
};
#elif defined(USE_DATAFLASH)
static format_info_t format_info_arr[] =
{
	{1000, 512, 8, 4, 32, 12}
};
#endif

static const int powers_of_two_arr[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024,
                                        2048, 4096, 8192, 16384, 32768, 65536};

static uint8_t
find_power_of_2_exp(
	uint32_t val
)
{
	for (i = 0; i < 17; i++)
	{
		if (powers_of_two_arr[i] == val)
		{
			return (uint8_t) i;
		}
	}

	return 0;
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

	if (format_info->hash_size == 4)
	{
		return hash;
	}
	else
	{
		return hash % 65521;
	}
}

static uint32_t
get_block_address(
	uint32_t block_number
)
{
	return block_number * format_info->block_size + state_section_size + info_section_size;
}

static int8_t
check_state_section(
	planck_unit_test_t	*tc,
	uint32_t 			start_bit,
	uint32_t 			end_bit
)
{
#if defined(USE_SD)
	uint32_t start_byte = start_bit / 8;
	uint32_t end_byte	= end_bit / 8;

	/* Check state section to see if pages were released */
	uint8_t b = 0;
	uint8_t s = 0;
	for (current_page = 0;
		 current_page < state_section_size;
		 current_page++)
	{
		for (current_byte = 0;
			 current_byte < format_info->page_size;
			 current_byte++)
		{
 			if (start_byte == current_byte + current_page * format_info->page_size &&
				start_byte / format_info->page_size == current_page)
			{
				b = 0xFF;
				for (i = 1; i <= 1 << (7 - (start_bit % 8)); i <<= 1)
				{
					b ^= i;
				}

				s = 1;

				if (start_byte == end_byte)
				{
					for (i = 1; i <= 1 << (7 - (end_bit % 8)); i <<= 1)
					{
						b |= i;
					}

					s = 0;
				}
			}
			else if (end_byte == current_byte + current_page * format_info->page_size &&
					 end_byte / format_info->page_size == current_page)
			{
				b = 0;
				for (i = 1; i <= 1 << (7 - (end_bit % 8)); i <<= 1)
				{
					b |= i;
				}

				s = 0;
			}
			else if (s == 1 || (current_page == state_section_size - 1 &&
					 current_byte >= state_section_size_in_bytes % format_info->page_size))
			{
				b = 0;
			}
			else if (s == 0)
			{
				b = 0xFF;
			}

			device_read(current_page + info_section_size, &byte_buffer, 1, current_byte);

			if (b != byte_buffer)
			{
				return -1;
			}
		}
	}
#endif

	return 0;
}

static void
populate_data_array_1(
	void
)
{
	for (i = 0; i < 26; i++)
	{
		data[i] = (uint8_t) ('a' + i);
	}

	for (i = 26; i < 512; i++)
	{
		data[i] = '.';
	}
}

static void
populate_data_array_2(
	void
)
{
	for (i = 0; i < 26; i++)
	{
		data[i] = (uint8_t) ('A' + i);
	}

	for (i = 26; i < 512; i++)
	{
		data[i] = '!';
	}
}

static int8_t
format_device(
	void
)
{
#if defined(USE_DATAFLASH) && defined(USE_FTL)
	ftl_Instantiate(&ftl);
	flare_Createftl(&ftl);
#endif

	return tefs_format_device(format_info->num_pages,
							  format_info->page_size,
							  format_info->block_size,
							  format_info->hash_size,
							  format_info->meta_data_size,
							  format_info->max_file_name_size,
							  1);
}

void
test_tefs_format_device_helper(
	planck_unit_test_t  *tc,
	uint8_t             pre_erase
)
{
#if defined(USE_DATAFLASH) && defined(USE_FTL)
	ftl_Instantiate(&ftl);
	flare_Createftl(&ftl);
#endif

#if defined(USE_SD)
	/* Write out dummy data to help check if format wrote the correct data. */
    byte_buffer = 0xEA;
	for (current_page = 0; current_page < 1000; current_page++)
	{
		for (current_byte = 0; current_byte < format_info->page_size; current_byte++)
		{
			device_write(current_page, &byte_buffer, 1, current_byte);
		}
	}

	device_flush();
#endif

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_format_device(format_info->num_pages,
															   format_info->page_size,
															   format_info->block_size,
															   format_info->hash_size,
															   format_info->meta_data_size,
															   format_info->max_file_name_size,
															   pre_erase));

	/* Read and verify the check flag */
	for (current_byte = 0; current_byte < 4; current_byte++)
	{
		device_read(0, &byte_buffer, 1, current_byte);
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0xFC, byte_buffer);
	}

	/* Read the number of pages that the device has */
    four_byte_buffer = 0;
	device_read(0, &four_byte_buffer, 4, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, format_info->num_pages, four_byte_buffer);
	current_byte += 4;

	/* Read the physical page size */
	device_read(0, &byte_buffer, 1, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, find_power_of_2_exp(format_info->page_size), byte_buffer);
	current_byte += 1;

	/* Read the block size */
	device_read(0, &byte_buffer, 1, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, find_power_of_2_exp(format_info->block_size), byte_buffer);
	current_byte += 1;

	/* Read the address size */
	device_read(0, &byte_buffer, 1, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, find_power_of_2_exp(address_size), byte_buffer);
	current_byte += 1;

	/* Read the hash value size */
	device_read(0, &byte_buffer, 1, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, format_info->hash_size, byte_buffer);
	current_byte += 1;

	/* Read the meta data record size */
    four_byte_buffer = 0;
	device_read(0, &four_byte_buffer, 2, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, format_info->meta_data_size, four_byte_buffer);
	current_byte += 2;

	/* Read the max name file size */
    four_byte_buffer = 0;
	device_read(0, &four_byte_buffer, 2, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, format_info->max_file_name_size, four_byte_buffer);
	current_byte += 2;

#if defined(USE_SD)
	/* Read the state section size */
	four_byte_buffer = 0;
	device_read(0, &four_byte_buffer, 4, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, state_section_size, four_byte_buffer);
	current_byte += 4;
#endif

	/* Read the meta data for the hash file */
	four_byte_buffer = 0;
	device_read(0, &four_byte_buffer, TEFS_DIR_EOF_PAGE_SIZE, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, four_byte_buffer);
	current_byte += 4;

	four_byte_buffer = 0;
	device_read(0, &four_byte_buffer, TEFS_DIR_EOF_BYTE_SIZE, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, four_byte_buffer);
	current_byte += 2;

	four_byte_buffer = 0;
	device_read(0, &four_byte_buffer, 4, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, get_block_address(0), four_byte_buffer);
	current_byte += 4;

	/* Read the meta data for the meta data file */
	four_byte_buffer = 0;
	device_read(0, &four_byte_buffer, TEFS_DIR_EOF_PAGE_SIZE, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, four_byte_buffer);
	current_byte += 4;

	four_byte_buffer = 0;
	device_read(0, &four_byte_buffer, TEFS_DIR_EOF_BYTE_SIZE, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, four_byte_buffer);
	current_byte += 2;

	four_byte_buffer = 0;
	device_read(0, &four_byte_buffer, 4, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, get_block_address(2), four_byte_buffer);
	current_byte += 4;

	/* Check if the rest of the info section has zeros. */
	while(current_byte < format_info->page_size)
	{
		device_read(0, &byte_buffer, 1, current_byte);
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, byte_buffer);

		current_byte++;
	}

	if (check_state_section(tc, 0, 4))
	{
		PLANCK_UNIT_ASSERT_FALSE(tc);
	}
}

void
test_tefs_format_device_without_erase(
	planck_unit_test_t *tc
)
{
	test_tefs_format_device_helper(tc, 0);
}

void
test_tefs_format_device_with_erase(
	planck_unit_test_t *tc
)
{
	test_tefs_format_device_helper(tc, 1);
}

void
test_tefs_create_single_file(
	planck_unit_test_t *tc
)
{
	files[0].file = malloc(sizeof(file_t));

	if (files[0].file == NULL)
	{
		printf("%s\n", memory_error);
		exit(-1);
	}

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, format_device());
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(files[0].file, files[0].name));

	/* Check the hash value in the hash entries file */
    four_byte_buffer = 0;
	device_read(get_block_address(1), &four_byte_buffer, 4, 0);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, files[0].hash, four_byte_buffer);

	/* Check the status byte in the meta data file */
	four_byte_buffer = 0;
	device_read(get_block_address(3), &four_byte_buffer, 1, 0);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 2, four_byte_buffer);

	current_byte = TEFS_DIR_STATUS_SIZE;

	/* Check the file size in the directory entry */
    four_byte_buffer = 0;
	device_read(get_block_address(3), &four_byte_buffer, 4, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, four_byte_buffer);

	current_byte += TEFS_DIR_EOF_PAGE_SIZE;

	four_byte_buffer = 0;
	device_read(get_block_address(3), &four_byte_buffer, 2, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, four_byte_buffer);

	current_byte += TEFS_DIR_EOF_BYTE_SIZE;

	/* Check the first index block address in directory entry */
    four_byte_buffer = 0;
	device_read(get_block_address(3), &four_byte_buffer, 4, current_byte);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, get_block_address(4), four_byte_buffer);

	current_byte += TEFS_DIR_ROOT_INDEX_ADDRESS_SIZE;

	/* Check the file name in the meta data file */
	char name[format_info->max_file_name_size];
	device_read(get_block_address(3), name, format_info->max_file_name_size, current_byte);

	for (i = 0; i < format_info->max_file_name_size; i++)
	{
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, name[i], files[0].name[i]);

		if (files[0].name[i] == '\0')
		{
			while (i < format_info->max_file_name_size)
			{
				PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, name[i], '\0');
				i++;
			}
		}
	}

	if (check_state_section(tc, 0, 6))
	{
		PLANCK_UNIT_ASSERT_FALSE(tc);
	}

	/* Check first data block address */
    four_byte_buffer = 0;
	device_read(get_block_address(4), &four_byte_buffer, 2, 0);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, get_block_address(5), four_byte_buffer);

	free(files[0].file);
}

void
test_tefs_create_multiple_files(
	planck_unit_test_t *tc
)
{
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, format_device());

	uint16_t num_hash_blocks = 0;
	uint16_t file_num;
	for (file_num = 0; file_num < 100; file_num++)
	{
		file_t *file = malloc(sizeof(file_t));

		if (file == NULL)
		{
			printf("%s\n", memory_error);
			exit(-1);
		}

		char file_name[10];
		sprintf(file_name, "%s%d", "file.", file_num);

		uint32_t file_hash = hash_string(file_name);

		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(file, file_name));
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_close(file));

		uint32_t num_hashes_per_block = (format_info->page_size * format_info->block_size / format_info->hash_size);
		uint32_t num_meta_entries_per_block = (format_info->page_size * format_info->block_size / format_info->meta_data_size);

		uint32_t hash_entry_page = (file_num * format_info->hash_size) / format_info->page_size;
		hash_entry_page += get_block_address(1) + (file_num / num_hashes_per_block) * num_hashes_per_block * 2 * format_info->block_size;

		if (file_num > 0 && (file_num % num_hashes_per_block) == 0)
		{
			num_hash_blocks += 1;
		}

		hash_entry_page += num_hash_blocks * 8 * format_info->block_size;
		hash_entry_page += (num_hash_blocks > 0) ? format_info->block_size : 0;

		uint16_t hash_entry_byte = (file_num * format_info->hash_size) % format_info->page_size;

	   	/* Check the hash value in the hash entries file */
		four_byte_buffer = 0;
		device_read(hash_entry_page, &four_byte_buffer, 4, hash_entry_byte);
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, file_hash, four_byte_buffer);

		uint32_t meta_data_page = (file_num * format_info->meta_data_size) / format_info->page_size;
		meta_data_page += get_block_address(3) + (file_num / num_meta_entries_per_block) * num_meta_entries_per_block * 2 * format_info->block_size;
		meta_data_page += (file_num / num_hashes_per_block) * format_info->block_size;

		uint16_t meta_data_byte = (file_num * format_info->meta_data_size) % format_info->page_size;

		/* Check the status byte in the meta data file */
		four_byte_buffer = 0;
		device_read(meta_data_page, &four_byte_buffer, 1, meta_data_byte);
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 2, four_byte_buffer);

		/* Check the file name in the meta data file */
		char name[format_info->max_file_name_size];
		device_read(meta_data_page, name, format_info->max_file_name_size, meta_data_byte + TEFS_DIR_STATIC_DATA_SIZE);

		for (i = 0; i < format_info->max_file_name_size; i++)
		{
			PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, name[i], file_name[i]);

			if (file_name[i] == '\0')
			{
				while (i < format_info->max_file_name_size)
				{
					PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, name[i], '\0');
					i++;
				}
			}
		}

		/* Check the file size in the directory entry */
		four_byte_buffer = 0;
		device_read(meta_data_page, &four_byte_buffer, 4, meta_data_byte + TEFS_DIR_STATUS_SIZE);
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, four_byte_buffer);

		four_byte_buffer = 0;
		device_read(meta_data_page, &four_byte_buffer, 2, meta_data_byte + TEFS_DIR_STATUS_SIZE + TEFS_DIR_EOF_PAGE_SIZE);
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, four_byte_buffer);

		uint32_t temp_block = 4 + file_num * 2 + (file_num / num_meta_entries_per_block) + (file_num / num_hashes_per_block);

		/* Check the first index block address in directory entry */
		four_byte_buffer = 0;
		device_read(meta_data_page, &four_byte_buffer, TEFS_DIR_ROOT_INDEX_ADDRESS_SIZE,
					meta_data_byte + TEFS_DIR_STATUS_SIZE + TEFS_DIR_EOF_PAGE_SIZE + TEFS_DIR_EOF_BYTE_SIZE);
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, get_block_address(temp_block), four_byte_buffer);

		if (check_state_section(tc, 0, temp_block + 2))
		{
			PLANCK_UNIT_ASSERT_FALSE(tc);
		}

		/* Check first data block address */
		four_byte_buffer = 0;
		device_read(get_block_address(temp_block), &four_byte_buffer, 2, 0);
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, get_block_address(temp_block + 1), four_byte_buffer);

		free(file);
	}
}

void
test_tefs_remove_files_consecutively_helper(
	planck_unit_test_t 	*tc,
	uint32_t			num_files,
	uint32_t 			num_pages
)
{
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, format_device());
	populate_data_array_1();

	uint16_t num_hash_blocks = 0;
	uint16_t file_num;
	for (file_num = 0; file_num < num_files; file_num++)
	{
		file_t *file = malloc(sizeof(file_t));

		if (file == NULL)
		{
			printf("%s\n", memory_error);
			exit(-1);
		}

		char file_name[10];
		sprintf(file_name, "%s%d", "file.", file_num);

		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(file, file_name));

		/* Write num_pages of data to the file */
		for (i = 0; i < num_pages; i++)
		{
			PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(file, i, data, format_info->page_size, 0));
		}

		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_close(file));

		free(file);
	}

	for (file_num = 0; file_num < num_files; file_num++)
	{
		char file_name[10];
		sprintf(file_name, "%s%d", "file.", file_num);

		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_remove(file_name));

		uint32_t num_hashes_per_block = (format_info->page_size * format_info->block_size / format_info->hash_size);
		uint32_t num_meta_entries_per_block = (format_info->page_size * format_info->block_size / format_info->meta_data_size);

		uint32_t hash_entry_page = (file_num * format_info->hash_size) / format_info->page_size;
		hash_entry_page += get_block_address(1) + (file_num / num_hashes_per_block) * num_hashes_per_block * 2 * format_info->block_size;

		if (file_num > 0 && (file_num % num_hashes_per_block) == 0)
		{
			num_hash_blocks += 1;
		}

		hash_entry_page += num_hash_blocks * 8 * format_info->block_size;
		hash_entry_page += (num_hash_blocks > 0) ? format_info->block_size : 0;

		uint16_t hash_entry_byte = (file_num * format_info->hash_size) % format_info->page_size;

		uint32_t meta_data_page = (file_num * format_info->meta_data_size) / format_info->page_size;
		meta_data_page += get_block_address(3) + (file_num / num_meta_entries_per_block) * num_meta_entries_per_block * 2 * format_info->block_size;
		meta_data_page += (file_num / num_hashes_per_block) * format_info->block_size;

		uint16_t meta_data_byte = (file_num * format_info->meta_data_size) % format_info->page_size;

		/* Check the hash to see if it has been set to 0. */
		four_byte_buffer = 0;
		device_read(hash_entry_page, &four_byte_buffer, 4, hash_entry_byte);
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, four_byte_buffer);

		/* Check directory entry to see if file status has been set to deleted. */
		four_byte_buffer = 0;
		device_read(meta_data_page, &four_byte_buffer, 1, meta_data_byte);
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, TEFS_DELETED, four_byte_buffer);
	}

	/* Note: This test will fail if the hash entries or meta data file wrote past a single data block! */
	if (check_state_section(tc, 0, 4))
	{
		PLANCK_UNIT_ASSERT_FALSE(tc);
	}
}

void
test_tefs_remove_files_staggered_helper(
	planck_unit_test_t 	*tc,
	uint32_t			num_files,
	uint32_t 			num_pages
)
{
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, format_device());
	populate_data_array_1();

	uint16_t file_num;
	for (file_num = 0; file_num < num_files; file_num++)
	{
		file_t *file = malloc(sizeof(file_t));

		if (file == NULL)
		{
			printf("%s\n", memory_error);
			exit(-1);
		}

		char file_name[10];
		sprintf(file_name, "%s%d", "file.", file_num);

		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(file, file_name));

		/* Write num_pages of data to the file */
		for (i = 0; i < num_pages; i++)
		{
			PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(file, i, data, format_info->page_size, 0));
		}

		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_close(file));
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_remove(file_name));

		/* Check the hash to see if it has been set to 0. */
		four_byte_buffer = 0;
		device_read(get_block_address(1), &four_byte_buffer, 4, 0);
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, four_byte_buffer);

		/* Check directory entry to see if file status has been set to deleted. */
		four_byte_buffer = 0;
		device_read(get_block_address(3), &four_byte_buffer, 1, 0);
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, TEFS_DELETED, four_byte_buffer);

		free(file);
	}

	if (check_state_section(tc, 0, 4))
	{
		PLANCK_UNIT_ASSERT_FALSE(tc);
	}
}

void
test_tefs_remove_empty_single_file(
	planck_unit_test_t *tc
)
{
	test_tefs_remove_files_consecutively_helper(tc, 1, 0);
}

void
test_tefs_remove_small_single_file(
	planck_unit_test_t *tc
)
{
	test_tefs_remove_files_consecutively_helper(tc, 1, 1);
}

void
test_tefs_remove_large_single_file(
	planck_unit_test_t *tc
)
{
	test_tefs_remove_files_consecutively_helper(tc, 1, 100);
}

void
test_tefs_remove_multiple_empty_files_consecutively(
	planck_unit_test_t *tc
)
{
	test_tefs_remove_files_consecutively_helper(tc, 100, 0);
}

void
test_tefs_remove_multiple_files_with_data_consecutively(
	planck_unit_test_t *tc
)
{
	test_tefs_remove_files_consecutively_helper(tc, 100, 100);
}

void
test_tefs_remove_multiple_empty_files_staggered(
	planck_unit_test_t *tc
)
{
	test_tefs_remove_files_staggered_helper(tc, 100, 0);
}

void
test_tefs_remove_multiple_files_with_data_staggered(
	planck_unit_test_t *tc
)
{
	test_tefs_remove_files_staggered_helper(tc, 100, 100);
}

void
test_tefs_exists_single_file(
	planck_unit_test_t *tc
)
{
	files[0].file = malloc(sizeof(file_t));

	if (files[0].file == NULL)
	{
		printf("%s\n", memory_error);
		exit(-1);
	}

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, format_device());

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_exists(files[0].name));
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(files[0].file, files[0].name));
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 1, tefs_exists(files[0].name));

	free(files[0].file);
}

void
test_tefs_write_page_to_single_file(
    planck_unit_test_t *tc
)
{
	files[0].file = malloc(sizeof(file_t));

	if (files[0].file == NULL)
	{
		printf("%s\n", memory_error);
		exit(-1);
	}

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, format_device());
    PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(files[0].file, files[0].name));

    populate_data_array_1();

    /* Write a single page and check if it has been written */
    PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(files[0].file, 0, data, format_info->page_size, 0));
    PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(get_block_address(5), buffer, format_info->page_size, 0));

    for (i = 0; i < format_info->page_size; i++)
    {
        PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[i], buffer[i]);
    }

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_close(files[0].file));

    uint32_t size = 0;
    device_read(get_block_address(3), &size, 4, TEFS_DIR_STATUS_SIZE);
    PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 1, size);

    size = 0;
    device_read(get_block_address(3), &size, 2, TEFS_DIR_STATUS_SIZE + TEFS_DIR_EOF_PAGE_SIZE);
    PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, size);

	if (check_state_section(tc, 0, 6))
	{
		PLANCK_UNIT_ASSERT_FALSE(tc);
	}

	free(files[0].file);
}

void
test_tefs_write_data_block_to_single_file(
	planck_unit_test_t *tc
)
{
	files[0].file = malloc(sizeof(file_t));

	if (files[0].file == NULL)
	{
		printf("%s\n", memory_error);
		exit(-1);
	}

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, format_device());
    PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(files[0].file, files[0].name));

    populate_data_array_1();

    /* Write multiple pages but stay within a single data block */
    for (i = 0; i < format_info->block_size; i++)
    {
        PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(files[0].file, i, data, format_info->page_size, 0));
        PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(get_block_address(5) + i, buffer, format_info->page_size, 0));

        for (j = 0; j < format_info->page_size; j++)
        {
            PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[j], buffer[j]);
        }

        PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_flush(files[0].file));
    }

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_close(files[0].file));

    uint32_t size = 0;
    device_read(get_block_address(3), &size, 4, TEFS_DIR_STATUS_SIZE);
    PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, format_info->block_size, size);

    size = 0;
    device_read(get_block_address(3), &size, 2, TEFS_DIR_STATUS_SIZE + TEFS_DIR_EOF_PAGE_SIZE);
    PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, size);

	if (check_state_section(tc, 0, 6))
	{
		PLANCK_UNIT_ASSERT_FALSE(tc);
	}

	free(files[0].file);
}

void
test_tefs_write_child_block_to_single_file(
    planck_unit_test_t *tc
)
{
	files[0].file = malloc(sizeof(file_t));

	if (files[0].file == NULL)
	{
		printf("%s\n", memory_error);
		exit(-1);
	}

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, format_device());
    PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(files[0].file, files[0].name));

    populate_data_array_1();

    uint32_t pages_per_child_block = format_info->page_size * format_info->block_size / address_size * format_info->block_size;
    uint32_t num_pages = (pages_per_child_block < format_info->num_pages) ? pages_per_child_block : format_info->num_pages; // TODO

    for (i = 0; i < num_pages; i++)
    {
        PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(files[0].file, i, data, format_info->page_size, 0));
        PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(get_block_address(5) + i, buffer, format_info->page_size, 0));

        for (j = 0; j < format_info->page_size; j++)
        {
            PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[j], buffer[j]);
        }

        PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_flush(files[0].file));
    }

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_close(files[0].file));

    uint32_t size = 0;
    device_read(get_block_address(3), &size, 4, TEFS_DIR_STATUS_SIZE);
    PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, num_pages, size);

    size = 0;
    device_read(get_block_address(3), &size, 2, TEFS_DIR_STATUS_SIZE + TEFS_DIR_EOF_PAGE_SIZE);
    PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, size);

	if (check_state_section(tc, 0, 5 + num_pages / format_info->block_size))
	{
		PLANCK_UNIT_ASSERT_FALSE(tc);
	}

	free(files[0].file);
}

void
test_tefs_write_multiple_child_blocks_to_single_file(
    planck_unit_test_t *tc
)
{
	files[0].file = malloc(sizeof(file_t));

	if (files[0].file == NULL)
	{
		printf("%s\n", memory_error);
		exit(-1);
	}

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, format_device());
    PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(files[0].file, files[0].name));

    populate_data_array_1();

    uint32_t pages_per_child_block = format_info->page_size * format_info->block_size / address_size * format_info->block_size;
    uint32_t num_pages = (pages_per_child_block < format_info->num_pages) ? pages_per_child_block : format_info->num_pages; // TODO

    for (i = 0; i < num_pages; i++)
    {
        PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(files[0].file, i, data, format_info->page_size, 0));
        PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(get_block_address(5) + i, buffer, format_info->page_size, 0));

        for (j = 0; j < format_info->page_size; j++)
        {
            PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[j], buffer[j]);
        }

        PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_flush(files[0].file));
    }

	for (i = num_pages; i < num_pages * 2; i++)
	{
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(files[0].file, i, data, format_info->page_size, 0));
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(get_block_address(5) + i + format_info->block_size * 2, buffer, format_info->page_size, 0));

		for (j = 0; j < format_info->page_size; j++)
		{
			PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[j], buffer[j]);
		}

		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_flush(files[0].file));
	}

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_close(files[0].file));

    uint32_t size = 0;
    device_read(get_block_address(3), &size, 4, TEFS_DIR_STATUS_SIZE);
    PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, num_pages * 2, size);

    size = 0;
    device_read(get_block_address(3), &size, 2, TEFS_DIR_STATUS_SIZE + TEFS_DIR_EOF_PAGE_SIZE);
    PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, size);

	if (check_state_section(tc, 0, 5 + num_pages * 2 / format_info->block_size + 2))
	{
		PLANCK_UNIT_ASSERT_FALSE(tc);
	}

	free(files[0].file);
}

void
test_tefs_read_after_write_to_single_file(
	planck_unit_test_t *tc
)
{
	files[0].file = malloc(sizeof(file_t));

	if (files[0].file == NULL)
	{
		printf("%s\n", memory_error);
		exit(-1);
	}

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, format_device());
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(files[0].file, files[0].name));

	populate_data_array_1();

	uint32_t pages_per_child_block = format_info->page_size * format_info->block_size / address_size * format_info->block_size;
	uint32_t num_pages = (pages_per_child_block < format_info->num_pages) ? pages_per_child_block : format_info->num_pages; // TODO

	for (i = 0; i < num_pages; i++)
	{
		/* Write a page then read it */
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(files[0].file, i, data, format_info->page_size, 0));
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_read(files[0].file, i, buffer, format_info->page_size, 0));

		for (j = 0; j < format_info->page_size; j++)
		{
			PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[j], buffer[j]);
		}
	}

	free(files[0].file);
}

void
test_tefs_read_after_write_to_multiple_files_one_at_a_time(
	planck_unit_test_t *tc
)
{
	files[0].file = malloc(sizeof(file_t));
	files[1].file = malloc(sizeof(file_t));

	if (files[0].file == NULL || files[1].file == NULL)
	{
		printf("%s\n", memory_error);
		exit(-1);
	}

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, format_device());
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(files[0].file, files[0].name));
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(files[1].file, files[1].name));

	populate_data_array_1();

	uint32_t pages_per_child_block = format_info->page_size * format_info->block_size / address_size * format_info->block_size;
	uint32_t num_pages = (pages_per_child_block < format_info->num_pages) ? pages_per_child_block : format_info->num_pages; // TODO

	for (i = 0; i < num_pages + 10; i++)
	{
		/* Write a page then read it */
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(files[0].file, i, data, format_info->page_size, 0));
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_read(files[0].file, i, buffer, format_info->page_size, 0));

		for (j = 0; j < format_info->page_size; j++)
		{
			PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[j], buffer[j]);
		}
	}

	for (i = 0; i < num_pages + 10; i++)
	{
		/* Write a page then read it */
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(files[1].file, i, data, format_info->page_size, 0));
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_read(files[1].file, i, buffer, format_info->page_size, 0));

		for (j = 0; j < format_info->page_size; j++)
		{
			PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[j], buffer[j]);
		}
	}

	free(files[0].file);
	free(files[1].file);
}

void
test_tefs_read_after_write_to_multiple_files_staggered(
	planck_unit_test_t *tc
)
{
	files[0].file = malloc(sizeof(file_t));
	files[1].file = malloc(sizeof(file_t));

	if (files[0].file == NULL || files[1].file == NULL)
	{
		printf("%s\n", memory_error);
		exit(-1);
	}

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, format_device());
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(files[0].file, files[0].name));
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(files[1].file, files[1].name));

	populate_data_array_1();

	uint32_t pages_per_child_block = format_info->page_size * format_info->block_size / address_size * format_info->block_size;
	uint32_t num_pages = (pages_per_child_block < format_info->num_pages) ? pages_per_child_block : format_info->num_pages; // TODO

	for (i = 0; i < num_pages + 10; i++)
	{
		/* Write a page then read it */
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(files[0].file, i, data, format_info->page_size, 0));
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_read(files[0].file, i, buffer, format_info->page_size, 0));

		for (j = 0; j < format_info->page_size; j++)
		{
			PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[j], buffer[j]);
		}

		/* Write a page then read it */
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(files[1].file, i, data, format_info->page_size, 0));
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_read(files[1].file, i, buffer, format_info->page_size, 0));

		for (j = 0; j < format_info->page_size; j++)
		{
			PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[j], buffer[j]);
		}
	}

	free(files[0].file);
	free(files[1].file);
}

void
test_tefs_write_to_multiple_files_one_at_a_time(
	planck_unit_test_t *tc
)
{
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, format_device());

	uint16_t file_num;
	for (file_num = 0; file_num < 10; file_num++)
	{
		file_t *file = malloc(sizeof(file_t));

		if (file == NULL)
		{
			printf("%s\n", memory_error);
			exit(-1);
		}

		char file_name[10];
		sprintf(file_name, "%s%d", "file.", file_num);

		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(file, file_name));

		/* Write num_pages of data to the file */
		for (i = 0; i < 1000; i++)
		{
			PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(file, i, data, format_info->page_size, 0));
		}

		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_close(file));

		free(file);
	}

	uint16_t num_hash_blocks = 0;
	for (file_num = 0; file_num < 10; file_num++)
	{
		uint32_t num_hashes_per_block = (format_info->page_size * format_info->block_size / format_info->hash_size);
		uint32_t num_meta_entries_per_block = (format_info->page_size * format_info->block_size /
											   format_info->meta_data_size);

		uint32_t hash_entry_page = (file_num * format_info->hash_size) / format_info->page_size;
		hash_entry_page += get_block_address(1) +
						   (file_num / num_hashes_per_block) * num_hashes_per_block * 2 * format_info->block_size;

		if (file_num > 0 && (file_num % num_hashes_per_block) == 0)
		{
			num_hash_blocks += 1;
		}

		hash_entry_page += num_hash_blocks * 8 * format_info->block_size;
		hash_entry_page += (num_hash_blocks > 0) ? format_info->block_size : 0;

		uint16_t hash_entry_byte = (file_num * format_info->hash_size) % format_info->page_size;

		uint32_t meta_data_page = (file_num * format_info->meta_data_size) / format_info->page_size;
		meta_data_page += get_block_address(3) +
						  (file_num / num_meta_entries_per_block) * num_meta_entries_per_block * 2 *
						  format_info->block_size;
		meta_data_page += (file_num / num_hashes_per_block) * format_info->block_size;

		uint16_t meta_data_byte = (file_num * format_info->meta_data_size) % format_info->page_size;

		/* Check the file size in the directory entry */
		four_byte_buffer = 0;
		device_read(meta_data_page, &four_byte_buffer, 4, meta_data_byte + TEFS_DIR_STATUS_SIZE);
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, four_byte_buffer);

		four_byte_buffer = 0;
		device_read(meta_data_page, &four_byte_buffer, 2,
					meta_data_byte + TEFS_DIR_STATUS_SIZE + TEFS_DIR_EOF_PAGE_SIZE);
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, four_byte_buffer);

		uint32_t temp_block =
			4 + file_num * 2 + (file_num / num_meta_entries_per_block) + (file_num / num_hashes_per_block);

		if (check_state_section(tc, 0, temp_block + 2))
		{
			PLANCK_UNIT_ASSERT_FALSE(tc);
		}

		/* Check first data block address */
		four_byte_buffer = 0;
		device_read(get_block_address(temp_block), &four_byte_buffer, 2, 0);
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, get_block_address(temp_block + 1), four_byte_buffer);
	}
}

void
test_tefs_write_to_multiple_files_staggered(
	planck_unit_test_t *tc
)
{

}

void
test_tefs_write_past_end_of_file(
	planck_unit_test_t *tc
)
{

}

void
test_tefs_hash_collision(
	planck_unit_test_t *tc
)
{
	file_t *file1 = malloc(sizeof(file_t));
	file_t *file2 = malloc(sizeof(file_t));

	if (file1 == NULL || file2 == NULL)
	{
		printf("%s\n", memory_error);
		exit(-1);
	}

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, format_device());

	/* The words "playwright" and "snush" collide for DJB2a. */
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(file1, "playwright"));
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(file2, "snush"));

	populate_data_array_1();

	/* Check the hash values in the hash entries file. */
	four_byte_buffer = 0;
	device_read(get_block_address(1), &four_byte_buffer, 4, 0);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 195669366, four_byte_buffer);

	four_byte_buffer = 0;
	device_read(get_block_address(1), &four_byte_buffer, 4, format_info->hash_size);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 195669366, four_byte_buffer);

	/* Close and open both files. */
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_close(file1));
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_close(file2));

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(file2, "snush"));
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(file1, "playwright"));

	/* Write to and from both files. */
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(file1, 0, data, format_info->page_size, 0));
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(get_block_address(5), buffer, format_info->page_size, 0));

	for (j = 0; j < format_info->page_size; j++)
	{
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[j], buffer[j]);
	}

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(file2, 0, data, format_info->page_size, 0));
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(get_block_address(7), buffer, format_info->page_size, 0));

	for (j = 0; j < format_info->page_size; j++)
	{
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[j], buffer[j]);
	}

	free(file1);
	free(file2);
}

//void test_tefs_release_block(
//	planck_unit_test_t *tc
//)
//{
//	file_t file;
//	format_device();
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(&file, "test.aaa"));
//
//	uint16_t i;
//	populate_data_array_1();
//
//	/* Write and read multiple pages */
//	for (i = 0; i < 100; i++)
//	{
//		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write(&file, i, data, 512, 0));
//	}
//
//	/* Release the pages */
//	for (i = 0; i < 2; i++)
//	{
//		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_release_block(&file, i * 8));
//	}
//
//	/* Check if the state section has the right values as well as the root index
//	   block */
//	uint8_t buffer;
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(1, &buffer, 1, 0));
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0x30, buffer);
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(1, &buffer, 1, 1));
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0x01, buffer);
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(1, &buffer, 1, 2));
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0xFF, buffer);
//
//	uint16_t address;
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(12, &address, 2, 0));
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, TEFS_DELETED, address);
//
//	/* Release the rest of the pages */
//	for (i = 2; i < (100 / 8) + 1; i++)
//	{
//		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_release_block(&file, i * 8));
//	}
//
//	/* Check if state section, root index block, and child index block are correct */
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(1, &buffer, 1, 0));
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0x7F, buffer);
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(1, &buffer, 1, 1));
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0xFF, buffer);
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(1, &buffer, 1, 2));
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0xFF, buffer);
//
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(4, &address, 2, 0));
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, TEFS_DELETED, address);
//}

//#if defined(TEFS_CONTINUOUS_SUPPORT)
//void test_tefs_sequential_read_and_write(
//	planck_unit_test_t *tc
//)
//{
//	file_t file;
//	format_device();
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_open(&file, "test.aaa"));
//
//	populate_data_array_1();
//
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write_continuous_start(&file, 0));
//
//	uint8_t buffer[27];
//	uint16_t i;
//
//	/* Write and read multiple pages */
//	for (i = 0; i < 16385 * 2; i++)
//	{
//		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write_continuous(&file, data, 512, 0));
//		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write_continuous_next(&file));
//	}
//
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_write_continuous_stop(&file));
//
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_read_continuous_start(&file, 0));
//
//	for (i = 0; i < 16385 * 2; i++)
//	{
//		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_read_continuous(&file, buffer, 27, 2));
//
//		uint8_t j;
//		for (j = 0; j < 27; j++)
//		{
//			PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[2 + j], buffer[j]);
//		}
//
//		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_read_continuous_next(&file));
//	}
//
//	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, tefs_read_continuous_stop(&file));
//}
//#endif

planck_unit_suite_t*
tefs_getsuite(
	void
)
{
	planck_unit_suite_t *suite = planck_unit_new_suite();

	planck_unit_add_to_suite(suite, test_tefs_format_device_without_erase);
	planck_unit_add_to_suite(suite, test_tefs_format_device_with_erase);
	planck_unit_add_to_suite(suite, test_tefs_create_single_file);

	planck_unit_add_to_suite(suite, test_tefs_create_multiple_files);

	planck_unit_add_to_suite(suite, test_tefs_remove_empty_single_file);
	planck_unit_add_to_suite(suite, test_tefs_remove_small_single_file);
	planck_unit_add_to_suite(suite, test_tefs_remove_large_single_file);

	planck_unit_add_to_suite(suite, test_tefs_remove_multiple_empty_files_consecutively);
	planck_unit_add_to_suite(suite, test_tefs_remove_multiple_files_with_data_consecutively);
	planck_unit_add_to_suite(suite, test_tefs_remove_multiple_empty_files_staggered);
	planck_unit_add_to_suite(suite, test_tefs_remove_multiple_files_with_data_staggered);

	planck_unit_add_to_suite(suite, test_tefs_exists_single_file);
	planck_unit_add_to_suite(suite, test_tefs_write_page_to_single_file);
	planck_unit_add_to_suite(suite, test_tefs_write_data_block_to_single_file);
	planck_unit_add_to_suite(suite, test_tefs_write_child_block_to_single_file);
	planck_unit_add_to_suite(suite, test_tefs_write_multiple_child_blocks_to_single_file);

	planck_unit_add_to_suite(suite, test_tefs_read_after_write_to_single_file);
	planck_unit_add_to_suite(suite, test_tefs_read_after_write_to_multiple_files_one_at_a_time);
	planck_unit_add_to_suite(suite, test_tefs_read_after_write_to_multiple_files_staggered);
//	planck_unit_add_to_suite(suite, test_tefs_write_to_multiple_files_one_at_a_time);//
//	planck_unit_add_to_suite(suite, test_tefs_write_to_multiple_files_staggered);//

//
//	planck_unit_add_to_suite(suite, test_tefs_release_block);~

#if defined(USE_SD) && defined(TEFS_CONTINUOUS_SUPPORT)
	//planck_unit_add_to_suite(suite, test_tefs_sequential_read_and_write);//
#endif

	/* Tests to check if errors are correctly returned. */
//	planck_unit_add_to_suite(suite, test_tefs_create_files_with_same_name);//
//	planck_unit_add_to_suite(suite, test_tefs_open_file_when_not_formatted);//
//	planck_unit_add_to_suite(suite, test_tefs_open_non_existent_file);//
//	planck_unit_add_to_suite(suite, test_tefs_read_past_end_of_file);//
//	planck_unit_add_to_suite(suite, test_tefs_write_past_end_of_file);//
//	planck_unit_add_to_suite(suite, test_tefs_file_name_too_long);//
	planck_unit_add_to_suite(suite, test_tefs_hash_collision);
#if !defined(ARDUINO)
//	planck_unit_add_to_suite(suite, test_tefs_fill_device);//
#endif

	if (format_info->block_size == 1)
	{
//		planck_unit_add_to_suite(suite, test_tefs_fill_file);//
	}

	return suite;
}

void
runalltests_tefs(
	void
)
{
	uint8_t format_run;
	for (format_run = 0; format_run < sizeof(format_info_arr) / sizeof(format_info_t); format_run++)
	{
		format_info = &format_info_arr[format_run];

#if defined(ARDUINO)
		format_info->num_pages = sd_spi_card_size();
#endif

		if (format_info->num_pages < 65536)
		{
			address_size = 2;
		}
		else
		{
			address_size = 4;
		}

		info_section_size = 1;
		state_section_size_in_bytes = (format_info->num_pages - info_section_size) / (format_info->block_size * 8);
		state_section_size = (state_section_size_in_bytes - 1) / (format_info->page_size) + 1;

		planck_unit_suite_t	*suite	= tefs_getsuite();
		planck_unit_run_suite(suite);
		planck_unit_destroy_suite(suite);
	}
}
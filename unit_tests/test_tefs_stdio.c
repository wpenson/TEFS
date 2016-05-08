#include "planck_unit/src/planckunit.h"
#include "../src/tefs_stdio/tefs_stdio.h"

#define CHIP_SELECT_PIN 4

static uint8_t data[530];
static uint16_t physical_page_size = 512;
static uint16_t block_size = 8;
static uint8_t hash_size = 4;
static uint16_t meta_data_size = 64;
static uint16_t max_file_name_size = 12;

static void
populate_data_array_1(
	void
)
{
	uint16_t i;

	for (i = 0; i < 26; i++) {
		data[i] = 'a' + i;
	}

	for (i = 26; i < 530; i++) {
		data[i] = '.';
	}
}

// static void
// populate_data_array_2(
// 	void
// )
// {
// 	long int i;

// 	for (i = 0; i < 26; i++) {
// 		data[i] = 'A' + i;
// 	}

// 	for (i = 26; i < 530; i++) {
// 		data[i] = '!';
// 	}
// }

static void
format_device(
	void
)
{
#if defined(USE_DATAFLASH) && defined(USE_FTL)
	ftl_Instantiate(&ftl);
	flare_Createftl(&ftl);
#endif

#if defined(USE_SD)
	//int number_of_pages = sd_spi_card_size();
	uint32_t number_of_pages = 62500;
#elif defined(USE_DATAFLASH) && defined(USE_FTL)
	uint32_t number_of_pages = 1000;
#elif defined(USE_DATAFLASH) && !defined(USE_FTL)
	uint32_t number_of_pages = DF_number_of_pages;
#endif

	tefs_format_device(number_of_pages, physical_page_size, block_size, hash_size, meta_data_size, max_file_name_size, 1);
}

void
test_tefs_stdio_write_pages(
	planck_unit_test_t *tc
)
{
	format_device();
	T_FILE *file = t_fopen("test.aaa", "w+");
	PLANCK_UNIT_ASSERT_TRUE(tc, file != NULL);

	populate_data_array_1();

	/* Write a single page and check if it has been written */
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 512, t_fwrite(data, 512, 1, file));

	uint8_t buffer[27];
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(43, buffer, 27, 2));

	uint32_t i;
	for (i = 0; i < 27; i++)
	{
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[i + 2], buffer[i]);
	}

	/* Write multiple pages but stay within a single meta block */
	for (i = 1; i < 50; i++)
	{
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 512, t_fwrite(data, 512, 1, file));
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(43 + i, buffer, 27, 2));

		uint8_t j;
		for (j = 0; j < 27; j++)
		{
			PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[j + 2], buffer[j]);
		}
	}

	/* Check the file size in the directory entry */
	uint32_t size = 0;
	device_read(27, &size, 4, TEFS_DIR_STATUS_SIZE + max_file_name_size);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 25088, size);

#if defined(USE_SD)
	/* Write multiple pages with meta block overflow */
	for (i = 50; i < 16385 * 2; i++)
	{
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 512, t_fwrite(data, 512, 1, file));
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(43 + i + (i / (16384)) * 8,
											 buffer, 27, 2));

		uint8_t j;
		for (j = 0; j < 27; j++)
		{
			PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[j + 2], buffer[j]);
		}
	}

	/* Check the file size in the directory entry */
	size = 0;
	device_read(27, &size, 4, TEFS_DIR_STATUS_SIZE + max_file_name_size);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 16777728, size);
#endif
}

void
test_tefs_stdio_write_past_block_boundary(
	planck_unit_test_t *tc
)
{
	format_device();
	T_FILE *file = t_fopen("test.aaa", "w+");
	PLANCK_UNIT_ASSERT_TRUE(tc, file != NULL);

	populate_data_array_1();
	uint8_t buffer[27];

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 530, t_fwrite(data, 530, 1, file));
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 530, t_fwrite(data, 530, 1, file));

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(43, buffer, 27, 0));

	uint8_t j;
	for (j = 0; j < 27; j++)
	{
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[j], buffer[j]);
	}

	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 0, device_read(44, buffer, 27, 18));

	for (j = 0; j < 27; j++)
	{
		PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, data[j], buffer[j]);
	}

	/* Check the file size in the directory entry */
	uint32_t size = 0;
	device_read(27, &size, 4, TEFS_DIR_STATUS_SIZE + max_file_name_size);
	PLANCK_UNIT_ASSERT_INT_ARE_EQUAL(tc, 512, size);
}

planck_unit_suite_t*
tefs_stdio_getsuite(
	void
)
{
	planck_unit_suite_t *suite = planck_unit_new_suite();

	planck_unit_add_to_suite(suite, test_tefs_stdio_write_pages);
	planck_unit_add_to_suite(suite, test_tefs_stdio_write_past_block_boundary);

	return suite;
}

void
runalltests_tefs_stdio(
	void
)
{
//	CuString	*output	= CuStringNew();
//	CuSuite		*suite	= tefs_stdio_getsuite();

	planck_unit_suite_t	*suite	= tefs_stdio_getsuite();
	planck_unit_run_suite(suite);

//	CuSuiteRun(suite);
//	CuSuiteSummary(suite, output);
//	CuSuiteDetails(suite, output);
//	printf("%s\n", output->buffer);
//
//	CuSuiteDelete(suite);
//	CuStringDelete(output);
}
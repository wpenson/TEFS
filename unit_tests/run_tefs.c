#define CHIP_SELECT_PIN 4

#include "../src/tefs/tefs.h"

#if defined(USE_DATAFLASH) && defined(USE_FTL)
#include "dataflash/ftl/ftl_api.h"
extern volatile flare_ftl_t ftl;
#endif

void
runalltests_tefs();

void
runalltests_tefs_stdio();

int
main()
{
#if defined(USE_DATAFLASH) && defined(USE_FTL)
	df_jvm_start();
	ftl_Instantiate(&ftl);
#elif defined(USE_DATAFLASH) && !defined(USE_FTL)
	int error = 0;

	if ((error = df_Instantiate(CHIP_SELECT_PIN)))
	{
		printf("Dataflash failed to initialize. Error code: %i\n", error);
		return -1;
	}
#elif defined(USE_SD)
	int error = 0;

	if ((error = sd_spi_init(CHIP_SELECT_PIN)))
	{
		printf("SD initialization failed. Error code: %i\n", error);
		return -1;
	}
#endif

	runalltests_tefs();
	runalltests_tefs_stdio();
	return 0;
}

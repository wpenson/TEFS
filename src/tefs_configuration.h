/******************************************************************************/
/**
@file		tefs_configuration.h
@author     Wade Penson
@date		June, 2015
@brief      Storage Configuration for TEFS. This is where you define the storage
            device you want to use for TEFS.
*/
/******************************************************************************/

#ifndef TEFS_CONFIGURATION_H_
#define TEFS_CONFIGURATION_H_

/* Uncomment this line if you wish to use an SD card for persistent storage. */
#define USE_SD

/* Uncomment this line if you wish to use an Adesto Dataflash card for
   persistent storage. A optional FTL (flash transition layer) with
   wear leveling is also provided. */
// #define USE_DATAFLASH
// #define USE_FTL

#endif /* TEFS_CONFIGURATION_H_ */
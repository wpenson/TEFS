cmake_minimum_required(VERSION 3.5)
project(tefs)

set(SOURCE_FILES
    tefs.c
    tefs.h
    ../tefs_configuration.h)

add_definitions(${sd_spi_emulator_DEFINITIONS})

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})

# Required on Unix OS family to be able to be linked into shared libraries.
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Change sd_spi_emulator to sd_spi_device if you want to use TEFS on device
target_link_libraries(${PROJECT_NAME} sd_spi_emulator)
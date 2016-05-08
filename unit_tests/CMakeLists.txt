cmake_minimum_required(VERSION 3.5)
project(test_tefs)

set(SOURCE_FILES
    run_tefs.c
    test_tefs.c
    test_tefs_stdio.c)

set(PLANCK_UNIT_SOURCE_FILES
	planck_unit/src/planckunit.c
	planck_unit/src/planckunit.h)

add_definitions(${tefs_stdio_DEFINITIONS})
#add_definitions(${planck_unit_DEFINITIONS})

include_directories(${tefs_stdio_INCLUDE_DIRS})
#include_directories(${planck_unit_INCLUDE_DIRS})

add_library(planck_unit STATIC ${PLANCK_UNIT_SOURCE_FILES})
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} tefs_stdio planck_unit)
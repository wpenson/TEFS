cmake_minimum_required(VERSION 3.5)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall")

add_subdirectory(src/tefs/)
add_subdirectory(src/tefs_stdio/)
add_subdirectory(unit_tests/)
add_subdirectory(src/tefs/sd_spi/src/)
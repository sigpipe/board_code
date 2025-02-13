cmake_minimum_required(VERSION 2.8.7)
project(ech C)
add_executable(ech ech.c myiio.c)
add_executable(mon mon.c myiio.c)
# Find_library(IIOLIB libiio-dev)
target_link_libraries(ech iio m)
target_link_libraries(mon iio m)


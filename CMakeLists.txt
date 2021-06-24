#
# project: chip8
#
cmake_minimum_required(VERSION 3.12)
project(chip8)
# include the fips main cmake file
get_filename_component(FIPS_ROOT_DIR "../fips" ABSOLUTE)
include("${FIPS_ROOT_DIR}/cmake/fips.cmake")

# set(Python3_EXECUTABLE C:\Users\-\AppData\Local\Microsoft\WindowsApps\python3.exe)

include_directories(third_party)
include_directories(third_party/sokol)
include_directories(roms)
fips_setup()

fips_add_subdirectory(third_party)
fips_add_subdirectory(roms)
fips_add_subdirectory(src)

fips_finish()

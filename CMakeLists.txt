cmake_minimum_required(VERSION 3.4)
project("Centreon Collect" C CXX)
set(CMAKE_CXX_STANDARD 11)

# Version.
set(CLIB_MAJOR 20)
set(CLIB_MINOR 04)
set(CLIB_PATCH 0)
set(CLIB_VERSION "${CLIB_MAJOR}.${CLIB_MINOR}.${CLIB_PATCH}")


if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
  include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
  conan_basic_setup(TARGETS)
else()
  message(WARNING "The file conanbuildinfo.cmake doesn't exist, you have to run conan install first")
endif()

add_subdirectory(clib)
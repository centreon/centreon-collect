cmake_minimum_required(VERSION 3.4)
project("Centreon Collect" C CXX)
set(CMAKE_CXX_STANDARD 11)

# Version.
set(COLLECT_MAJOR 20)
set(COLLECT_MINOR 10)
set(COLLECT_PATCH 0)
set(COLLECT_VERSION "${COLLECT_MAJOR}.${COLLECT_MINOR}.${COLLECT_PATCH}")


if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
  include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
  conan_basic_setup(TARGETS)
else()
  message(WARNING "The file conanbuildinfo.cmake doesn't exist, you have to run conan install first")
endif()

add_subdirectory(clib)
add_subdirectory(connectors)
add_subdirectory(engine)

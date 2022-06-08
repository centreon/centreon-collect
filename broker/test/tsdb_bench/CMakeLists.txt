project("tsdb_bench" CXX)
cmake_minimum_required(VERSION 3.16)
add_definitions("-D_GLIBCXX_USE_CXX11_ABI=1")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)

conan_basic_setup(TARGETS)

add_definitions(${CONAN_DEFINES_SPDLOG})

include_directories(${CONAN_INCLUDE_DIRS} db timescale)

add_executable(tsdb_bench tsdb_bench.cc metric.cc db/connection.cc db/request.cc timescale/pg_connection.cc)
target_link_libraries(tsdb_bench CONAN_PKG::abseil CONAN_PKG::boost CONAN_PKG::fmt CONAN_PKG::spdlog CONAN_PKG::libpq)
target_precompile_headers(tsdb_bench PRIVATE precomp_inc.hh)

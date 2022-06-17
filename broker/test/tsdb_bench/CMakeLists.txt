project("tsdb_bench" CXX)
cmake_minimum_required(VERSION 3.16)
add_definitions("-D_GLIBCXX_USE_CXX11_ABI=1")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)

conan_basic_setup(TARGETS)

add_definitions(${CONAN_DEFINES_SPDLOG})

set(SRC_DIR "${PROJECT_SOURCE_DIR}")

include_directories(${CONAN_INCLUDE_DIRS} ${SRC_DIR} ${SRC_DIR}/db ${SRC_DIR}/timescale)

set(SOURCES
    "${SRC_DIR}/tsdb_bench.cc"
    "${SRC_DIR}/metric.cc"
    "${SRC_DIR}/asio.cc"
    "${SRC_DIR}/beast.cc"
    "${SRC_DIR}/db/connection.cc"
    "${SRC_DIR}/db/request.cc"
    "${SRC_DIR}/timescale/pg_connection.cc"
    "${SRC_DIR}/timescale/pg_request.cc"
    "${SRC_DIR}/prometheus/pr_http_server.cc"
)

add_executable(tsdb_bench ${SOURCES})
add_definitions("-DBOOST_ASIO_SEPARATE_COMPILATION" "-DBOOST_BEAST_SEPARATE_COMPILATION")
target_link_libraries(tsdb_bench CONAN_PKG::abseil CONAN_PKG::boost CONAN_PKG::fmt CONAN_PKG::spdlog CONAN_PKG::libpq)
target_precompile_headers(tsdb_bench PRIVATE precomp_inc.hh)

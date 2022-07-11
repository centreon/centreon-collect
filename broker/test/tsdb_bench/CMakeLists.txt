project("tsdb_bench" CXX)
cmake_minimum_required(VERSION 3.16)
add_definitions("-D_GLIBCXX_USE_CXX11_ABI=1")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)

conan_basic_setup(TARGETS)

add_definitions(${CONAN_DEFINES_SPDLOG})

set(SRC_DIR "${PROJECT_SOURCE_DIR}")

if(NOT DEFINED ENV{QUESTDB_ILP_DIR})
    message(FATAL_ERROR "environment variable QUESTDB_ILP_DIR is not defined,
it should point to the compiled c-questdb-client project that you can obtain at
https://github.com/questdb/c-questdb-client")
endif()

include_directories(${CONAN_INCLUDE_DIRS} ${SRC_DIR} ${SRC_DIR}/db ${SRC_DIR}/timescale ${SRC_DIR}/http_client ${SRC_DIR}/influxdb ${SRC_DIR}/m3 ${SRC_DIR}/quesdb $ENV{QUESTDB_ILP_DIR}/include)

set(SOURCES
    "${SRC_DIR}/tsdb_bench.cc"
    "${SRC_DIR}/metric.cc"
    "${SRC_DIR}/asio.cc"
    "${SRC_DIR}/beast.cc"
    "${SRC_DIR}/db/db_conf.cc"
    "${SRC_DIR}/db/connection.cc"
    "${SRC_DIR}/db/request.cc"
    "${SRC_DIR}/timescale/pg_connection.cc"
    "${SRC_DIR}/timescale/pg_request.cc"
    "${SRC_DIR}/prometheus/pr_http_server.cc"
    "${SRC_DIR}/http_client/http_connection.cc"
    "${SRC_DIR}/influxdb/inf_http_client.cc"
    "${SRC_DIR}/warp10/warp10_http_client.cc"
    "${SRC_DIR}/m3/remote.pb.cc"
    "${SRC_DIR}/m3/types.pb.cc"
    "${SRC_DIR}/m3/gogoproto/gogo.pb.cc"
    "${SRC_DIR}/m3/m3_http_client.cc"
    "${SRC_DIR}/mimir/mimir.pb.cc"
    "${SRC_DIR}/mimir/mimir_http_client.cc"
    "${SRC_DIR}/questdb/questdb.cc"
)

add_custom_command(
    DEPENDS ${SRC_DIR}/m3/remote.proto
    COMMENT "Generating interface files of the prometheus remote proto file"
    OUTPUT ${SRC_DIR}/m3/remote.pb.cc ${SRC_DIR}/m3/remote.pb.h
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --cpp_out=${SRC_DIR}/m3 --proto_path=${SRC_DIR}/m3 remote.proto

    DEPENDS ${SRC_DIR}/m3/types.proto
    COMMENT "Generating interface files of the prometheus types proto file"
    OUTPUT ${SRC_DIR}/m3/types.pb.cc ${SRC_DIR}/m3/types.pb.h
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --cpp_out=${SRC_DIR}/m3 --proto_path=${SRC_DIR}/m3 types.proto

    DEPENDS ${SRC_DIR}/m3/gogoproto/gogo.proto
    COMMENT "Generating interface files of the prometheus gogo proto file"
    OUTPUT ${SRC_DIR}/m3/gogoproto/gogo.pb.cc ${SRC_DIR}/m3/gogoproto/gogo.pb.h
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --cpp_out=${SRC_DIR}/m3 --proto_path=${SRC_DIR}/m3 gogoproto/gogo.proto
)

add_custom_command(
    DEPENDS ${SRC_DIR}/mimir/mimir.proto
    COMMENT "Generating interface files of the mimir proto file"
    OUTPUT ${SRC_DIR}/mimir/mimir.pb.cc ${SRC_DIR}/mimir/mimir.pb.h
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --cpp_out=${SRC_DIR}/mimir --proto_path=${SRC_DIR}/mimir --proto_path=${SRC_DIR}/m3 mimir.proto
)

add_custom_target(tsdb_bench_proto DEPENDS
    ${SRC_DIR}/m3/remote.pb.cc
    ${SRC_DIR}/m3/remote.pb.h
    ${SRC_DIR}/m3/types.pb.cc
    ${SRC_DIR}/m3/types.pb.h
    ${SRC_DIR}/m3/gogoproto/gogo.pb.cc
    ${SRC_DIR}/m3/gogoproto/gogo.pb.h
    ${SRC_DIR}/mimir/mimir.pb.cc
    ${SRC_DIR}/mimir/mimir.pb.h
)

add_executable(tsdb_bench ${SOURCES})
add_definitions("-DBOOST_ASIO_SEPARATE_COMPILATION" "-DBOOST_BEAST_SEPARATE_COMPILATION")
target_link_libraries(tsdb_bench CONAN_PKG::abseil CONAN_PKG::boost CONAN_PKG::fmt CONAN_PKG::spdlog CONAN_PKG::libpq CONAN_PKG::protobuf CONAN_PKG::snappy $ENV{QUESTDB_ILP_DIR}/build/libquestdb_client.a dl)
target_precompile_headers(tsdb_bench PRIVATE precomp_inc.hh)

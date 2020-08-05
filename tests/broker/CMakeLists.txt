include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR})

include(${CMAKE_SOURCE_DIR}/cmake/FindMariaDB.cmake)

add_definitions(-DCENTREON_BROKER_TEST_MODULE_PATH="${CMAKE_BINARY_DIR}/lib/")
add_definitions(-DCENTREON_BROKER_BAM_TEST_PATH="${CMAKE_SOURCE_DIR}/tests/broker/bam")
add_definitions(-DCENTREON_BROKER_LUA_SCRIPT_PATH="${CMAKE_SOURCE_DIR}/tests/broker/lua")
add_definitions(-DCENTREON_BROKER_NEB_TEST="${CMAKE_SOURCE_DIR}/tests/broker/neb")
add_definitions(-DBROKERRPC_TESTS_PATH="${CMAKE_SOURCE_DIR}/tests/broker/rpc")

if (WITH_SQL_TEST)
  set(SQL_SOURCE
    ${CMAKE_SOURCE_DIR}/tests/broker/bam/monitoring_stream.cc
    ${CMAKE_SOURCE_DIR}/tests/broker/bam/reporting_stream.cc
    ${CMAKE_SOURCE_DIR}/tests/broker/storage/conflict_manager.cc
    ${CMAKE_SOURCE_DIR}/tests/broker/mysql/mysql.cc
    )
else()
  set(SQL_SOURCE "")
endif()

add_library(null_module SHARED
  ${CMAKE_SOURCE_DIR}/tests/broker/modules/null_module.cc)

add_library(bad_version_module SHARED
  ${CMAKE_SOURCE_DIR}/tests/broker/modules/bad_version_module.cc)

add_executable(rpc_client ${CMAKE_SOURCE_DIR}/tests/broker/rpc/client.cc)
target_link_libraries(rpc_client CONAN_PKG::grpc ccb_rpc)

add_executable(ccb_ut
  ${CMAKE_SOURCE_DIR}/src/cbd/broker_impl.cc
  ${CMAKE_SOURCE_DIR}/src/cbd/brokerrpc.cc
  ${CMAKE_SOURCE_DIR}/src/cbd/config/applier/init.cc
  ${CMAKE_SOURCE_DIR}/src/cbd/config/applier/logger.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/set_log_data.cc
  ${SQL_SOURCE}
  ${CMAKE_SOURCE_DIR}/tests/broker/bam/ba/kpi_change_at_recompute.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/bam/configuration/applier-boolexp.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/bam/exp_builder/exp_builder.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/bam/exp_parser/get_postfix.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/bam/exp_tokenizer/next.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/bam/time/check_timeperiod.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/bbdo/input_buffer/erase.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/bbdo/input_buffer/extract.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/bbdo/output.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/bbdo/read.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/ceof/ceof_parser/parse.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/compression/stream/read.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/compression/stream/write.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/compression/zlib/zlib.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/config/logger.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/config/parser.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/file/splitter/concurrent.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/file/splitter/default.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/file/splitter/more_than_max_size.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/file/splitter/permission_denied.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/file/splitter/resume.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/file/splitter/split.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/graphite/factory.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/graphite/query.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/graphite/stream.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/influxdb/column.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/influxdb/factory.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/influxdb/influxdb12.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/influxdb/line_protocol_query.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/influxdb/stream.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/lua/lua.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/misc/exec.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/misc/filesystem.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/misc/math.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/misc/misc.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/misc/string.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/misc/stringifier.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/modules/module.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/multiplexing/engine/hook.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/multiplexing/engine/hooker.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/multiplexing/engine/start_stop.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/multiplexing/engine/unhook.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/multiplexing/muxer/read.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/multiplexing/publisher/read.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/multiplexing/publisher/write.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/multiplexing/subscriber/ctor_default.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/custom_variable.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/custom_variable_status.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/event_handler.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/flapping_status.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/host.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/host_check.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/host_dependency.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/host_group.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/host_parent.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/host_status.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/instance.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/instance_status.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/log_entry.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/module.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/randomize.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/service.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/service_group.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/service_check.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/service_dependency.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/service_status.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/neb/set_log_data.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/processing/acceptor.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/processing/feeder.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/processing/temporary_endpoint.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/processing/temporary_stream.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/processing/thread.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/rpc/brokerrpc.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/rrd/cached.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/rrd/factory.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/rrd/lib.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/rrd/rrd.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/stats/stats.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/storage/connector.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/storage/metric.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/storage/perfdata.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/storage/rebuild.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/storage/remove_graph.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/storage/status.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/tcp/acceptor.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/tcp/connector.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/tcp/factory.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/exceptions.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/io.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/logging.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/main.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/simu.cc
  ${CMAKE_SOURCE_DIR}/tests/broker/test_server.cc
  )

target_link_libraries(ccb_ut CONAN_PKG::gtest CONAN_PKG::json11 ccb_rpc
                             ccb_core ccb_conflict_manager 10-neb 15-stats
                             20-storage 20-bam 50-tcp 70-lua 70-rrd 70-simu
                             70-influxdb 70-graphite CONAN_PKG::grpc
                             CONAN_PKG::spdlog)
set_target_properties(ccb_ut PROPERTIES COMPILE_FLAGS ${MYSQL_CFLAGS})

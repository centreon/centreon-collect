add_definitions(-DCENTREON_BROKER_VERSION=\"${COLLECT_VERSION}\")

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR})

include(${CMAKE_SOURCE_DIR}/cmake/FindMariaDB.cmake)

add_definitions(-DCENTREON_BROKER_TEST_MODULE_PATH="${CMAKE_BINARY_DIR}/lib/")
add_definitions(-DCENTREON_BROKER_BAM_TEST_PATH="${CMAKE_SOURCE_DIR}/test/broker/bam")
add_definitions(-DCENTREON_BROKER_LUA_SCRIPT_PATH="${CMAKE_SOURCE_DIR}/test/broker/lua")
add_definitions(-DCENTREON_BROKER_NEB_TEST="${CMAKE_SOURCE_DIR}/test/broker/neb")
add_definitions(-DBROKERRPC_TESTS_PATH="${CMAKE_SOURCE_DIR}/test/broker/rpc")

if (WITH_SQL_TEST)
  set(SQL_SOURCE
    ${CMAKE_SOURCE_DIR}/test/broker/bam/monitoring_stream.cc
    ${CMAKE_SOURCE_DIR}/test/broker/bam/reporting_stream.cc
    ${CMAKE_SOURCE_DIR}/test/broker/storage/conflict_manager.cc
    ${CMAKE_SOURCE_DIR}/test/broker/mysql/mysql.cc
    )
else()
  set(SQL_SOURCE "")
endif()

add_library(null_module SHARED
  ${CMAKE_SOURCE_DIR}/test/broker/modules/null_module.cc)

add_library(bad_version_module SHARED
  ${CMAKE_SOURCE_DIR}/test/broker/modules/bad_version_module.cc)

add_executable(rpc_client ${CMAKE_SOURCE_DIR}/test/broker/rpc/client.cc)
target_link_libraries(rpc_client CONAN_PKG::grpc ccb_rpc)

add_executable(ccb_ut
  ${CMAKE_SOURCE_DIR}/src/cbd/broker_impl.cc
  ${CMAKE_SOURCE_DIR}/src/cbd/brokerrpc.cc
  ${CMAKE_SOURCE_DIR}/src/cbd/config/applier/init.cc
  ${CMAKE_SOURCE_DIR}/src/cbd/config/applier/logger.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/set_log_data.cc
  ${SQL_SOURCE}
  ${CMAKE_SOURCE_DIR}/test/broker/bam/ba/kpi_change_at_recompute.cc
  ${CMAKE_SOURCE_DIR}/test/broker/bam/configuration/applier-boolexp.cc
  ${CMAKE_SOURCE_DIR}/test/broker/bam/exp_builder/exp_builder.cc
  ${CMAKE_SOURCE_DIR}/test/broker/bam/exp_parser/get_postfix.cc
  ${CMAKE_SOURCE_DIR}/test/broker/bam/exp_tokenizer/next.cc
  ${CMAKE_SOURCE_DIR}/test/broker/bam/time/check_timeperiod.cc
  ${CMAKE_SOURCE_DIR}/test/broker/bbdo/input_buffer/erase.cc
  ${CMAKE_SOURCE_DIR}/test/broker/bbdo/input_buffer/extract.cc
  ${CMAKE_SOURCE_DIR}/test/broker/bbdo/output.cc
  ${CMAKE_SOURCE_DIR}/test/broker/bbdo/read.cc
  ${CMAKE_SOURCE_DIR}/test/broker/ceof/ceof_parser/parse.cc
  ${CMAKE_SOURCE_DIR}/test/broker/compression/stream/read.cc
  ${CMAKE_SOURCE_DIR}/test/broker/compression/stream/write.cc
  ${CMAKE_SOURCE_DIR}/test/broker/compression/zlib/zlib.cc
  ${CMAKE_SOURCE_DIR}/test/broker/config/logger.cc
  ${CMAKE_SOURCE_DIR}/test/broker/config/parser.cc
  ${CMAKE_SOURCE_DIR}/test/broker/file/splitter/concurrent.cc
  ${CMAKE_SOURCE_DIR}/test/broker/file/splitter/default.cc
  ${CMAKE_SOURCE_DIR}/test/broker/file/splitter/more_than_max_size.cc
  ${CMAKE_SOURCE_DIR}/test/broker/file/splitter/permission_denied.cc
  ${CMAKE_SOURCE_DIR}/test/broker/file/splitter/resume.cc
  ${CMAKE_SOURCE_DIR}/test/broker/file/splitter/split.cc
  ${CMAKE_SOURCE_DIR}/test/broker/graphite/factory.cc
  ${CMAKE_SOURCE_DIR}/test/broker/graphite/query.cc
  ${CMAKE_SOURCE_DIR}/test/broker/graphite/stream.cc
  ${CMAKE_SOURCE_DIR}/test/broker/influxdb/column.cc
  ${CMAKE_SOURCE_DIR}/test/broker/influxdb/factory.cc
  ${CMAKE_SOURCE_DIR}/test/broker/influxdb/influxdb12.cc
  ${CMAKE_SOURCE_DIR}/test/broker/influxdb/line_protocol_query.cc
  ${CMAKE_SOURCE_DIR}/test/broker/influxdb/stream.cc
  ${CMAKE_SOURCE_DIR}/test/broker/lua/lua.cc
  ${CMAKE_SOURCE_DIR}/test/broker/misc/exec.cc
  ${CMAKE_SOURCE_DIR}/test/broker/misc/filesystem.cc
  ${CMAKE_SOURCE_DIR}/test/broker/misc/math.cc
  ${CMAKE_SOURCE_DIR}/test/broker/misc/misc.cc
  ${CMAKE_SOURCE_DIR}/test/broker/misc/string.cc
  ${CMAKE_SOURCE_DIR}/test/broker/misc/stringifier.cc
  ${CMAKE_SOURCE_DIR}/test/broker/modules/module.cc
  ${CMAKE_SOURCE_DIR}/test/broker/multiplexing/engine/hook.cc
  ${CMAKE_SOURCE_DIR}/test/broker/multiplexing/engine/hooker.cc
  ${CMAKE_SOURCE_DIR}/test/broker/multiplexing/engine/start_stop.cc
  ${CMAKE_SOURCE_DIR}/test/broker/multiplexing/engine/unhook.cc
  ${CMAKE_SOURCE_DIR}/test/broker/multiplexing/muxer/read.cc
  ${CMAKE_SOURCE_DIR}/test/broker/multiplexing/publisher/read.cc
  ${CMAKE_SOURCE_DIR}/test/broker/multiplexing/publisher/write.cc
  ${CMAKE_SOURCE_DIR}/test/broker/multiplexing/subscriber/ctor_default.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/custom_variable.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/custom_variable_status.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/event_handler.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/flapping_status.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/host.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/host_check.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/host_dependency.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/host_parent.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/host_status.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/instance.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/instance_status.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/log_entry.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/module.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/randomize.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/service.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/service_check.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/service_dependency.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/service_status.cc
  ${CMAKE_SOURCE_DIR}/test/broker/neb/set_log_data.cc
  ${CMAKE_SOURCE_DIR}/test/broker/processing/acceptor.cc
  ${CMAKE_SOURCE_DIR}/test/broker/processing/feeder.cc
  ${CMAKE_SOURCE_DIR}/test/broker/processing/temporary_endpoint.cc
  ${CMAKE_SOURCE_DIR}/test/broker/processing/temporary_stream.cc
  ${CMAKE_SOURCE_DIR}/test/broker/processing/thread.cc
  ${CMAKE_SOURCE_DIR}/test/broker/rpc/brokerrpc.cc
  ${CMAKE_SOURCE_DIR}/test/broker/rrd/cached.cc
  ${CMAKE_SOURCE_DIR}/test/broker/rrd/exceptions.cc
  ${CMAKE_SOURCE_DIR}/test/broker/rrd/factory.cc
  ${CMAKE_SOURCE_DIR}/test/broker/rrd/lib.cc
  ${CMAKE_SOURCE_DIR}/test/broker/rrd/rrd.cc
  ${CMAKE_SOURCE_DIR}/test/broker/stats/stats.cc
  ${CMAKE_SOURCE_DIR}/test/broker/storage/connector.cc
  ${CMAKE_SOURCE_DIR}/test/broker/storage/metric.cc
  ${CMAKE_SOURCE_DIR}/test/broker/storage/perfdata.cc
  ${CMAKE_SOURCE_DIR}/test/broker/storage/rebuild.cc
  ${CMAKE_SOURCE_DIR}/test/broker/storage/remove_graph.cc
  ${CMAKE_SOURCE_DIR}/test/broker/storage/status.cc
  ${CMAKE_SOURCE_DIR}/test/broker/tcp/acceptor.cc
  ${CMAKE_SOURCE_DIR}/test/broker/tcp/connector.cc
  ${CMAKE_SOURCE_DIR}/test/broker/tcp/factory.cc
  ${CMAKE_SOURCE_DIR}/test/broker/exceptions.cc
  ${CMAKE_SOURCE_DIR}/test/broker/io.cc
  ${CMAKE_SOURCE_DIR}/test/broker/logging.cc
  ${CMAKE_SOURCE_DIR}/test/broker/main.cc
  ${CMAKE_SOURCE_DIR}/test/broker/test_server.cc
  )


target_link_libraries(ccb_ut CONAN_PKG::gtest CONAN_PKG::json11 ccb_rpc
                             ccb_core ccb_conflict_manager 10-neb 15-stats
                             20-storage 20-bam 50-tcp 70-lua 70-rrd
                             70-influxdb 70-graphite CONAN_PKG::grpc
                             CONAN_PKG::spdlog)
set_target_properties(ccb_ut PROPERTIES COMPILE_FLAGS ${MYSQL_CFLAGS})
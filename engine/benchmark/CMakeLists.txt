find_package(benchmark REQUIRED)

add_executable(engine_bench unescape.cc)

target_compile_features(engine_bench PRIVATE cxx_std_17)
target_compile_options(engine_bench PRIVATE -O2)

target_link_libraries(engine_bench
    PRIVATE
    benchmark::benchmark_main
    absl::strings
    absl::str_format
    Boost::algorithm)

# utf8.cc has no dependency of its own, so it is compiled straight into the
# benchmark rather than pulling the whole centreon_common library in.
add_executable(utf8_bench utf8.cc ${CMAKE_SOURCE_DIR}/common/src/utf8.cc)

target_include_directories(utf8_bench
    PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common/inc/com/centreon/common)

target_compile_features(utf8_bench PRIVATE cxx_std_17)
target_compile_options(utf8_bench PRIVATE -O2)

target_link_libraries(utf8_bench PRIVATE benchmark::benchmark_main)

add_executable(timeperiod_bench timeperiod.cc)

target_compile_features(timeperiod_bench PRIVATE cxx_std_17)
target_compile_options(timeperiod_bench PRIVATE -O2)

target_link_libraries(timeperiod_bench
    PRIVATE
    benchmark::benchmark
    timeperiods
    engine_conf
    absl::flat_hash_map
    spdlog::spdlog
    fmt::fmt)

# Macro evaluation, family by family (see doc/engine-macros-en.md). It builds a
# poller with the real appliers, so it links Engine as ut_engine does, and
# reuses the init_config_state() helper of the unit tests.
add_executable(macros_bench macros.cc ${PROJECT_SOURCE_DIR}/tests/helper.cc)

target_include_directories(macros_bench
    PRIVATE
    ${PROJECT_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/bbdo
    ${CMAKE_BINARY_DIR})

target_precompile_headers(macros_bench REUSE_FROM cce_core)

target_link_libraries(macros_bench
    PRIVATE
    benchmark::benchmark
    -Wl,-whole-archive
    cce_core
    log_v2
    opentelemetry
    centagent_lib
    -Wl,-no-whole-archive
    enginerpc
    pb_open_telemetry_lib
    centreon_grpc
    centreon_http
    centreon_process
    Boost::program_options
    pthread
    gRPC::grpc++
    crypto
    ssl
    z
    fmt::fmt
    ryml::ryml
    stdc++fs
    dl)

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

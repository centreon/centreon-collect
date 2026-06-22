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

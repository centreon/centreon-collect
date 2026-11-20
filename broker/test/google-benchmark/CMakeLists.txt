add_executable(broker_bench
    int64_map.cc
    #    old_muxer_queue.cc
    muxer_queue.cc)

target_compile_features(broker_bench PRIVATE cxx_std_17)

target_link_libraries(broker_bench
    PRIVATE
    benchmark::benchmark_main
    absl::flat_hash_map
    absl::btree
    Boost::container)

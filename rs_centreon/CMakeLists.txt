if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CARGO_CMD cargo build --verbose)
    set(TARGET_DIR "debug")
else ()
    set(CARGO_CMD cargo build --release --verbose)
    set(TARGET_DIR "release")
endif ()

#if(ENABLE_LTO)
#    set(RUST_FLAGS "-Clinker-plugin-lto" "-Clinker=clang-17" "-Clink-arg=-fuse-ld=lld-17")
#endif()

set(RS_CENTREON_LIB "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_DIR}/librs_centreon.a")

set(RS_CENTREON_CXX "${CMAKE_CURRENT_BINARY_DIR}/rs_centreon.cc")
add_library(rs_centreon STATIC ${RS_CENTREON_CXX})
set(rs_files
  src/lib.rs
  )

add_custom_command(
  OUTPUT ${RS_CENTREON_CXX}
    DEPENDS ${rs_files}
    COMMAND CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR} RUSTFLAGS="${RUST_FLAGS}" ${CARGO_CMD}
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/cxxbridge/rs_centreon/src/lib.rs.cc ${RS_CENTREON_CXX}
    COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/cxxbridge/rs_centreon/src/lib.rs.h ${CMAKE_CURRENT_BINARY_DIR}/rs_centreon.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(target_rs_centreon DEPENDS ${RS_CENTREON_CXX})
set_target_properties(rs_centreon PROPERTIES COMPILE_FLAGS "-fPIC")
target_link_libraries(rs_centreon pthread dl ${RS_CENTREON_LIB})

add_test(NAME rs_centreon_test
    COMMAND cargo test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

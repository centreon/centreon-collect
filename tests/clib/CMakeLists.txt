add_executable(bin_test_process_output ${CMAKE_SOURCE_DIR}/tests/clib/bin_test_process_output.cc)

add_library(shared_testing_library
  SHARED
  ${CMAKE_SOURCE_DIR}/tests/clib/shared_testing_library.cc
  )

add_executable(clib-ut
  ${CMAKE_SOURCE_DIR}/tests/clib/process.cc
  ${CMAKE_SOURCE_DIR}/tests/clib/exceptions.cc
  ${CMAKE_SOURCE_DIR}/tests/clib/handle_manager.cc
  ${CMAKE_SOURCE_DIR}/tests/clib/io.cc
  ${CMAKE_SOURCE_DIR}/tests/clib/library.cc
  ${CMAKE_SOURCE_DIR}/tests/clib/logging.cc
  ${CMAKE_SOURCE_DIR}/tests/clib/main.cc
  ${CMAKE_SOURCE_DIR}/tests/clib/misc.cc
  ${CMAKE_SOURCE_DIR}/tests/clib/task_manager.cc
  ${CMAKE_SOURCE_DIR}/tests/clib/timestamp.cc
  ${CMAKE_SOURCE_DIR}/tests/clib/version.cc)
target_link_libraries(clib-ut CONAN_PKG::gtest centreon_clib)

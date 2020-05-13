add_executable(bin_test_process_output ${CMAKE_SOURCE_DIR}/test/clib/bin_test_process_output.cc)

add_library(shared_testing_library
  SHARED
  ${CMAKE_SOURCE_DIR}/test/clib/shared_testing_library.cc
  )

add_executable(clib-ut
  ${CMAKE_SOURCE_DIR}/test/clib/process.cc
  ${CMAKE_SOURCE_DIR}/test/clib/exceptions.cc
  ${CMAKE_SOURCE_DIR}/test/clib/handle_manager.cc
  ${CMAKE_SOURCE_DIR}/test/clib/io.cc
  ${CMAKE_SOURCE_DIR}/test/clib/library.cc
  ${CMAKE_SOURCE_DIR}/test/clib/logging.cc
  ${CMAKE_SOURCE_DIR}/test/clib/main.cc
  ${CMAKE_SOURCE_DIR}/test/clib/misc.cc
  ${CMAKE_SOURCE_DIR}/test/clib/task_manager.cc
  ${CMAKE_SOURCE_DIR}/test/clib/timestamp.cc
  ${CMAKE_SOURCE_DIR}/test/clib/version.cc)
target_link_libraries(clib-ut CONAN_PKG::gtest centreon-clib)
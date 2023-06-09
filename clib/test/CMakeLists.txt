include_directories(${CONAN_INCLUDE_DIRS_GTEST})
add_executable(bin_test_process_output ${PROJECT_SOURCE_DIR}/test/bin_test_process_output.cc)

add_library(shared_testing_library
  SHARED
  ${PROJECT_SOURCE_DIR}/test/shared_testing_library.cc
  )

set_target_properties(
    shared_testing_library
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/tests
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/tests
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/tests
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/tests
)

include_directories(${PROJECT_SOURCE_DIR}/test/)

add_executable(ut_clib
  ${PROJECT_SOURCE_DIR}/test/process.cc
  ${PROJECT_SOURCE_DIR}/test/exceptions.cc
  ${PROJECT_SOURCE_DIR}/test/handle_manager.cc
  ${PROJECT_SOURCE_DIR}/test/io.cc
  ${PROJECT_SOURCE_DIR}/test/library.cc
  ${PROJECT_SOURCE_DIR}/test/logging.cc
  ${PROJECT_SOURCE_DIR}/test/main.cc
  ${PROJECT_SOURCE_DIR}/test/misc.cc
  ${PROJECT_SOURCE_DIR}/test/task_manager.cc
  ${PROJECT_SOURCE_DIR}/test/timestamp.cc
  ${PROJECT_SOURCE_DIR}/test/version.cc)

set_target_properties(
    ut_clib bin_test_process_output
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/tests
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/tests
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/tests
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/tests
)

if (WITH_COVERAGE)
  set(COVERAGE_EXCLUDES '${PROJECT_SOURCE_DIR}/test/*' '/usr/include/*')
  SETUP_TARGET_FOR_COVERAGE(
      NAME clib-test-coverage
      EXECUTABLE ut_clib
      DEPENDENCIES ut_clib
  )
  set(GCOV gcov)
endif ()
add_test(NAME tests COMMAND ut_clib)
target_link_libraries(ut_clib CONAN_PKG::gtest pthread centreon_clib)

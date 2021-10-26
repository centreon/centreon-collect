include_directories(${CONAN_INCLUDE_DIRS_GTEST})
#link_directories(${GTest_LIB_DIRS})
add_executable(bin_test_process_output ${PROJECT_SOURCE_DIR}/test/bin_test_process_output.cc)


add_library(shared_testing_library
  SHARED
  ${PROJECT_SOURCE_DIR}/test/shared_testing_library.cc
  )

include_directories(${PROJECT_SOURCE_DIR}/test/)

add_executable(ut-clib
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

if (WITH_COVERAGE)
  set(COVERAGE_EXCLUDES '${PROJECT_BINARY_DIR}/*' '${PROJECT_SOURCE_DIR}/tests/*' '/usr/include/* ')
  SETUP_TARGET_FOR_COVERAGE(
      NAME test-coverage
      EXECUTABLE ut-clib
      DEPENDENCIES ut-clib
  )
  set(GCOV gcov)
endif ()
add_test(NAME tests COMMAND ut-clib)
target_link_libraries(ut-clib CONAN_PKG::gtest pthread centreon_clib)

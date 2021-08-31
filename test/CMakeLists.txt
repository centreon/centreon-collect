find_package(GTest REQUIRED)
include_directories(${GTest_INCLUDE_DIRS})
link_directories(${GTest_LIB_DIRS})
add_executable(bin_test_process_output ${CMAKE_SOURCE_DIR}/test/bin_test_process_output.cc)


add_library(shared_testing_library
  SHARED
  ${CMAKE_SOURCE_DIR}/test/shared_testing_library.cc
  )

include_directories(${CMAKE_SOURCE_DIR}/test/)

add_executable(ut-clib
  ${CMAKE_SOURCE_DIR}/test/process.cc
  ${CMAKE_SOURCE_DIR}/test/exceptions.cc
  ${CMAKE_SOURCE_DIR}/test/handle_manager.cc
  ${CMAKE_SOURCE_DIR}/test/io.cc
  ${CMAKE_SOURCE_DIR}/test/library.cc
  ${CMAKE_SOURCE_DIR}/test/logging.cc
  ${CMAKE_SOURCE_DIR}/test/main.cc
  ${CMAKE_SOURCE_DIR}/test/misc.cc
  ${CMAKE_SOURCE_DIR}/test/task_manager.cc
  ${CMAKE_SOURCE_DIR}/test/timestamp.cc
  ${CMAKE_SOURCE_DIR}/test/version.cc)

if (WITH_COVERAGE)
  set(COVERAGE_EXCLUDES '${PROJECT_BINARY_DIR}/*' '${PROJECT_SOURCE_DIR}/tests/*' '/usr/include/* ')
  SETUP_TARGET_FOR_COVERAGE(
      NAME test-coverage
      EXECUTABLE ut
      DEPENDENCIES ut
  )
  set(GCOV gcov)
endif ()
add_test(NAME tests COMMAND ut-clib)
target_link_libraries(ut-clib ${GTest_LIBS} pthread centreon_clib_shared)

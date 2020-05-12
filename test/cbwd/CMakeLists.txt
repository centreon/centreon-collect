add_executable(cbwd_ut ${CMAKE_SOURCE_DIR}/test/cbwd/watchdog.cc)
add_definitions(-DCENTREON_BROKER_WD_TEST="${CMAKE_SOURCE_DIR}/test/cbwd/")
target_link_libraries(cbwd_ut centreon-clib ccb_core CONAN_PKG::gtest)
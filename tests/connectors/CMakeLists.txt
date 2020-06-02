add_definitions(-DBUILD_PATH="${CMAKE_BINARY_DIR}")

# Generate code to dynamically load modules.
include(${CMAKE_SOURCE_DIR}/cmake/Findperl.cmake)



set_source_files_properties("${CMAKE_BINARY_DIR}/xs_init.cc" PROPERTIES GENERATED TRUE)

add_executable(ccc_ut
  ${CMAKE_SOURCE_DIR}/src/connectors/perl/embedded_perl.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/common/log.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/perl/pipe_handle.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/perl/script.cc
  ${CMAKE_BINARY_DIR}/xs_init.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/checks/check.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/checks/result.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/checks/timeout.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/multiplexer.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/orders/options.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/orders/parser.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/sessions/credentials.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/sessions/session.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/sessions/socket_handle.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/reporter.cc
  ${CMAKE_SOURCE_DIR}/tests/connectors/buffer_handle.cc
  ${CMAKE_SOURCE_DIR}/tests/connectors/checks.cc
  ${CMAKE_SOURCE_DIR}/tests/connectors/connector.cc
  ${CMAKE_SOURCE_DIR}/tests/connectors/embedded_perl.cc
  ${CMAKE_SOURCE_DIR}/tests/connectors/fake_listener.cc
  ${CMAKE_SOURCE_DIR}/tests/connectors/main.cc
  ${CMAKE_SOURCE_DIR}/tests/connectors/orders.cc
  ${CMAKE_SOURCE_DIR}/tests/connectors/reporter.cc
  ${CMAKE_SOURCE_DIR}/tests/connectors/sessions.cc)

add_dependencies(ccc_ut xs_init)
target_link_libraries(ccc_ut CONAN_PKG::gtest CONAN_PKG::spdlog CONAN_PKG::libssh2 ${PERL_LIBRARIES} centreon-clib)

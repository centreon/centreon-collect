add_definitions(-DCENTREON_CONNECTOR_VERSION=\"${COLLECT_VERSION}\")
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR})

include(${CMAKE_SOURCE_DIR}/cmake/Findperl.cmake)

add_definitions(-DBUILD_PATH="${CMAKE_BINARY_DIR}")

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
  ${CMAKE_SOURCE_DIR}/test/connectors/buffer_handle.cc
  ${CMAKE_SOURCE_DIR}/test/connectors/checks.cc
  ${CMAKE_SOURCE_DIR}/test/connectors/connector.cc
  ${CMAKE_SOURCE_DIR}/test/connectors/embedded_perl.cc
  ${CMAKE_SOURCE_DIR}/test/connectors/fake_listener.cc
  ${CMAKE_SOURCE_DIR}/test/connectors/main.cc
  ${CMAKE_SOURCE_DIR}/test/connectors/orders.cc
  ${CMAKE_SOURCE_DIR}/test/connectors/reporter.cc
  ${CMAKE_SOURCE_DIR}/test/connectors/sessions.cc)

target_link_libraries(ccc_ut CONAN_PKG::gtest CONAN_PKG::spdlog CONAN_PKG::libssh2 ${PERL_LIBRARIES} centreon-clib)
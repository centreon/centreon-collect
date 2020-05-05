##
## Copyright 2011-2015,2017-2019 Centreon
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## For more information : contact@centreon.com
##
pkg_check_modules(centreon-clib QUIET centreon-clib)

# Generate code to dynamically load modules.
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/xs_init.cc"
  COMMAND "${PERL_EXECUTABLE}" "-MExtUtils::Embed" "-e" "xsinit" "--" "-o" "${CMAKE_BINARY_DIR}/xs_init.cc")

include_directories(${CMAKE_SOURCE_DIR}/connectors/perl/inc/)
# Perl connector.
add_executable(centreon_connector_perl
  # Sources.
  ${CMAKE_SOURCE_DIR}/connectors/common/src/log.cc
  ${CMAKE_SOURCE_DIR}/connectors/perl/src/main.cc
  ${CMAKE_SOURCE_DIR}/connectors/perl/src/checks/check.cc
  ${CMAKE_SOURCE_DIR}/connectors/perl/src/checks/result.cc
  ${CMAKE_SOURCE_DIR}/connectors/perl/src/checks/timeout.cc
  ${CMAKE_SOURCE_DIR}/connectors/perl/src/embedded_perl.cc
  ${CMAKE_SOURCE_DIR}/connectors/perl/src/multiplexer.cc
  ${CMAKE_SOURCE_DIR}/connectors/perl/src/options.cc
  ${CMAKE_SOURCE_DIR}/connectors/perl/src/orders/parser.cc
  ${CMAKE_SOURCE_DIR}/connectors/perl/src/pipe_handle.cc
  ${CMAKE_SOURCE_DIR}/connectors/perl/src/policy.cc
  ${CMAKE_SOURCE_DIR}/connectors/perl/src/reporter.cc
  ${CMAKE_SOURCE_DIR}/connectors/perl/src/script.cc
  ${CMAKE_BINARY_DIR}/xs_init.cc
  # Headers.
  ${CMAKE_SOURCE_DIR}/connectors/perl/inc/com/centreon/connector/perl/checks/check.hh
  ${CMAKE_SOURCE_DIR}/connectors/perl/inc/com/centreon/connector/perl/checks/listener.hh
  ${CMAKE_SOURCE_DIR}/connectors/perl/inc/com/centreon/connector/perl/checks/result.hh
  ${CMAKE_SOURCE_DIR}/connectors/perl/inc/com/centreon/connector/perl/checks/timeout.hh
  ${CMAKE_SOURCE_DIR}/connectors/perl/inc/com/centreon/connector/perl/embedded_perl.hh
  ${CMAKE_SOURCE_DIR}/connectors/perl/inc/com/centreon/connector/perl/multiplexer.hh
  ${CMAKE_SOURCE_DIR}/connectors/perl/inc/com/centreon/connector/perl/namespace.hh
  ${CMAKE_SOURCE_DIR}/connectors/perl/inc/com/centreon/connector/perl/options.hh
  ${CMAKE_SOURCE_DIR}/connectors/perl/inc/com/centreon/connector/perl/orders/listener.hh
  ${CMAKE_SOURCE_DIR}/connectors/perl/inc/com/centreon/connector/perl/orders/parser.hh
  ${CMAKE_SOURCE_DIR}/connectors/perl/inc/com/centreon/connector/perl/pipe_handle.hh
  ${CMAKE_SOURCE_DIR}/connectors/perl/inc/com/centreon/connector/perl/policy.hh
  ${CMAKE_SOURCE_DIR}/connectors/perl/inc/com/centreon/connector/perl/reporter.hh
  )

target_link_libraries(centreon_connector_perl ${CLIB_LIBRARIES} ${PERL_LIBRARIES} centreon-clib CONAN_PKG::spdlog pthread)

# Installation path.
if (WITH_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${WITH_PREFIX}")
endif ()
if (WITH_PREFIX_BINARY)
  set(PREFIX_BINARY "${WITH_PREFIX_BINARY}")
else ()
  set(PREFIX_BINARY "${CMAKE_INSTALL_PREFIX}/bin")
endif ()

# Install rules.
install(TARGETS centreon_connector_perl RUNTIME DESTINATION ${PREFIX_BINARY})

# Code coverage on unit tests
option(WITH_COVERAGE "Add code coverage on unit tests." OFF)
if (WITH_COVERAGE)
  set(WITH_TESTING ON)
  set(CMAKE_BUILD_TYPE "Debug")
  include(CodeCoverage.cmake)
  APPEND_COVERAGE_COMPILER_FLAGS()
endif ()

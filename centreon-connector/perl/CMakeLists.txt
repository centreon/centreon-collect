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

# Generate code to dynamically load modules.
add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/perl/src/xs_init.cc"
  COMMAND "${PERL_EXECUTABLE}" "-MExtUtils::Embed" "-e" "xsinit" "--" "-o" "${PROJECT_SOURCE_DIR}/perl/src/xs_init.cc")

include_directories(${PROJECT_SOURCE_DIR}/perl/inc/)
link_directories(${CMAKE_SOURCE_DIR}/build/centreon-clib/)
# Perl connector.
add_executable(centreon_connector_perl
  # Sources.
  ${PROJECT_SOURCE_DIR}/common/src/log.cc
  ${PROJECT_SOURCE_DIR}/perl/src/main.cc
  ${PROJECT_SOURCE_DIR}/perl/src/checks/check.cc
  ${PROJECT_SOURCE_DIR}/perl/src/checks/result.cc
  ${PROJECT_SOURCE_DIR}/perl/src/checks/timeout.cc
  ${PROJECT_SOURCE_DIR}/perl/src/embedded_perl.cc
  ${PROJECT_SOURCE_DIR}/perl/src/multiplexer.cc
  ${PROJECT_SOURCE_DIR}/perl/src/options.cc
  ${PROJECT_SOURCE_DIR}/perl/src/orders/parser.cc
  ${PROJECT_SOURCE_DIR}/perl/src/pipe_handle.cc
  ${PROJECT_SOURCE_DIR}/perl/src/policy.cc
  ${PROJECT_SOURCE_DIR}/perl/src/reporter.cc
  ${PROJECT_SOURCE_DIR}/perl/src/script.cc
  ${PROJECT_SOURCE_DIR}/perl/src/xs_init.cc
  # Headers.
  ${PROJECT_SOURCE_DIR}/perl/inc/com/centreon/connector/perl/checks/check.hh
  ${PROJECT_SOURCE_DIR}/perl/inc/com/centreon/connector/perl/checks/listener.hh
  ${PROJECT_SOURCE_DIR}/perl/inc/com/centreon/connector/perl/checks/result.hh
  ${PROJECT_SOURCE_DIR}/perl/inc/com/centreon/connector/perl/checks/timeout.hh
  ${PROJECT_SOURCE_DIR}/perl/inc/com/centreon/connector/perl/embedded_perl.hh
  ${PROJECT_SOURCE_DIR}/perl/inc/com/centreon/connector/perl/multiplexer.hh
  ${PROJECT_SOURCE_DIR}/perl/inc/com/centreon/connector/perl/namespace.hh
  ${PROJECT_SOURCE_DIR}/perl/inc/com/centreon/connector/perl/options.hh
  ${PROJECT_SOURCE_DIR}/perl/inc/com/centreon/connector/perl/orders/listener.hh
  ${PROJECT_SOURCE_DIR}/perl/inc/com/centreon/connector/perl/orders/parser.hh
  ${PROJECT_SOURCE_DIR}/perl/inc/com/centreon/connector/perl/pipe_handle.hh
  ${PROJECT_SOURCE_DIR}/perl/inc/com/centreon/connector/perl/policy.hh
  ${PROJECT_SOURCE_DIR}/perl/inc/com/centreon/connector/perl/reporter.hh
  )
add_dependencies(centreon_connector_perl centreon_clib_shared)
target_link_libraries(centreon_connector_perl centreon_clib ${PERL_LIBRARIES} ${spdlog_LIBS} ${fmt_LIBS} pthread)


# Installation path.
if (WITH_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${WITH_PREFIX}")
endif ()

# Install rules.
install(TARGETS centreon_connector_perl RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/centreon-connector/)

# Code coverage on unit tests
option(WITH_COVERAGE "Add code coverage on unit tests." OFF)
if (WITH_COVERAGE)
  set(WITH_TESTING ON)
  set(CMAKE_BUILD_TYPE "Debug")
  include(CodeCoverage.cmake)
  APPEND_COVERAGE_COMPILER_FLAGS()
endif ()

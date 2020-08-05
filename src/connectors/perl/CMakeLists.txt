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
include(${CMAKE_SOURCE_DIR}/cmake/Findperl.cmake)

add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/xs_init.cc"
  COMMAND "${PERL_EXECUTABLE}" "-MExtUtils::Embed" "-e" "xsinit" "--" "-o" "${CMAKE_BINARY_DIR}/xs_init.cc")

add_custom_target(xs_init DEPENDS "${CMAKE_BINARY_DIR}/xs_init.cc")

# Perl connector.
add_executable(centreon_connector_perl
  # Sources.
  ${CMAKE_SOURCE_DIR}/src/connectors/common/log.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/perl/main.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/perl/checks/check.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/perl/checks/result.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/perl/checks/timeout.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/perl/embedded_perl.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/perl/multiplexer.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/perl/options.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/perl/orders/parser.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/perl/pipe_handle.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/perl/policy.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/perl/reporter.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/perl/script.cc
  ${CMAKE_BINARY_DIR}/xs_init.cc
  )

target_link_libraries(centreon_connector_perl ${PERL_LIBRARIES} centreon_clib CONAN_PKG::spdlog pthread)
install(TARGETS centreon_connector_perl COMPONENT connector-perl DESTINATION ${CMAKE_INSTALL_LIBDIR}/centreon-connector)

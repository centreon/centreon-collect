#
# Copyright 2011-2015,2017-2019,2026 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#

# Generate code to dynamically load modules.
add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/perl/src/xs_init.cc"
  COMMAND "${PERL_EXECUTABLE}" "-MExtUtils::Embed" "-e" "xsinit" "--" "-o" "${PROJECT_SOURCE_DIR}/perl/src/xs_init.cc")

include_directories(${PROJECT_SOURCE_DIR}/perl/inc/ 
  ${CMAKE_BINARY_DIR}/connectors/perl
  ${CMAKE_SOURCE_DIR}/common/process/inc )
link_directories(${CMAKE_SOURCE_DIR}/build/centreon-clib/)

add_library(pb_perl_connector_lib src/perl_connector.proto)
set_target_properties(pb_perl_connector_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
protobuf_generate(TARGET pb_perl_connector_lib)

# Perl connector.
add_executable(centreon_connector_perl

  # Sources.
  ${PROJECT_SOURCE_DIR}/common/src/log.cc
  ${PROJECT_SOURCE_DIR}/common/src/reporter.cc
  ${PROJECT_SOURCE_DIR}/common/src/result.cc
  ${PROJECT_SOURCE_DIR}/common/src/parser.cc
  ${PROJECT_SOURCE_DIR}/perl/src/config.cc
  ${PROJECT_SOURCE_DIR}/perl/src/orders/parser.cc
  ${PROJECT_SOURCE_DIR}/perl/src/check_child.cc
  ${PROJECT_SOURCE_DIR}/perl/src/main.cc
  ${PROJECT_SOURCE_DIR}/perl/src/policy.cc
  ${PROJECT_SOURCE_DIR}/perl/src/protocol.cc
  ${PROJECT_SOURCE_DIR}/perl/src/script_child.cc
  ${PROJECT_SOURCE_DIR}/perl/src/xs_init.cc
)




add_dependencies(centreon_connector_perl centreon_clib)
target_link_libraries(centreon_connector_perl 
  centreon_process_no_crypto
  centreon_file_system
  centreon_hexdump
  process_stat
  ${PERL_LIBRARIES}
  pb_perl_connector_lib
  protobuf::libprotobuf
  Boost::program_options
  Boost::process
  re2::re2
  spdlog::spdlog
  fmt::fmt
  OpenSSL::SSL
  absl::any
  absl::log
  absl::base
  absl::bits
  absl::raw_hash_set
  absl::hash
  absl::hashtablez_sampler
  pthread)

target_precompile_headers(centreon_connector_perl PRIVATE ${PROJECT_SOURCE_DIR}/precomp_inc/precomp.hh)

# Install rules.
install(TARGETS centreon_connector_perl RUNTIME DESTINATION ${PREFIX_CONNECTORS})

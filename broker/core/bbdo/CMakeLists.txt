#
# Copyright 2024-2026 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# For more information : contact@centreon.com
#

include_directories(${CMAKE_BINARY_DIR}/bbdo)

add_library(broker_bbdo STATIC acceptor.cc connector.cc factory.cc internal.cc
  basic_stream.cc
  broker_stream.cc
  stream.cc)

target_compile_definitions(broker_bbdo PRIVATE BROKER_COMPILATION)

target_link_libraries(broker_bbdo PRIVATE protobuf::libprotobuf engine_conf)
add_dependencies(
  broker_bbdo
  pb_all_libs
  process_stat
  engine_conf)
target_precompile_headers(broker_bbdo PRIVATE precomp_inc/precomp.hh)

set_target_properties(broker_bbdo PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(cbmod_bbdo STATIC acceptor.cc connector.cc factory.cc internal.cc
  basic_stream.cc
  cbmod_stream.cc
  stream.cc)

target_compile_definitions(cbmod_bbdo PRIVATE CBMOD_COMPILATION)

target_link_libraries(cbmod_bbdo PRIVATE engine_conf)
add_dependencies(
  cbmod_bbdo
  pb_all_libs
  process_stat
  engine_conf)
target_precompile_headers(cbmod_bbdo PRIVATE precomp_inc/precomp.hh)

set_target_properties(cbmod_bbdo PROPERTIES POSITION_INDEPENDENT_CODE ON)

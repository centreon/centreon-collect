#
# Copyright 2026 Centreon
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
include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/core)

add_library(broker_applier_state STATIC
  broker_state.cc
  broker_state.hh
  endpoint.cc
  endpoint.hh
  init.cc
  init.hh
  modules.cc
  modules.hh
  state.cc
  state.hh
)

target_compile_definitions(broker_applier_state PRIVATE BROKER_COMPILATION)

target_precompile_headers(broker_applier_state PRIVATE ../../precomp_inc/precomp.hpp)

set_target_properties(broker_applier_state PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_dependencies(broker_applier_state process_stat)

add_library(cbmod_applier_state STATIC
  cbmod_state.cc
  cbmod_state.hh
  endpoint.cc
  endpoint.hh
  init.cc
  init.hh
  modules.cc
  modules.hh
  state.cc
  state.hh
)

target_compile_definitions(cbmod_applier_state PRIVATE CBMOD_COMPILATION)

target_precompile_headers(cbmod_applier_state PRIVATE ../../precomp_inc/precomp.hpp)

set_target_properties(cbmod_applier_state PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_dependencies(cbmod_applier_state process_stat)

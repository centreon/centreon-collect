# Copyright 2026 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#

# The macros of Engine. They read the objects of the core library which uses
# them, so they are an OBJECT library whose objects go into cce_core.
add_library(
  engine_macros OBJECT
  descriptors.cc
  macro_context.cc
  process.cc
  resolve.cc
  summary.cc)

add_dependencies(engine_macros process_stat engine_rpc centreon_clib
                 pb_neb_lib cbmod timeperiods)

target_link_libraries(engine_macros PRIVATE engine_conf notifications
                                            timeperiods fmt::fmt spdlog::spdlog)

target_precompile_headers(engine_macros PRIVATE ${PRECOMP_HEADER})

if(WITH_SHARED_LIB)
  set_target_properties(engine_macros PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

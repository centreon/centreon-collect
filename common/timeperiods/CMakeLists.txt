#
# Copyright 2026 Centreon
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

# Timeperiods library.
add_library(timeperiods STATIC
  daterange.cc
  timeperiod.cc
  timerange.cc
  timezone.cc
)

target_include_directories(timeperiods PUBLIC
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/clib/inc
  ${CMAKE_SOURCE_DIR}/common/inc
  ${CMAKE_BINARY_DIR}
)

target_link_libraries(timeperiods
  PUBLIC
    spdlog::spdlog
    fmt::fmt
)

add_dependencies(timeperiods engine_conf)

target_compile_definitions(timeperiods PRIVATE
  SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

set_target_properties(timeperiods PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

target_precompile_headers(timeperiods PRIVATE
  ${CMAKE_SOURCE_DIR}/engine/precomp_inc/precomp.hh)

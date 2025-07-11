#
# Copyright 2023 Centreon
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

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
add_definitions(${spdlog_DEFINITIONS})

add_library(
  log_v2 STATIC
  # Sources.
  log_v2.cc log_v2.hh config.hh)

set_target_properties(log_v2 PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(log_v2 spdlog::spdlog stdc++fs gRPC::gpr
                      absl::raw_hash_set)

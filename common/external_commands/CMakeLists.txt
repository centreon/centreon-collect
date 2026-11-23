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

# Legacy external commands table, shared by Engine (parser) and Broker (routing).
add_library(external_commands STATIC
  command_table.cc
)

target_include_directories(external_commands PUBLIC
  ${CMAKE_SOURCE_DIR}
)

target_link_libraries(external_commands PUBLIC absl::flat_hash_map)

set_target_properties(external_commands PROPERTIES POSITION_INDEPENDENT_CODE ON)

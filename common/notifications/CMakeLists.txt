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

# Notifications library.
add_library(notifications STATIC
  contact_viability.cc
  escalation.cc
  notification_manager.cc
)

target_include_directories(notifications PUBLIC
  ${PROJECT_SOURCE_DIR}/inc
  ${CMAKE_SOURCE_DIR}/common/inc
)

target_link_libraries(notifications
  PUBLIC
    spdlog::spdlog
    fmt::fmt
)

target_compile_definitions(notifications PRIVATE
  SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

set_target_properties(notifications PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

target_precompile_headers(notifications PRIVATE
  ${PROJECT_SOURCE_DIR}/precomp_inc/precomp.hh)

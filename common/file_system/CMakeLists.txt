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

add_library(
  centreon_file_system STATIC
  # Sources.
  file_watcher.cc)

set_target_properties(centreon_file_system
                      PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_precompile_headers(centreon_file_system REUSE_FROM centreon_common)


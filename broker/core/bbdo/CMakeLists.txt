#
# Copyright 2024 Centreon
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

# include_directories(${PROJECT_SOURCE_DIR}/core)

add_library(bbdo STATIC acceptor.cc connector.cc factory.cc
                        internal.cc stream.cc)

add_dependencies(bbdo target_common target_bbdo target_extcmd pb_open_telemetry_lib)
target_precompile_headers(bbdo PRIVATE precomp_inc/precomp.hh)

set_target_properties(bbdo PROPERTIES POSITION_INDEPENDENT_CODE ON)

#
# Copyright 2009-2025 Centreon
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

# Set directories.
set(SRC_DIR "${SRC_DIR}/configuration")
set(INC_DIR "${INC_DIR}/com/centreon/engine/configuration")

# Subdirectory.
add_subdirectory(applier)

# Set files.
set(FILES
    ${FILES}
    # Sources.
    "${SRC_DIR}/whitelist.cc"
    "${SRC_DIR}/extended_conf.cc"
    ${SRC_DIR}/indexed_state.cc
    PARENT_SCOPE)

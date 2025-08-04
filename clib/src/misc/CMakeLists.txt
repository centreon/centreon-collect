#
# Copyright 2011-2013 Centreon
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

# Set directories.
set(SRC_DIR "${SRC_DIR}/misc")
set(INC_DIR "${INC_DIR}/misc")

# Set sources.
set(
  SOURCES
  ${SOURCES}
  "${SRC_DIR}/argument.cc"
  "${SRC_DIR}/command_line.cc"
  "${SRC_DIR}/get_options.cc"
  "${SRC_DIR}/stringifier.cc"
  PARENT_SCOPE
)

# Set headers.
set(
  HEADERS
  ${HEADERS}
  "${INC_DIR}/argument.hh"
  "${INC_DIR}/command_line.hh"
  "${INC_DIR}/get_options.hh"
  "${INC_DIR}/stringifier.hh"
  PARENT_SCOPE
)

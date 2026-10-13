#
# Copyright 2011-2013 Merethis
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

# Set directories.
set(SRC_DIR "${SRC_DIR}/macros")
set(INC_DIR "${INC_DIR}/com/centreon/engine/macros")

# Set files.
set(
  FILES
  ${FILES}

  # Sources.
  "${SRC_DIR}/descriptors.cc"
  "${SRC_DIR}/grab_host.cc"
  "${SRC_DIR}/grab_service.cc"
  "${SRC_DIR}/grab_value.cc"
  "${SRC_DIR}/process.cc"

  # Headers.
  "${INC_DIR}/descriptors.hh"
  "${INC_DIR}/grab.hh"
  "${INC_DIR}/grab_host.hh"
  "${INC_DIR}/grab_service.hh"
  "${INC_DIR}/grab_value.hh"
  "${INC_DIR}/process.hh"

  PARENT_SCOPE
)

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

# Set directories.
set(SRC_DIR "${SRC_DIR}/commands")
set(INC_DIR "${INC_DIR}/com/centreon/engine/commands")

# Set files.
set(
  FILES
  ${FILES}

  # Sources.
  "${SRC_DIR}/command.cc"
  "${SRC_DIR}/commands.cc"
  "${SRC_DIR}/connector.cc"
  "${SRC_DIR}/environment.cc"
  "${SRC_DIR}/forward.cc"
  "${SRC_DIR}/otel_connector.cc"
  "${SRC_DIR}/otel_interface.cc"
  "${SRC_DIR}/processing.cc"
  "${SRC_DIR}/raw.cc"
  "${SRC_DIR}/raw_v2.cc"
  "${SRC_DIR}/result.cc"

  # Headers.
  "${INC_DIR}/command.hh"
  "${INC_DIR}/commands.hh"
  "${INC_DIR}/command_listener.hh"
  "${INC_DIR}/connector.hh"
  "${INC_DIR}/environment.hh"
  "${INC_DIR}/forward.hh"
  "${INC_DIR}/processing.hh"
  "${INC_DIR}/raw.hh"
  "${INC_DIR}/result.hh"

  PARENT_SCOPE
)

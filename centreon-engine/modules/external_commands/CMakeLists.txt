#
# Copyright 2011-2013 Centreon
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY # KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#

# Set directories.
set(MODULE_DIR "${PROJECT_SOURCE_DIR}/modules/external_commands")
set(SRC_DIR "${MODULE_DIR}/src")
set(INC_DIR "${MODULE_DIR}/inc/com/centreon/engine/modules/external_commands")

# Include directories.
include_directories("${MODULE_DIR}/inc")
link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

# mod_externalcmd target.
add_library(
  externalcmd SHARED
  # Sources.
  "${SRC_DIR}/commands.cc"
  "${SRC_DIR}/internal.cc"
  "${SRC_DIR}/main.cc"
  "${SRC_DIR}/processing.cc"
  "${SRC_DIR}/utils.cc"
  # Headers.
  "${INC_DIR}/commands.hh"
  "${INC_DIR}/internal.hh"
  "${INC_DIR}/processing.hh"
  "${INC_DIR}/utils.hh")

# Prettier name.
set_property(TARGET "externalcmd" PROPERTY PREFIX "")

# Location.
set(EXTERNALCMD_MODULE
    "${EXTERNALCMD_MODULE}"
    PARENT_SCOPE)
add_dependencies(externalcmd centreon_clib)
target_link_libraries(externalcmd centreon_clib)

# Install rule.
install(
  TARGETS "externalcmd"
  DESTINATION "${PREFIX_LIB}"
  COMPONENT "runtime")

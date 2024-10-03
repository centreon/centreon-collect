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
set(SRC_DIR "${PROJECT_SOURCE_DIR}/conf")

# Configure files.
configure_file("${SRC_DIR}/centagent.json.in"
  "${SRC_DIR}/centagent.json")

# Install files if necessary.
option(WITH_SAMPLE_CONFIG "Install sample configuration files." ON)
if (WITH_SAMPLE_CONFIG)
  install(DIRECTORY "${SRC_DIR}/"
    DESTINATION "${PREFIX_AGENT_CONF}"
    COMPONENT "runtime"
    FILES_MATCHING PATTERN "*.cfg")

  install(CODE "
  function(my_chown user group file)
    if (APPLE OR (UNIX AND NOT CYGWIN))
      execute_process(COMMAND \"chown\" \"\${user}:\${group}\" \"\${file}\")
    endif ()
  endfunction()

  my_chown(\"${USER_AGENT}\" \"${USER_AGENT}\" \"${PREFIX_AGENT_CONF}/centagent.json\")
  ")
endif ()

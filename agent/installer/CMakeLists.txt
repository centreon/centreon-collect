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

project("Centreon agent Installer")


# Set directories.
set(CENTAGENT_PATH_OUTPUT_DIR ${Centreon\ agent_BINARY_DIR})

if( ${CMAKE_GENERATOR} MATCHES "Visual Studio.*" )
  set(CENTAGENT_PATH "${Centreon\ agent_BINARY_DIR}/${CMAKE_BUILD_TYPE}/centagent.exe")
else()  
  set(CENTAGENT_PATH "${Centreon\ agent_BINARY_DIR}/centagent.exe")
endif()

string(REPLACE "/" "\\" CENTAGENT_PATH "${CENTAGENT_PATH}") 

set(AGENT_VERSION "${AGENT_MAJOR}.${AGENT_MINOR}.${AGENT_PATCH}")

# Path to Inno Setup Compiler
set(ISCC_EXECUTABLE "C:/Program Files (x86)/Inno Setup 6/ISCC.exe")

# Inno script path
set(INNO_SCRIPT "${PROJECT_SOURCE_DIR}/installer.iss")

# Define your output path for the installer
set(INSTALLER_OUTPUT "${PROJECT_SOURCE_DIR}/centreon-monitoring-agent.exe")

# # Resource files (optional use)
# file(GLOB INSTALLER_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/resources/*")

# Create configured .iss file from .in template
configure_file(
  ${PROJECT_SOURCE_DIR}/installer.iss.in
  ${PROJECT_SOURCE_DIR}/installer_configured.iss
  @ONLY
)

# Inno Setup command
# Add a custom target to build the installer
# Custom target to run Inno Setup
add_custom_target(installer
  COMMAND "${ISCC_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/installer_configured.iss"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Building installer using Inno Setup with version ${MY_APP_VERSION}"
  VERBATIM
)
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


# set the centagent.exe path 
set(CENTAGENT_PATH_OUTPUT_DIR ${Centreon\ agent_BINARY_DIR})

if( ${CMAKE_GENERATOR} MATCHES "Visual Studio.*" )
  set(CENTAGENT_PATH "${Centreon\ agent_BINARY_DIR}/${CMAKE_BUILD_TYPE}/centagent.exe")
else()  
  set(CENTAGENT_PATH "${Centreon\ agent_BINARY_DIR}/centagent.exe")
endif()

string(REPLACE "/" "\\" CENTAGENT_PATH "${CENTAGENT_PATH}") 

#set the verion of the agent
set(AGENT_VERSION "${AGENT_MAJOR}.${AGENT_MINOR}.${AGENT_PATCH}")

#set the output directory for the installer and the output base filename
set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(OUTPUT_BASE_FILENAME "centreon-agent-monitoring")

# Path to Inno Setup Compiler
set(ISCC_EXECUTABLE "C:/Program Files (x86)/Inno Setup 6/ISCC.exe")

# Inno script path
set(INNO_SCRIPT "${PROJECT_SOURCE_DIR}/installer.iss")


# Create configured .iss file from .in template
configure_file(
  ${PROJECT_SOURCE_DIR}/installer.iss.in
  ${CMAKE_CURRENT_BINARY_DIR}/installer_configured.iss
  @ONLY
)



# Inno Setup command
# Add a custom target to build the installer
# Custom target to run Inno Setup
add_custom_target(installer
  COMMAND "${ISCC_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/installer_configured.iss"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMENT "Building installer using Inno Setup"
  VERBATIM
)

# Define a target to copy the resources
add_custom_target(copy_resources
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/resources"
          "${CMAKE_CURRENT_BINARY_DIR}/resources"
  COMMENT "Copying resources to build directory"
)

# Ensure 'installer' depends on it
add_dependencies(installer copy_resources)
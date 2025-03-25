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

#uncomment if makensis is not in the path
set(MKNSIS "C:/Program Files (x86)/NSIS/Bin/makensis.exe")

#set(MKNSIS "makensis.exe")

# Configure files.
configure_file("${PROJECT_SOURCE_DIR}/version.nsi.in" "${PROJECT_SOURCE_DIR}/version.nsi")

file(GLOB COMMON_INSTALLERS_FILES "${PROJECT_SOURCE_DIR}/version.nsi" "${PROJECT_SOURCE_DIR}/dlg_helper.nsi" "${PROJECT_SOURCE_DIR}/resources/*")


message(NOTICE "---------------- Generate installer in ${PROJECT_SOURCE_DIR} ---------------")

# modify binary called from the application manager
# embedded in installer
add_custom_command(
  DEPENDS "${PROJECT_SOURCE_DIR}/centreon-monitoring-agent-modify.nsi"  ${COMMON_INSTALLERS_FILES} ${CENTREON_AGENT}
  COMMENT "--------- Generating cma configuration modifier --------"
  OUTPUT "${PROJECT_SOURCE_DIR}/centreon-monitoring-agent-modify.exe"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" 
  COMMAND ${MKNSIS} "${PROJECT_SOURCE_DIR}/centreon-monitoring-agent-modify.nsi")

add_custom_target("centreon-monitoring-agent-modifier" ALL DEPENDS "${PROJECT_SOURCE_DIR}/centreon-monitoring-agent-modify.exe")

configure_file("${PROJECT_SOURCE_DIR}/compile_installer.ps1.in" "${PROJECT_SOURCE_DIR}/compile_installer.ps1")

#final installer
add_custom_command(
  DEPENDS "${PROJECT_SOURCE_DIR}/centreon-monitoring-agent.nsi" "centreon-monitoring-agent-modifier" ${COMMON_INSTALLERS_FILES}  "${PROJECT_SOURCE_DIR}/compile_installer.ps1"
  COMMENT "--------- Generating cma configuration installer --------"
  OUTPUT "${PROJECT_SOURCE_DIR}/centreon-monitoring-agent.exe"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" 
  COMMAND pwsh.exe -File "${PROJECT_SOURCE_DIR}/compile_installer.ps1")

if (WITH_BUILD_AGENT_INSTALLER)
  add_custom_target("centreon-monitoring-agent-installer" ALL DEPENDS "${PROJECT_SOURCE_DIR}/centreon-monitoring-agent.exe")
endif()


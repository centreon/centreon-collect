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
set(OUTPUT_DIR "${PROJECT_SOURCE_DIR}")
set(OUTPUT_BASE_FILENAME_INSTALLER "centreon-monitoring-agent")
set(OUTPUT_BASE_FILENAME_MODIFIER "centreon-monitoring-agent-modify")

# Path to Inno Setup Compiler
set(ISCC_EXECUTABLE "C:/Program Files (x86)/Inno Setup 6/ISCC.exe")

if (WITH_BUILD_AGENT_INSTALLER)

# Download plugins
## Define GitHub URL
set(PLUGINS_RELEASE_URL "https://api.github.com/repos/centreon/centreon-nsclient-build/releases/latest")
set(RELEASE_JSON "${CMAKE_CURRENT_BINARY_DIR}/plugins_release.json")
file(DOWNLOAD "${PLUGINS_RELEASE_URL}" "${RELEASE_JSON}"
     STATUS status
     TIMEOUT 10)

list(GET status 0 status_code)
if(NOT status_code EQUAL 0)
  message(FATAL_ERROR "Failed to download release status: ${status}")
  exit(1)
endif()

file(READ "${RELEASE_JSON}" RELEASE_CONTENT)

# Get latest tag
string(JSON RELEASE_TAG GET "${RELEASE_CONTENT}" "tag_name")

# Get array length
string(JSON ASSETS_LENGTH LENGTH "${RELEASE_CONTENT}" "assets")

# Search for asset by name
set(CENTREON_PLUGIN_URL "")
math(EXPR LAST_INDEX "${ASSETS_LENGTH} - 1")
foreach(i RANGE 0 ${LAST_INDEX})
  # Extract the whole asset object
  string(JSON ASSET_JSON GET "${RELEASE_CONTENT}" "assets" "${i}")

  # Extract the 'name' field from the object
  string(JSON ASSET_NAME GET "${ASSET_JSON}" "name")
  if(ASSET_NAME STREQUAL "centreon_plugins.exe")
      string(JSON CENTREON_PLUGIN_URL GET "${ASSET_JSON}" "browser_download_url")
      break()
    endif()
endforeach()

if(CENTREON_PLUGIN_URL STREQUAL "")
  message(FATAL_ERROR "centreon_plugins.exe not found in latest release.")
  exit(1)
endif()

# Extract version number between /download/ and /centreon_plugins
string(REGEX MATCH "/download/([0-9]+)/centreon_plugins\\.exe" _match "${CENTREON_PLUGIN_URL}")

if(CMAKE_MATCH_COUNT GREATER 0)
    set(PLUGIN_VERSION "${CMAKE_MATCH_1}")
    message(STATUS "Latest centreon_plugins.exe version: ${PLUGIN_VERSION}")
else()
    message(WARNING "Could not extract version from centreon_plugins.exe URL: ${CENTREON_PLUGIN_URL}")
    set(PLUGIN_VERSION "")
endif()

# Download the centreon_plugins.exe
set(PLUGIN_DOWNLOAD_PATH "${CMAKE_CURRENT_BINARY_DIR}/centreon_plugins.exe")
file(DOWNLOAD "${CENTREON_PLUGIN_URL}" "${PLUGIN_DOWNLOAD_PATH}"
  STATUS status
  TIMEOUT 10)

#signning setup
if (WITH_INSTALLER_SIGNING)
  set(INNO_SIGNING_SETUP
"SignTool=TrustedSigning
SignedUninstaller=yes")
else()
  set(INNO_SIGNING_SETUP "")
endif()

set(INNO_SIGN_CMD "")
if (WITH_INSTALLER_SIGNING)
  set(INNO_SIGN_CMD
    "/STrustedSigning=pwsh.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ${PROJECT_SOURCE_DIR}/trusted-sign.ps1 $f"
  )
endif()


# Get the size of the downloaded file
if(EXISTS "${PLUGIN_DOWNLOAD_PATH}")
  file(SIZE "${PLUGIN_DOWNLOAD_PATH}" PLUGIN_SIZE)
endif()
# installer .iss file from .in template
configure_file(
  ${PROJECT_SOURCE_DIR}/installer.iss.in
  ${PROJECT_SOURCE_DIR}/installer_configured.iss
  @ONLY
)
endif()

# modifier .iss file from .in template
configure_file(
  ${PROJECT_SOURCE_DIR}/modifier.iss.in
  ${PROJECT_SOURCE_DIR}/modifier_configured.iss
  @ONLY
)


# Inno Setup command
# Add a custom target to build the  modifier/installer
# Modifier build
# ----- Modifier (standalone) -----
if (WITH_BUILD_AGENT_MODIFIER OR WITH_BUILD_AGENT_INSTALLER)
  add_custom_command(
    OUTPUT "${PROJECT_SOURCE_DIR}/centreon-monitoring-agent-modify.exe"
    DEPENDS "${PROJECT_SOURCE_DIR}/modifier_configured.iss" ${CENTREON_AGENT}
    COMMAND ${ISCC_EXECUTABLE} ${PROJECT_SOURCE_DIR}/modifier_configured.iss
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Building modifier using Inno Setup"
    VERBATIM)
  add_custom_target(centreon-monitoring-agent-modifier ALL
    DEPENDS "${PROJECT_SOURCE_DIR}/centreon-monitoring-agent-modify.exe")
endif()

# ----- Installer (only if enabled) -----
if (WITH_BUILD_AGENT_INSTALLER )
  add_custom_command(
    OUTPUT "${PROJECT_SOURCE_DIR}/centreon-monitoring-agent.exe"
    DEPENDS "${PROJECT_SOURCE_DIR}/centreon-monitoring-agent-modify.exe"
    COMMAND ${ISCC_EXECUTABLE} ${INNO_SIGN_CMD} "${PROJECT_SOURCE_DIR}/installer_configured.iss"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Building installer using Inno Setup"
    VERBATIM)
  add_custom_target(centreon-monitoring-agent-installer ALL
    DEPENDS "${PROJECT_SOURCE_DIR}/centreon-monitoring-agent.exe")
  add_dependencies(centreon-monitoring-agent-installer
                   centreon-monitoring-agent-modifier)
endif()
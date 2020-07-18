##
## Copyright 2014-2015 Centreon
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## For more information : contact@centreon.com
##

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR}/)

set(WITH_DAEMONS
    central-broker
    central-rrd
)

foreach (DAEMON_NAME IN LISTS WITH_DAEMONS)
  set(DAEMONS_CONFIGURATION
   "${DAEMONS_CONFIGURATION}        \"cbd\": [ \"name\": \"${DAEMON_NAME}\", \"configuration_file\": \"${CMAKE_INSTALL_FULL_SYSCONFDIR}/centreon-broker/${DAEMON_NAME}.json\", \"run\": true, \"reload\": true } ],\n")
endforeach ()

configure_file(${CMAKE_SOURCE_DIR}/config/watchdog.json.in
  ${CMAKE_BINARY_DIR}/watchdog.json
  @ONLY)

# Global options.
add_executable(cbwd
  # Sources.
  ${CMAKE_SOURCE_DIR}/src/cbwd/main.cc
  ${CMAKE_SOURCE_DIR}/src/cbwd/configuration.cc
  ${CMAKE_SOURCE_DIR}/src/cbwd/configuration_parser.cc
  ${CMAKE_SOURCE_DIR}/src/cbwd/instance.cc
  ${CMAKE_SOURCE_DIR}/src/cbwd/instance_configuration.cc
)
target_link_libraries(cbwd CONAN_PKG::spdlog CONAN_PKG::json11)
install(FILES ${CMAKE_BINARY_DIR}/watchdog.json COMPONENT cbd DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/centreon-broker)
install(TARGETS cbwd COMPONENT cbd DESTINATION ${CMAKE_INSTALL_SBINDIR})

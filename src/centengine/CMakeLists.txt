##
## Copyright 2011-2019 Centreon
##
## This file is part of Centreon Engine.
##
## Centreon Engine is free software: you can redistribute it and/or
## modify it under the terms of the GNU General Public License version 2
## as published by the Free Software Foundation.
##
## Centreon Engine is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with Centreon Engine. If not, see
## <http://www.gnu.org/licenses/>.
##
link_directories(${CONAN_LIB_DIRS_GRPC})

add_definitions(-DDEFAULT_LOG_FILE="${VAR_DIR}/centengine.log")
add_definitions(-DDEFAULT_LOG_ARCHIVE_PATH="${LOG_ARCHIVE_DIR}")
add_definitions(-DDEFAULT_DEBUG_FILE="${VAR_DIR}/centengine.debug")
add_definitions(-DDEFAULT_RETENTION_FILE="${VAR_DIR}/retention.dat")
add_definitions(-DDEFAULT_COMMAND_FILE="${RW_DIR}/centengine.cmd")

# centengine target.
add_executable(centengine
  ${CMAKE_SOURCE_DIR}/src/centengine/engine_impl.cc
  ${CMAKE_SOURCE_DIR}/src/centengine/enginerpc.cc
  ${CMAKE_SOURCE_DIR}/src/centengine/main.cc)

find_package(Protobuf MODULE)
MESSAGE(INFO "pb ${Protobuf_IMPORT_DIRS} gRPC::grpc++_reflection ")
# Link centengine with required libraries.
target_link_libraries(centengine "-Wl,-whole-archive" cce_rpc cce_core grpc++_reflection "-Wl,-no-whole-archive")

configure_file("${CMAKE_SOURCE_DIR}/script/systemd/centengine.service.in" "${CMAKE_BINARY_DIR}/centengine.service" @ONLY IMMEDIATE)
configure_file("${CMAKE_SOURCE_DIR}/config/centengine.cfg.in" "${CMAKE_BINARY_DIR}/centengine.cfg" @ONLY IMMEDIATE)
configure_file("${CMAKE_SOURCE_DIR}/config/resource.cfg.in" "${CMAKE_BINARY_DIR}/resource.cfg" @ONLY IMMEDIATE)
configure_file("${CMAKE_SOURCE_DIR}/config/objects/commands.cfg.in" "${CMAKE_BINARY_DIR}/commands.cfg" @ONLY IMMEDIATE)
configure_file("${CMAKE_SOURCE_DIR}/config/objects/contacts.cfg.in" "${CMAKE_BINARY_DIR}/contacts.cfg" @ONLY IMMEDIATE)
configure_file("${CMAKE_SOURCE_DIR}/config/objects/localhost.cfg.in" "${CMAKE_BINARY_DIR}/localhost.cfg" @ONLY IMMEDIATE)
configure_file("${CMAKE_SOURCE_DIR}/config/objects/printer.cfg.in" "${CMAKE_BINARY_DIR}/printer.cfg" @ONLY IMMEDIATE)
configure_file("${CMAKE_SOURCE_DIR}/config/objects/switch.cfg.in" "${CMAKE_BINARY_DIR}/switch.cfg" @ONLY IMMEDIATE)
configure_file("${CMAKE_SOURCE_DIR}/config/objects/templates.cfg.in" "${CMAKE_BINARY_DIR}/templates.cfg" @ONLY IMMEDIATE)
configure_file("${CMAKE_SOURCE_DIR}/config/objects/timeperiods.cfg.in" "${CMAKE_BINARY_DIR}/timeperiods.cfg" @ONLY IMMEDIATE)
configure_file("${CMAKE_SOURCE_DIR}/config/objects/windows.cfg.in" "${CMAKE_BINARY_DIR}/windows.cfg" @ONLY IMMEDIATE)

install(FILES ${CMAKE_BINARY_DIR}/centengine.service COMPONENT engine-daemon DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
install(FILES ${CMAKE_BINARY_DIR}/centengine.cfg COMPONENT engine-daemon DESTINATION /etc/centreon-engine)
install(FILES ${CMAKE_BINARY_DIR}/resource.cfg COMPONENT engine-daemon DESTINATION /etc/centreon-engine)
install(FILES ${CMAKE_BINARY_DIR}/commands.cfg COMPONENT engine-daemon DESTINATION /etc/centreon-engine/objects)
install(FILES ${CMAKE_BINARY_DIR}/contacts.cfg COMPONENT engine-daemon DESTINATION /etc/centreon-engine/objects)
install(FILES ${CMAKE_BINARY_DIR}/printer.cfg COMPONENT engine-daemon DESTINATION /etc/centreon-engine/objects)
install(FILES ${CMAKE_BINARY_DIR}/localhost.cfg COMPONENT engine-daemon DESTINATION /etc/centreon-engine/objects)
install(FILES ${CMAKE_BINARY_DIR}/switch.cfg COMPONENT engine-daemon DESTINATION /etc/centreon-engine/objects)
install(FILES ${CMAKE_BINARY_DIR}/templates.cfg COMPONENT engine-daemon DESTINATION /etc/centreon-engine/objects)
install(FILES ${CMAKE_BINARY_DIR}/timeperiods.cfg COMPONENT engine-daemon DESTINATION /etc/centreon-engine/objects)
install(FILES ${CMAKE_BINARY_DIR}/centengine.cfg COMPONENT engine-daemon DESTINATION /etc/centreon-engine/objects)
install(TARGETS centengine COMPONENT engine-daemon DESTINATION ${CMAKE_INSTALL_SBINDIR})

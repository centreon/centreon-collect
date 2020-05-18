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

install(TARGETS centengine DESTINATION ${CMAKE_INSTALL_SBINDIR})
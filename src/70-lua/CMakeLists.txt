##
## Copyright 2013 Centreon
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

# TLS module.
add_library(70-lua SHARED
  # Sources
  ${CMAKE_SOURCE_DIR}/src/70-lua/broker_cache.cc
  ${CMAKE_SOURCE_DIR}/src/70-lua/broker_log.cc
  ${CMAKE_SOURCE_DIR}/src/70-lua/broker_socket.cc
  ${CMAKE_SOURCE_DIR}/src/70-lua/broker_utils.cc
  ${CMAKE_SOURCE_DIR}/src/70-lua/connector.cc
  ${CMAKE_SOURCE_DIR}/src/70-lua/factory.cc
  ${CMAKE_SOURCE_DIR}/src/70-lua/luabinding.cc
  ${CMAKE_SOURCE_DIR}/src/70-lua/macro_cache.cc
  ${CMAKE_SOURCE_DIR}/src/70-lua/main.cc
  ${CMAKE_SOURCE_DIR}/src/70-lua/stream.cc
)

target_link_libraries(70-lua CONAN_PKG::lua CONAN_PKG::json11 CONAN_PKG::spdlog CONAN_PKG::asio)
set_target_properties(70-lua PROPERTIES PREFIX "")
install(TARGETS 70-lua DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/centreon/lib/centreon-broker)

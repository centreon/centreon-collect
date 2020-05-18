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
include_directories(${CONAN_INCLUDE_DIRS_JSON11})

# TLS module.
add_library(50-tcp SHARED
  # Sources
  ${CMAKE_SOURCE_DIR}/src/50-tcp/acceptor.cc
  ${CMAKE_SOURCE_DIR}/src/50-tcp/connector.cc
  ${CMAKE_SOURCE_DIR}/src/50-tcp/factory.cc
  ${CMAKE_SOURCE_DIR}/src/50-tcp/main.cc
  ${CMAKE_SOURCE_DIR}/src/50-tcp/stream.cc
  ${CMAKE_SOURCE_DIR}/src/50-tcp/tcp_async.cc
)

target_link_libraries(50-tcp CONAN_PKG::spdlog CONAN_PKG::asio)
set_target_properties(50-tcp PROPERTIES PREFIX "")
install(TARGETS 50-tcp DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/centreon/lib/centreon-broker)
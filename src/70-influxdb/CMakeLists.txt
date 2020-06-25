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
include_directories(${CONAN_INCLUDE_DIRS_SPDLOG})

# TLS module.
add_library(70-influxdb SHARED
  # Sources
  ${CMAKE_SOURCE_DIR}/src/70-influxdb/column.cc
  ${CMAKE_SOURCE_DIR}/src/70-influxdb/connector.cc
  ${CMAKE_SOURCE_DIR}/src/70-influxdb/factory.cc
  ${CMAKE_SOURCE_DIR}/src/70-influxdb/influxdb12.cc
  ${CMAKE_SOURCE_DIR}/src/70-influxdb/line_protocol_query.cc
  ${CMAKE_SOURCE_DIR}/src/70-influxdb/macro_cache.cc
  ${CMAKE_SOURCE_DIR}/src/70-influxdb/main.cc
  ${CMAKE_SOURCE_DIR}/src/70-influxdb/stream.cc
)
set_target_properties(70-influxdb PROPERTIES PREFIX "")
target_link_libraries(70-influxdb CONAN_PKG::asio CONAN_PKG::fmt)
install(TARGETS 70-influxdb DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/centreon/lib/centreon-broker)

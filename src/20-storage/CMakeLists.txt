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
include(${CMAKE_SOURCE_DIR}/cmake/FindMariaDB.cmake)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CONAN_INCLUDE_DIRS_JSON11})


# TLS module.
add_library(20-storage SHARED
  # Sources
  ${CMAKE_SOURCE_DIR}/src/20-storage/connector.cc
  ${CMAKE_SOURCE_DIR}/src/20-storage/exceptions/perfdata.cc
  ${CMAKE_SOURCE_DIR}/src/20-storage/factory.cc
  ${CMAKE_SOURCE_DIR}/src/20-storage/index_mapping.cc
  ${CMAKE_SOURCE_DIR}/src/20-storage/main.cc
  ${CMAKE_SOURCE_DIR}/src/20-storage/metric.cc
  ${CMAKE_SOURCE_DIR}/src/20-storage/metric_mapping.cc
  ${CMAKE_SOURCE_DIR}/src/20-storage/parser.cc
  ${CMAKE_SOURCE_DIR}/src/20-storage/perfdata.cc
  ${CMAKE_SOURCE_DIR}/src/20-storage/rebuild.cc
  ${CMAKE_SOURCE_DIR}/src/20-storage/rebuilder.cc
  ${CMAKE_SOURCE_DIR}/src/20-storage/remove_graph.cc
  ${CMAKE_SOURCE_DIR}/src/20-storage/status.cc
  ${CMAKE_SOURCE_DIR}/src/20-storage/stream.cc
)

target_link_libraries(20-storage CONAN_PKG::spdlog ccb_conflict_manager ccb_sql)
set_target_properties(20-storage PROPERTIES COMPILE_FLAGS "${MYSQL_CFLAGS} -fPIC")
set_target_properties(20-storage PROPERTIES PREFIX "")
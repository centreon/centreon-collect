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
add_definitions(-DCENTREON_BROKER_VERSION=\"${COLLECT_VERSION}\")
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CONAN_INCLUDE_DIRS_JSON11})

# TLS module.
add_library(10-neb
  SHARED
  # Sources
  ${CMAKE_SOURCE_DIR}/src/10-neb/broker.cc
  ${CMAKE_SOURCE_DIR}/src/10-neb/downtime_map.cc
  ${CMAKE_SOURCE_DIR}/src/10-neb/downtime_scheduler.cc
  ${CMAKE_SOURCE_DIR}/src/10-neb/downtime_serializable.cc
  ${CMAKE_SOURCE_DIR}/src/10-neb/node_cache.cc
  ${CMAKE_SOURCE_DIR}/src/10-neb/node_id.cc
)

target_link_libraries(10-neb CONAN_PKG::spdlog ccb_neb)
set_target_properties(10-neb PROPERTIES PREFIX "")
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

include(${CMAKE_SOURCE_DIR}/cmake/FindRrd.cmake)

include_directories(${LIBRRD_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CONAN_INCLUDE_DIRS_JSON11})
include_directories(${CONAN_INCLUDE_DIRS_SPDLOG})
# TLS module.
add_library(70-rrd SHARED
  # Sources
  ${CMAKE_SOURCE_DIR}/src/70-rrd/backend.cc
  ${CMAKE_SOURCE_DIR}/src/70-rrd/cached.cc
  ${CMAKE_SOURCE_DIR}/src/70-rrd/connector.cc
  ${CMAKE_SOURCE_DIR}/src/70-rrd/creator.cc
  ${CMAKE_SOURCE_DIR}/src/70-rrd/exceptions/open.cc
  ${CMAKE_SOURCE_DIR}/src/70-rrd/exceptions/update.cc
  ${CMAKE_SOURCE_DIR}/src/70-rrd/factory.cc
  ${CMAKE_SOURCE_DIR}/src/70-rrd/lib.cc
  ${CMAKE_SOURCE_DIR}/src/70-rrd/main.cc
  ${CMAKE_SOURCE_DIR}/src/70-rrd/output.cc
)

target_link_libraries(70-rrd ${LIBRRD_LDFLAGS} CONAN_PKG::spdlog)
set_target_properties(70-rrd PROPERTIES PREFIX "")
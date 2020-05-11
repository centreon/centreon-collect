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
add_library(ccb_neb STATIC
  # Sources
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/acknowledgement.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/check.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/comment.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/custom_variable.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/custom_variable_status.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/dependency.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/downtime.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/event_handler.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/flapping_status.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/group.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/group_member.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/host.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/host_check.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/host_dependency.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/host_group.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/host_group_member.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/host_parent.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/host_service.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/host_service_status.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/host_status.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/instance.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/instance_configuration.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/instance_status.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/log_entry.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/module.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/responsive_instance.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/service.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/service_check.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/service_dependency.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/service_group.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/service_group_member.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/service_status.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_neb/status.cc
)

target_link_libraries(ccb_neb CONAN_PKG::spdlog)
set_target_properties(ccb_neb PROPERTIES COMPILE_FLAGS "${MYSQL_CFLAGS} -fPIC")
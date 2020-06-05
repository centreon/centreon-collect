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
include_directories(${CMAKE_BINARY_DIR})

set_source_files_properties("${CMAKE_BINARY_DIR}/table_max_size.hh" PROPERTIES GENERATED TRUE)

# TLS module.
add_library(ccb_conflict_manager STATIC
  # Sources
  ${CMAKE_SOURCE_DIR}/src/ccb_conflict_manager/conflict_manager.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_conflict_manager/conflict_manager_sql.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_conflict_manager/conflict_manager_storage.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_conflict_manager/stored_timestamp.cc
  ${CMAKE_BINARY_DIR}/table_max_size.hh
)

add_dependencies(ccb_conflict_manager table_max_size)
target_link_libraries(ccb_conflict_manager CONAN_PKG::spdlog)
set_target_properties(ccb_conflict_manager PROPERTIES COMPILE_FLAGS "${MYSQL_CFLAGS} -fPIC")

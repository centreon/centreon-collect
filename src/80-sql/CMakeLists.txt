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
add_library(80-sql SHARED
  # Sources
  ${CMAKE_SOURCE_DIR}/src/80-sql/cleanup.cc
  ${CMAKE_SOURCE_DIR}/src/80-sql/connector.cc
  ${CMAKE_SOURCE_DIR}/src/80-sql/factory.cc
  ${CMAKE_SOURCE_DIR}/src/80-sql/main.cc
  ${CMAKE_SOURCE_DIR}/src/80-sql/stream.cc
)

target_link_libraries(80-sql CONAN_PKG::spdlog)
set_target_properties(80-sql PROPERTIES COMPILE_FLAGS "${MYSQL_CFLAGS} -fPIC")
set_target_properties(80-sql PROPERTIES PREFIX "")
install(TARGETS 80-sql DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/centreon/lib/centreon-broker)
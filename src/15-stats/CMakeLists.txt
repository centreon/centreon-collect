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
add_library(15-stats
  SHARED
  # Sources
  ${CMAKE_SOURCE_DIR}/src/ccb_core/stats/helper.cc
  ${CMAKE_SOURCE_DIR}/src/15-stats/builder.cc
  ${CMAKE_SOURCE_DIR}/src/15-stats/main.cc
  ${CMAKE_SOURCE_DIR}/src/15-stats/parser.cc
  ${CMAKE_SOURCE_DIR}/src/15-stats/worker.cc
  ${CMAKE_SOURCE_DIR}/src/15-stats/worker_pool.cc
)

set_target_properties(15-stats PROPERTIES COMPILE_FLAGS ${MYSQL_CFLAGS})
target_link_libraries(15-stats CONAN_PKG::spdlog CONAN_PKG::asio)
set_target_properties(15-stats PROPERTIES PREFIX "")
install(TARGETS 15-stats DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/centreon/lib/centreon-broker)
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
include(${CMAKE_SOURCE_DIR}/cmake/FindGnuTls.cmake)

include_directories(${GNUTLS_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CONAN_INCLUDE_DIRS_JSON11})

# TLS module.
add_library(60-tls SHARED
  # Sources
  ${CMAKE_SOURCE_DIR}/src/60-tls/acceptor.cc
  ${CMAKE_SOURCE_DIR}/src/60-tls/connector.cc
  ${CMAKE_SOURCE_DIR}/src/60-tls/factory.cc
  ${CMAKE_SOURCE_DIR}/src/60-tls/internal.cc
  ${CMAKE_SOURCE_DIR}/src/60-tls/main.cc
  ${CMAKE_SOURCE_DIR}/src/60-tls/params.cc
  ${CMAKE_SOURCE_DIR}/src/60-tls/stream.cc
)

target_link_libraries(60-tls ${GNUTLS_LIBRARIES} CONAN_PKG::spdlog)
set_target_properties(60-tls PROPERTIES PREFIX "")
install(TARGETS 60-tls DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/centreon/lib/centreon-broker)
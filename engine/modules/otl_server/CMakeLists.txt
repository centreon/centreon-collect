#
# Copyright 2009-2024 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# For more information : contact@centreon.com
#

# Set directories.
set(MODULE_DIR "${PROJECT_SOURCE_DIR}/modules/otl_server")
set(SRC_DIR "${MODULE_DIR}/src")
set(INC_DIR "${MODULE_DIR}/inc/com/centreon/engine/modules/otl_server")

# Include directories.
include_directories("${MODULE_DIR}/inc")

# mod_externalcmd target.
add_library(otl_server SHARED 
"${SRC_DIR}/grpc_config.cc"
"${SRC_DIR}/otl_config.cc"
"${SRC_DIR}/otl_server.cc"
"${SRC_DIR}/main.cc"  
)

target_precompile_headers(otl_server PRIVATE precomp_inc/precomp.hh)

# set(EXTERNALCMD_MODULE "${EXTERNALCMD_MODULE}" PARENT_SCOPE)
target_link_libraries(otl_server 
  centreon_common
  CONAN_PKG::spdlog
  CONAN_PKG::rapidjson
  -L${CONAN_LIB_DIRS_OPENTELEMETRY-CPP})

target_include_directories(otl_server PRIVATE ${INC_DIR} ${CMAKE_SOURCE_DIR}/common/inc )

# Install rule.
install(
  TARGETS "otl_server"
  DESTINATION "${ENGINE_MODULES_DIR}"
  COMPONENT "runtime")

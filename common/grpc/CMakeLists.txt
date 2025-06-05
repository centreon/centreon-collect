#
# Copyright 2024 Centreon
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

# Global options.

set(INC_DIR "${PROJECT_SOURCE_DIR}/grpc/inc")
set(SRC_DIR "src")

add_library(centreon_grpc STATIC
  ${SRC_DIR}/grpc_client.cc
  ${SRC_DIR}/grpc_server.cc)

target_include_directories(centreon_grpc PRIVATE ${INC_DIR} ${CMAKE_SOURCE_DIR}/common/inc)

target_precompile_headers(centreon_grpc REUSE_FROM centreon_common)

set_target_properties(centreon_grpc PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Link the ctncrypto library with centreon_grpc.
target_link_libraries(centreon_grpc
  PRIVATE
    ctncrypto
)

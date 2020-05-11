##
## Copyright 2019 Centreon (https://www.centreon.com/)
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
## http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## For more information : contact@centreon.com
##
##



add_custom_command(
  OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
  COMMAND protobuf::protoc
  ARGS --grpc_out "${CMAKE_BINARY_DIR}"
  --cpp_out "${CMAKE_BINARY_DIR}"
  -I "${hw_proto_path}"
  --plugin=protoc-gen-grpc="$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
  "${hw_proto}"
  DEPENDS "${hw_proto}")

add_library(
    cerpc
    STATIC

  ${hw_proto_srcs}
  ${hw_proto_hdrs}
  ${hw_grpc_srcs}
  ${hw_grpc_hdrs})

target_link_libraries(cerpc CONAN_PKG::protobuf CONAN_PKG::grpc CONAN_PKG::json11 centreon-clib)

# mod_enginerpc target.
add_library(
  enginerpc
  STATIC

  # Sources.
  ${CMAKE_SOURCE_DIR}/src/engine/enginerpc/engine_impl.cc
  ${CMAKE_SOURCE_DIR}/src/engine/enginerpc/enginerpc.cc

  # Headers.
  ${CMAKE_SOURCE_DIR}/src/engine/enginerpc/engine_impl.hh
  ${CMAKE_SOURCE_DIR}/src/engine/enginerpc/enginerpc.hh
)

# Prettier name.
set_property(TARGET enginerpc PROPERTY PREFIX "" PROPERTY POSITION_INDEPENDENT_CODE ON)
# Link target with libraries.
target_link_libraries(enginerpc cerpc)

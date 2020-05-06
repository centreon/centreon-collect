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

# Set directories.
set(ENGINERPC_DIR "${PROJECT_SOURCE_DIR}/enginerpc")
set(ENGINERPC_DIR "${PROJECT_SOURCE_DIR}/enginerpc" PARENT_SCOPE)

set(ENGINERPC enginerpc)
set(ENGINERPC enginerpc PARENT_SCOPE)
# Include directories.
include_directories(
  ${ENGINERPC_DIR}
)


find_package(protoc CONFIG)
find_package(protobuf CONFIG)

include_directories(${CMAKE_BINARY_DIR})

get_filename_component(hw_proto "${CMAKE_SOURCE_DIR}/engine/enginerpc/engine.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

set(hw_proto_srcs "${CMAKE_BINARY_DIR}/engine.pb.cc")
set(hw_proto_hdrs "${CMAKE_BINARY_DIR}/engine.pb.h")
set(hw_grpc_srcs "${CMAKE_BINARY_DIR}/engine.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_BINARY_DIR}/engine.grpc.pb.h")

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
  ${CMAKE_SOURCE_DIR}/engine/enginerpc/engine_impl.cc
  ${CMAKE_SOURCE_DIR}/engine/enginerpc/enginerpc.cc

  # Headers.
  ${CMAKE_SOURCE_DIR}/engine/enginerpc/engine_impl.hh
  ${CMAKE_SOURCE_DIR}/engine/enginerpc/enginerpc.hh
)

# Prettier name.
set_property(TARGET enginerpc PROPERTY PREFIX "" PROPERTY POSITION_INDEPENDENT_CODE ON)
# Link target with libraries.
target_link_libraries(enginerpc cerpc)

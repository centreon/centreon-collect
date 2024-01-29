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

#protobuf
set(protobuf_files
    opentelemetry/proto/collector/metrics/v1/metrics_service
    opentelemetry/proto/metrics/v1/metrics
    opentelemetry/proto/common/v1/common
    opentelemetry/proto/resource/v1/resource
)

foreach(name IN LISTS protobuf_files)
  set(proto_file "${name}.proto")
  add_custom_command(
    OUTPUT "${SRC_DIR}/${name}.pb.cc"
    COMMENT "Generating interface files of the otl file ${proto_file}"
    DEPENDS ${CONAN_RES_DIRS_OPENTELEMETRY-PROTO}/${proto_file}
    COMMAND
      ${Protobuf_PROTOC_EXECUTABLE} ARGS --cpp_out=${SRC_DIR}
      --proto_path=${CONAN_RES_DIRS_OPENTELEMETRY-PROTO} ${proto_file}
    VERBATIM)

endforeach()


#service
set(service_files
  opentelemetry/proto/collector/metrics/v1/metrics_service
)

foreach(name IN LISTS service_files)
  set(proto_file "${name}.proto")
  add_custom_command(
    OUTPUT "${SRC_DIR}/${name}.grpc.pb.cc"
    COMMENT "Generating grpc files of the otl service file ${proto_file}"
    DEPENDS ${CONAN_RES_DIRS_OPENTELEMETRY-PROTO}/${proto_file}
    COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS
    --plugin=protoc-gen-grpc=${CONAN_BIN_DIRS_GRPC}/grpc_cpp_plugin
    --proto_path=${CONAN_RES_DIRS_OPENTELEMETRY-PROTO} 
    --grpc_out=${SRC_DIR} ${proto_file}
    VERBATIM)

endforeach()



# mod_externalcmd target.
add_library(otl_server SHARED 
${SRC_DIR}/grpc_config.cc
${SRC_DIR}/otl_config.cc
${SRC_DIR}/otl_server.cc
${SRC_DIR}/main.cc
${SRC_DIR}/opentelemetry/proto/collector/metrics/v1/metrics_service.pb.cc
${SRC_DIR}/opentelemetry/proto/metrics/v1/metrics.pb.cc
${SRC_DIR}/opentelemetry/proto/common/v1/common.pb.cc
${SRC_DIR}/opentelemetry/proto/resource/v1/resource.pb.cc
${SRC_DIR}/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.cc
)

target_precompile_headers(otl_server PRIVATE precomp_inc/precomp.hh)

# set(EXTERNALCMD_MODULE "${EXTERNALCMD_MODULE}" PARENT_SCOPE)
target_link_libraries(otl_server 
  centreon_common
  CONAN_PKG::spdlog
  CONAN_PKG::rapidjson
  )

target_include_directories(otl_server PRIVATE ${INC_DIR} ${CMAKE_SOURCE_DIR}/common/inc src )

# Install rule.
install(
  TARGETS "otl_server"
  DESTINATION "${ENGINE_MODULES_DIR}"
  COMPONENT "runtime")

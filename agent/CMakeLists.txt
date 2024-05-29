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
project("Centreon agent" C CXX)

# Set directories.
set(INCLUDE_DIR "${PROJECT_SOURCE_DIR}/inc/com/centreon/agent")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")


add_definitions("-D_GLIBCXX_USE_CXX11_ABI=1")
add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

option(WITH_LIBCXX "compiles and link cbd with clang++/libc++")

if(WITH_LIBCXX)
  set(CMAKE_CXX_COMPILER "clang++")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

  # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -Werror -O1
  # -fno-omit-frame-pointer")
endif()

#otel service
set(service_files
  opentelemetry/proto/collector/metrics/v1/metrics_service
)

foreach(name IN LISTS service_files)
  set(proto_file "${name}.proto")
  add_custom_command(
    OUTPUT "${SRC_DIR}/${name}.grpc.pb.cc"
    COMMENT "Generating grpc files of the otl service file ${proto_file}"
    DEPENDS opentelemetry-proto-files
    COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS
    --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
    --proto_path=${CMAKE_SOURCE_DIR}/opentelemetry-proto 
    --grpc_out=${SRC_DIR} ${proto_file}
    VERBATIM
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

endforeach()

set(otl_protobuf_files
    opentelemetry/proto/collector/metrics/v1/metrics_service
    opentelemetry/proto/metrics/v1/metrics
    opentelemetry/proto/common/v1/common
    opentelemetry/proto/resource/v1/resource
)
foreach(name IN LISTS otl_protobuf_files)
  set(proto_file "${name}.proto")
  add_custom_command(
    OUTPUT "${SRC_DIR}/${name}.pb.cc"
    COMMENT "Generating interface files of the otl file ${proto_file}"
    DEPENDS opentelemetry-proto-files
    COMMAND
      ${Protobuf_PROTOC_EXECUTABLE} ARGS --cpp_out=${SRC_DIR}
      --proto_path=${CMAKE_SOURCE_DIR}/opentelemetry-proto ${proto_file}
    VERBATIM)
endforeach()


#centreon_agent server and client
add_custom_command(
  DEPENDS ${PROJECT_SOURCE_DIR}/proto/agent.proto
  COMMENT "Generating interface files of the conf centreon_agent proto file (grpc)"
  OUTPUT ${SRC_DIR}/agent.grpc.pb.cc
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS
    --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
    --proto_path=${PROJECT_SOURCE_DIR}/proto --proto_path=${CMAKE_SOURCE_DIR}/opentelemetry-proto
    --grpc_out=${SRC_DIR} ${PROJECT_SOURCE_DIR}/proto/agent.proto
  DEPENDS ${PROJECT_SOURCE_DIR}/proto/agent.proto
  COMMENT "Generating interface files of the conf centreon_agent proto file (protobuf)"
  OUTPUT ${SRC_DIR}/agent.pb.cc
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS --cpp_out=${SRC_DIR}
    --proto_path=${PROJECT_SOURCE_DIR}/proto --proto_path=${CMAKE_SOURCE_DIR}/opentelemetry-proto
    ${PROJECT_SOURCE_DIR}/proto/agent.proto
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})


add_library(centreon_agent_lib STATIC
    ${SRC_DIR}/agent.grpc.pb.cc
    ${SRC_DIR}/agent.pb.cc
    ${SRC_DIR}/check.cc
    ${SRC_DIR}/check_exec.cc
    ${SRC_DIR}/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.cc
    ${SRC_DIR}/opentelemetry/proto/collector/metrics/v1/metrics_service.pb.cc
    ${SRC_DIR}/opentelemetry/proto/metrics/v1/metrics.pb.cc
    ${SRC_DIR}/opentelemetry/proto/common/v1/common.pb.cc
    ${SRC_DIR}/opentelemetry/proto/resource/v1/resource.pb.cc
    ${SRC_DIR}/scheduler.cc
)

include_directories(
  ${INCLUDE_DIR} 
  ${SRC_DIR} 
  ${CMAKE_SOURCE_DIR}/common/inc
)

target_precompile_headers(centreon_agent_lib PRIVATE precomp_inc/precomp.hh)

add_executable(centreon_agent ${SRC_DIR}/main.cc)

target_link_libraries(
    centreon_agent PRIVATE
  -L${PROTOBUF_LIB_DIR}
  gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts
#  cerpc
#  berpc
  centreon_agent_lib
  centreon_common
  -L${Boost_LIBRARY_DIR_RELEASE}
  boost_program_options
  fmt::fmt)

target_precompile_headers(centreon_agent  REUSE_FROM centreon_agent_lib)

target_include_directories(centreon_agent PRIVATE 
  ${INCLUDE_DIR} 
  ${SRC_DIR} 
  ${CMAKE_SOURCE_DIR}/common/inc
)

install(TARGETS centreon_agent RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

if(WITH_TESTING)
  add_subdirectory(test)
endif()

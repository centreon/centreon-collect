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
project("Centreon agent" CXX)

# Set directories.
set(INCLUDE_DIR "${PROJECT_SOURCE_DIR}/inc/com/centreon/agent")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(SCRIPT_DIR "${PROJECT_SOURCE_DIR}/scripts")


add_definitions("-D_GLIBCXX_USE_CXX11_ABI=1")
add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

option(WITH_LIBCXX "compiles and links cbd with clang++/libc++")

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
    COMMENT "Generating grpc files from the otl service file ${proto_file}"
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
    COMMENT "Generating interface files from the otl file ${proto_file}"
    DEPENDS opentelemetry-proto-files
    COMMAND
      ${Protobuf_PROTOC_EXECUTABLE} ARGS --cpp_out=${SRC_DIR}
      --proto_path=${CMAKE_SOURCE_DIR}/opentelemetry-proto ${proto_file}
    VERBATIM)
endforeach()


#centagent server and client
add_custom_command(
  DEPENDS ${PROJECT_SOURCE_DIR}/proto/agent.proto
  COMMENT "Generating interface files from the conf centagent proto file (grpc)"
  OUTPUT ${SRC_DIR}/agent.grpc.pb.cc
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS
    --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
    --proto_path=${PROJECT_SOURCE_DIR}/proto --proto_path=${CMAKE_SOURCE_DIR}/opentelemetry-proto
    --grpc_out=${SRC_DIR} ${PROJECT_SOURCE_DIR}/proto/agent.proto
  DEPENDS ${PROJECT_SOURCE_DIR}/proto/agent.proto
  COMMENT "Generating interface files from the conf centagent proto file (protobuf)"
  OUTPUT ${SRC_DIR}/agent.pb.cc
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS --cpp_out=${SRC_DIR}
    --proto_path=${PROJECT_SOURCE_DIR}/proto --proto_path=${CMAKE_SOURCE_DIR}/opentelemetry-proto
    ${PROJECT_SOURCE_DIR}/proto/agent.proto
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(NATIVE_DIR "native_linux")
else()
  set(NATIVE_DIR "native_windows")
endif()

set(NATIVE_INC "${PROJECT_SOURCE_DIR}/${NATIVE_DIR}/inc/com/centreon/agent")
set(NATIVE_SRC "${PROJECT_SOURCE_DIR}/${NATIVE_DIR}/src")

set( SRC_COMMON
  ${NATIVE_SRC}/agent_info.cc
  ${NATIVE_SRC}/check_cpu.cc
  ${SRC_DIR}/agent.grpc.pb.cc
  ${SRC_DIR}/agent.pb.cc
  ${SRC_DIR}/bireactor.cc
  ${SRC_DIR}/check.cc
  ${SRC_DIR}/check_exec.cc
  ${SRC_DIR}/drive_size.cc
  ${SRC_DIR}/check_health.cc
  ${SRC_DIR}/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.cc
  ${SRC_DIR}/opentelemetry/proto/collector/metrics/v1/metrics_service.pb.cc
  ${SRC_DIR}/opentelemetry/proto/metrics/v1/metrics.pb.cc
  ${SRC_DIR}/opentelemetry/proto/common/v1/common.pb.cc
  ${SRC_DIR}/opentelemetry/proto/resource/v1/resource.pb.cc
  ${SRC_DIR}/scheduler.cc
  ${SRC_DIR}/streaming_client.cc
  ${SRC_DIR}/streaming_server.cc
)

set( SRC_WINDOWS
  ${SRC_DIR}/config_win.cc
  ${NATIVE_SRC}/check_uptime.cc
  ${NATIVE_SRC}/check_drive_size.cc
  ${NATIVE_SRC}/check_memory.cc
  ${NATIVE_SRC}/check_service.cc
  ${NATIVE_SRC}/ntdll.cc
)

set( SRC_LINUX
  ${SRC_DIR}/config.cc
)

#resource version
configure_file("${SRC_DIR}/agent.rc.in"
              "${SRC_DIR}/agent.rc")
configure_file("${INCLUDE_DIR}/version.hh.in"
               "${INCLUDE_DIR}/version.hh")


if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(SRC ${SRC_COMMON} ${SRC_LINUX})
else()
  set(SRC ${SRC_COMMON} ${SRC_WINDOWS})
endif()


add_library(centagent_lib STATIC
  ${SRC}
)

include_directories(
  ${INCLUDE_DIR}
  ${NATIVE_INC} 
  ${SRC_DIR} 
  ${CMAKE_SOURCE_DIR}/common/inc
  ${CMAKE_SOURCE_DIR}/common/grpc/inc
  ${CMAKE_SOURCE_DIR}/common/process/inc
)

target_precompile_headers(centagent_lib PRIVATE precomp_inc/precomp.hh)

SET(CENTREON_AGENT centagent)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  add_executable(${CENTREON_AGENT} ${SRC_DIR}/main.cc)

  target_link_libraries(
    ${CENTREON_AGENT} PRIVATE
    -L${PROTOBUF_LIB_DIR}
    gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts
    centagent_lib
    centreon_common
    centreon_grpc
    centreon_process
    -L${Boost_LIBRARY_DIR_RELEASE}
    boost_program_options
    fmt::fmt
    stdc++fs)
else()
  add_executable(${CENTREON_AGENT} ${SRC_DIR}/main_win.cc  ${SRC_DIR}/agent.rc)

  target_link_libraries(
    ${CENTREON_AGENT} PRIVATE
    centagent_lib
    centreon_common
    centreon_grpc
    centreon_process
    gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts
    absl::any absl::log absl::base absl::bits
    Boost::program_options
    fmt::fmt
    pdh)

    if(WITH_BUILD_AGENT_INSTALLER  OR WITH_BUILD_AGENT_MODIFIER)
      add_subdirectory(installer)
    endif()

endif()


target_precompile_headers(${CENTREON_AGENT}  REUSE_FROM centagent_lib)

set(AGENT_VAR_LOG_DIR
    "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log/centreon-monitoring-agent")


install(TARGETS ${CENTREON_AGENT} RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

if(WITH_TESTING)
  add_subdirectory(test)
endif()


set(PREFIX_AGENT_CONF "${CMAKE_INSTALL_FULL_SYSCONFDIR}/centreon-monitoring-agent")
set(USER_AGENT centreon-monitoring-agent)


if(WITH_CONF)
  add_subdirectory(conf)
endif()

# Generate Systemd script.
message(STATUS "Generating systemd startup script.")
configure_file("${SCRIPT_DIR}/centagent.service.in"
                "${SCRIPT_DIR}/centagent.service")

# Startup dir.
if(WITH_STARTUP_DIR)
  set(STARTUP_DIR "${WITH_STARTUP_DIR}")
else()
  set(STARTUP_DIR "/etc/systemd/system")
endif()

# Script install rule.
install(
  PROGRAMS "${SCRIPT_DIR}/centagent.service"
  DESTINATION "${STARTUP_DIR}"
  COMPONENT "runtime")

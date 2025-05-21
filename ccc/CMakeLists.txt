#
# Copyright 2022 Centreon
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

#
# Global settings.
#

# Set necessary settings.
project("Centreon Collect Client" C CXX)

# set -latomic if OS is Raspbian.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
  set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")
endif()

add_definitions("-D_GLIBCXX_USE_CXX11_ABI=1")

option(WITH_LIBCXX "compiles and link cbd with clang++/libc++")

if(WITH_LIBCXX)
  set(CMAKE_CXX_COMPILER "clang++")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

  # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -Werror -O1
  # -fno-omit-frame-pointer")
endif()

include_directories(
  ${CMAKE_BINARY_DIR}/common
  ${CMAKE_SOURCE_DIR}/broker/core/src ${CMAKE_SOURCE_DIR}/engine/enginerpc
  ${CMAKE_SOURCE_DIR}/common/src)
set(ccc_files main.cc client.cc)

if(WITH_ASAN)
  set(CMAKE_BUILD_TYPE Debug)
  set(CMAKE_CXX_FLAGS_DEBUG
      "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
  set(CMAKE_LINKER_FLAGS_DEBUG
      "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()

add_executable(ccc ${ccc_files})
target_link_libraries(
  ccc PRIVATE
  #gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts
  gRPC::grpc++
  cerpc
  berpc
  centreon_common
  fmt::fmt)
set_target_properties(ccc PROPERTIES COMPILE_FLAGS "-fPIC")

install(TARGETS ccc RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

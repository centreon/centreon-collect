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

include_directories(${CMAKE_SOURCE_DIR}/broker/core/src
                    ${CMAKE_SOURCE_DIR}/engine/enginerpc)
set(ccc_files main.cc client.cc)

add_executable(ccc ${ccc_files})
target_link_libraries(ccc CONAN_PKG::grpc cerpc berpc CONAN_PKG::abseil
                      CONAN_PKG::fmt)
set_target_properties(ccc PROPERTIES COMPILE_FLAGS "-fPIC")

install(TARGETS ccc RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

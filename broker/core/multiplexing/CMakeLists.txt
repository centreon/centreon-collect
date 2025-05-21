#
# Copyright 2009-2023 Centreon
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

set(SRC_DIR "src")
set(TESTS_DIR "${PROJECT_SOURCE_DIR}/core/multiplexing/test")

# Sources.
set(SOURCES ${SRC_DIR}/engine.cc ${SRC_DIR}/muxer.cc ${SRC_DIR}/publisher.cc)

# Static libraries.
add_library(multiplexing STATIC ${SOURCES})
add_dependencies(multiplexing pb_bbdo_lib pb_extcmd_lib)

set_target_properties(multiplexing PROPERTIES COMPILE_FLAGS "-fPIC")
target_precompile_headers(multiplexing PRIVATE ../precomp_inc/precomp.hpp)
target_link_libraries(multiplexing PRIVATE spdlog::spdlog)

if(WITH_TESTING)
  set(TESTS_SOURCES
      ${TESTS_SOURCES}
      ${TESTS_DIR}/engine/start_stop.cc
      ${TESTS_DIR}/muxer/read.cc
      ${TESTS_DIR}/publisher/read.cc
      ${TESTS_DIR}/publisher/write.cc
      PARENT_SCOPE)
endif(WITH_TESTING)

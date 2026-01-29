#
# Copyright 2026 Centreon
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
set(INC_DIR "${PROJECT_SOURCE_DIR}/event_script/inc")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/event_script/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/event_script/test")
include_directories(
  ${INC_DIR} ${PROJECT_SOURCE_DIR}/neb/inc ${CMAKE_SOURCE_DIR}/common/process/inc)

add_library(
  "80-event_script" SHARED
  # Sources.
  "${SRC_DIR}/connector.cc"
  "${SRC_DIR}/factory.cc"
  "${SRC_DIR}/stream.cc"
)
target_link_libraries(
  "80-event_script" PRIVATE fmt::fmt)

target_precompile_headers("80-event_script" PRIVATE precomp_inc/precomp.hpp)
set_target_properties("80-event_script" PROPERTIES PREFIX ""
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/broker/lib")

#Testing.
if(WITH_TESTING)
  set(TESTS_SOURCES
      ${TESTS_SOURCES} ${TEST_DIR}/factory_test.cc ${TEST_DIR}/stream_test.cc
      PARENT_SCOPE)
  set(TESTS_LIBRARIES
      ${TESTS_LIBRARIES} 80-event_script
      PARENT_SCOPE)
endif(WITH_TESTING)

# Install rule.
install(TARGETS "80-event_script" LIBRARY DESTINATION "${PREFIX_MODULES}")

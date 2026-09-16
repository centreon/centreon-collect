#
# Copyright 2025-2026 Centreon
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
set(TESTS_DIR "${PROJECT_SOURCE_DIR}/core/cache/test")

include_directories(
  ${CMAKE_SOURCE_PROJECT}/bam/inc
  ${CMAKE_BINARY_DIR}
)

add_library(broker_cache STATIC
  broker_cache.cc
)

target_precompile_headers(broker_cache PRIVATE ../precomp_inc/precomp.hh)
set_target_properties(broker_cache PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/broker/lib")

add_dependencies(broker_cache process_stat pb_all_libs)

if(WITH_TESTING)
  set(TESTS_SOURCES
      ${TESTS_SOURCES}
      ${TESTS_DIR}/broker_cache.cc
      PARENT_SCOPE)
endif(WITH_TESTING)

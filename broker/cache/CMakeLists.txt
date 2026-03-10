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
set(INC_DIR "${PROJECT_SOURCE_DIR}/cache/inc/com/centreon/broker/cache")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/cache/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/cache/test")
include_directories(
  "${PROJECT_SOURCE_DIR}/cache/inc" 
  "${PROJECT_SOURCE_DIR}/neb/inc"
  "${PROJECT_SOURCE_DIR}/bam/inc")

# Cache module.
set(CACHE_MOD "15-cache")
add_library(
  "${CACHE_MOD}" SHARED
  # Sources.
  ${SRC_DIR}/main.cc
  ${SRC_DIR}/global_cache.cc
  ${SRC_DIR}/global_cache_data.cc
  ${SRC_DIR}/protobuf.cc
)

add_custom_command(
  OUTPUT ${SRC_DIR}/protobuf.cc ${INC_DIR}/protobuf.hh
  COMMENT "Generating protobuf.hh and protobuf.cc from protobuf files"
  DEPENDS ${CMAKE_SOURCE_DIR}/bbdo/neb.proto ${CMAKE_SOURCE_DIR}/bbdo/bam.proto
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMAND
    python3 ARGS ${PROJECT_SOURCE_DIR}/cache/create_class_from_proto.py neb,header,bam ${INC_DIR}/protobuf.hh ${SRC_DIR}/protobuf.cc
      Comment,AdaptiveService,Host,Service,Tag,Instance,HostGroup,ServiceGroup,DimensionBaEvent,DimensionBvEvent 
  VERBATIM)

add_custom_command(
  OUTPUT ${TEST_DIR}/protobuf_test_classes.cc ${TEST_DIR}/protobuf_test_classes.hh
  COMMENT "Generating protobuf_test_classes.hh and protobuf_test_classes.cc from protobuf files"
  DEPENDS ${CMAKE_SOURCE_DIR}/bbdo/neb.proto ${CMAKE_SOURCE_DIR}/bbdo/bam.proto
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMAND
    python3 ARGS ${PROJECT_SOURCE_DIR}/cache/create_class_from_proto.py neb,header,bam ${TEST_DIR}/protobuf_test_classes.hh ${TEST_DIR}/protobuf_test_classes.cc
  VERBATIM)


set_target_properties("${CACHE_MOD}" PROPERTIES PREFIX ""
  POSITION_INDEPENDENT_CODE ON
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/broker/lib")
target_link_libraries("${CACHE_MOD}" PRIVATE fmt::fmt)
target_precompile_headers(${CACHE_MOD} PRIVATE precomp_inc/precomp.hpp)
add_dependencies(${CACHE_MOD}  pb_common_lib process_stat)

# Testing.
if(WITH_TESTING)
  add_library(ut_cache STATIC
    ${TEST_DIR}/protobuf_test_classes.cc
  )

  target_precompile_headers(ut_cache REUSE_FROM  ${CACHE_MOD} )
  set(TESTS_SOURCES
      ${TESTS_SOURCES} 
      ${TEST_DIR}/global_cache_test.cc
      ${TEST_DIR}/protobuf_test.cc
      PARENT_SCOPE)
  set(TESTS_LIBRARIES
      ${TESTS_LIBRARIES} ut_cache ${CACHE_MOD}
      PARENT_SCOPE)
endif(WITH_TESTING)

# Install rule.
install(TARGETS "${CACHE_MOD}" LIBRARY DESTINATION "${PREFIX_MODULES}")

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

# Global options.

set(INC_DIR "${PROJECT_SOURCE_DIR}/http_tsdb/inc/com/centreon/broker/http_tsdb")
set(HTTP_INC_DIR "${CMAKE_SOURCE_DIR}/common/http/inc")
set(SRC_DIR "src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/http_tsdb/test")
include_directories(inc ${INC_DIR} ${HTTP_INC_DIR})

# Sources.
set(SOURCES
    ${SRC_DIR}/factory.cc
    ${SRC_DIR}/column.cc
    ${SRC_DIR}/line_protocol_query.cc
    ${SRC_DIR}/stream.cc
)

# Headers.
set(HEADERS
    ${INC_DIR}/factory.hh
    ${INC_DIR}/stream.hh
    ${INC_DIR}/column.hh
    ${INC_DIR}/line_protocol_query.hh
)

add_library(http_tsdb STATIC ${SOURCES} ${HEADERS})
add_dependencies(http_tsdb
	pb_neb_lib
	pb_header_lib
	pb_storage_lib
	)
target_include_directories(http_tsdb PRIVATE ${INC_DIR})

target_precompile_headers(http_tsdb PRIVATE precomp_inc/precomp.hh)

set_target_properties(http_tsdb PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Testing.
#if(WITH_TESTING)
#    set(TESTS_SOURCES
#        ${TESTS_SOURCES}
#        ${TEST_DIR}/factory_test.cc
#        ${TEST_DIR}/stream_test.cc
#        PARENT_SCOPE)
#endif(WITH_TESTING)

#
# Copyright 2011-2025 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#

# Global options.
set(INC_DIR "${PROJECT_SOURCE_DIR}/tcp/inc/com/centreon/broker/tcp")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/tcp/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/tcp/test")
include_directories(${PROJECT_SOURCE_DIR}/tcp/inc
                    ${PROJECT_SOURCE_DIR}/neb/inc
		    ${CMAKE_BINARY_DIR}/common/process_stat
)

# TCP module.
set(TCP "50-tcp")
set(TCP "${TCP}" PARENT_SCOPE)
add_library("${TCP}" SHARED
  "${SRC_DIR}/acceptor.cc"
  "${SRC_DIR}/connector.cc"
  "${SRC_DIR}/factory.cc"
  "${SRC_DIR}/main.cc"
  "${SRC_DIR}/stream.cc"
  "${SRC_DIR}/tcp_async.cc"
  "${SRC_DIR}/tcp_connection.cc"
)
set_target_properties("${TCP}" PROPERTIES PREFIX ""
  POSITION_INDEPENDENT_CODE ON
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/broker/lib"
)
add_dependencies("${TCP}" process_stat)
target_link_libraries("${TCP}" spdlog::spdlog)
target_precompile_headers(${TCP} PRIVATE precomp_inc/precomp.hpp)

# Testing.
if (WITH_TESTING)
  set(
    TESTS_SOURCES
    ${TESTS_SOURCES}
    ${TEST_DIR}/acceptor.cc
    ${TEST_DIR}/connector.cc
    ${TEST_DIR}/factory.cc
    PARENT_SCOPE
  )
  set(
    TESTS_LIBRARIES
    ${TESTS_LIBRARIES}
    ${TCP}
    PARENT_SCOPE
  )
endif(WITH_TESTING)

# Install rule.
install(TARGETS "${TCP}"
  LIBRARY DESTINATION "${PREFIX_MODULES}"
)

#
# Copyright 2023 Centreon
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

set(TESTS_DIR "${PROJECT_SOURCE_DIR}/core/misc/test")

# Sources.
set(SOURCES
    diagnostic.cc
    filesystem.cc
    misc.cc
    parse_perfdata.cc
    perfdata.cc
    processing_speed_computer.cc
    string.cc
    time.cc
    uuid.cc
    variant.cc
)

# Static libraries.
add_library(misc STATIC ${SOURCES})
set_target_properties(misc PROPERTIES COMPILE_FLAGS "-fPIC")

if(WITH_TESTING)
  set(TESTS_SOURCES
    ${TESTS_SOURCES}
    ${TESTS_DIR}/buffer.cc
    ${TESTS_DIR}/exec.cc
    ${TESTS_DIR}/filesystem.cc
    ${TESTS_DIR}/math.cc
    ${TESTS_DIR}/misc.cc
    ${TESTS_DIR}/perfdata.cc
    ${TESTS_DIR}/string.cc
    PARENT_SCOPE
  )
endif(WITH_TESTING)


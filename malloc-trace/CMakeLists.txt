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
project("Centreon malloc trace" C CXX)

# Set directories.
set(INC_DIR "${PROJECT_SOURCE_DIR}/inc/com/centreon/malloc_trace")
set(SRC_DIR "src")

add_library(centreon-malloc-trace SHARED
    "src/by_thread_trace_active.cc"
    "src/malloc_trace.cc"
    "src/orphan_container.cc"
    "src/simply_allocator.cc"
)

target_link_libraries(centreon-malloc-trace
    fmt::fmt
)

target_include_directories(centreon-malloc-trace PRIVATE 
    ${INC_DIR} 
    ${CMAKE_SOURCE_DIR}/common/inc
)

target_precompile_headers(centreon-malloc-trace PRIVATE "precomp_inc/precomp.hh")

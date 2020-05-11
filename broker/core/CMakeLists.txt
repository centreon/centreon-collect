##
## Copyright 2009-2020 Centreon
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## For more information : contact@centreon.com
##

include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)








add_library(roker STATIC
)
target_link_libraries(roker rokerbase)




# Centreon Broker Watchdog
option(WITH_CBWD "Build centreon broker watchdong." ON)
if (WITH_CBWD)
  add_subdirectory(watchdog)
endif ()

#
# Unit tests.
#
# Enable testing.
option(WITH_TESTING "Generate unit tests." OFF)
if (WITH_TESTING)
  add_subdirectory(test)
endif ()

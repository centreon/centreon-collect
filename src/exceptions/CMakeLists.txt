##
## Copyright 2011-2014 Centreon
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

# Set directories.
set(SRC_DIR "${SRC_DIR}/exceptions")
set(INC_DIR "${INC_DIR}/exceptions")

# Set sources.
set(
  SOURCES
  ${SOURCES}
  "${SRC_DIR}/basic.cc"
  "${SRC_DIR}/interruption.cc"
  PARENT_SCOPE
)

# Set headers.
set(
  HEADERS
  ${HEADERS}
  "${INC_DIR}/basic.hh"
  "${INC_DIR}/interruption.hh"
  PARENT_SCOPE
)

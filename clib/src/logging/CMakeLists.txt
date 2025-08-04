#
# Copyright 2011-2013 Centreon
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

# Set directories.
set(SRC_DIR "${SRC_DIR}/logging")
set(INC_DIR "${INC_DIR}/logging")

# Platform-specific files.
set(SPECIFIC_SOURCES
  "${SRC_DIR}/syslogger.cc"
)
set(SPECIFIC_HEADERS
  "${INC_DIR}/syslogger.hh"
)

# Set sources.
set(
  SOURCES
  ${SOURCES}
  ${SPECIFIC_SOURCES}
  "${SRC_DIR}/backend.cc"
  "${SRC_DIR}/engine.cc"
  "${SRC_DIR}/file.cc"
  "${SRC_DIR}/temp_logger.cc"
  PARENT_SCOPE
)

# Set headers.
set(
  HEADERS
  ${HEADERS}
  ${SPECIFIC_HEADERS}
  "${INC_DIR}/backend.hh"
  "${INC_DIR}/engine.hh"
  "${INC_DIR}/file.hh"
  "${INC_DIR}/logger.hh"
  "${INC_DIR}/temp_logger.hh"
  PARENT_SCOPE
)

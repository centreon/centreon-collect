##
## Copyright 2011 Merethis
##
## This file is part of Centreon Clib.
##
## Centreon Clib is free software: you can redistribute it
## and/or modify it under the terms of the GNU Affero General Public
## License as published by the Free Software Foundation, either version
## 3 of the License, or (at your option) any later version.
##
## Centreon Clib is distributed in the hope that it will be
## useful, but WITHOUT ANY WARRANTY; without even the implied warranty
## of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
## Affero General Public License for more details.
##
## You should have received a copy of the GNU Affero General Public
## License along with Centreon Clib. If not, see
## <http://www.gnu.org/licenses/>.
##

# Set directories.
set(SRC_DIR "${SRC_DIR}/logging")
set(INC_DIR "${INC_DIR}/logging")

# Set sources.
set(
  SOURCES
  ${SOURCES}
  "${SRC_DIR}/backend.cc"
  "${SRC_DIR}/engine.cc"
  "${SRC_DIR}/file.cc"
  "${SRC_DIR}/logger.cc"
  "${SRC_DIR}/syslogger.cc"
  "${SRC_DIR}/temp_logger.cc"
  "${SRC_DIR}/verbosity.cc"
  PARENT_SCOPE
)

# Set headers.
set(
  HEADERS
  ${HEADERS}
  "${INC_DIR}/backend.hh"
  "${INC_DIR}/engine.hh"
  "${INC_DIR}/file.hh"
  "${INC_DIR}/logger.hh"
  "${INC_DIR}/syslogger.hh"
  "${INC_DIR}/temp_logger.hh"
  "${INC_DIR}/verbosity.hh"
  PARENT_SCOPE
)

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
set(INC_DIR "${INC_DIR}/clib")
set(SRC_DIR "${SRC_DIR}/clib")

# Generate version header.
configure_file(
  "${INC_DIR}/version.hh.in"
  "${INC_DIR}/version.hh")

# Set sources.
set(
  SOURCES
  ${SOURCES}
  "${SRC_DIR}/version.cc"
  PARENT_SCOPE
)

# Set headers.
set(
  HEADERS
  ${HEADERS}
  "${INC_DIR}/version.hh"
  PARENT_SCOPE
)

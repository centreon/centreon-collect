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
set(SRC_DIR "${SRC_DIR}/concurrency")
set(INC_DIR "${INC_DIR}/concurrency")

# Set sources.
set(
  SOURCES
  ${SOURCES}
  "${SRC_DIR}/locker.cc"
  "${SRC_DIR}/mutex.cc"
  "${SRC_DIR}/runnable.cc"
  "${SRC_DIR}/semaphore.cc"
  "${SRC_DIR}/thread.cc"
  "${SRC_DIR}/thread_pool.cc"
  "${SRC_DIR}/wait_condition.cc"
  PARENT_SCOPE
)

# Set headers.
set(
  HEADERS
  ${HEADERS}
  "${INC_DIR}/locker.hh"
  "${INC_DIR}/mutex.hh"
  "${INC_DIR}/runnable.hh"
  "${INC_DIR}/semaphore.hh"
  "${INC_DIR}/thread.hh"
  "${INC_DIR}/thread_pool.hh"
  "${INC_DIR}/wait_condition.hh"
)

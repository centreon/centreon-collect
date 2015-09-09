##
## Copyright 2011-2013 Centreon
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
set(SRC_DIR "${SRC_DIR}/concurrency")
set(INC_DIR "${INC_DIR}/concurrency")

# Platform-specific files.
if (WIN32)
  set(SPECIFIC_SOURCES
    "${SRC_DIR}/condvar_win32.cc"
    "${SRC_DIR}/mutex_win32.cc"
    "${SRC_DIR}/read_write_lock_win32.cc"
    "${SRC_DIR}/semaphore_win32.cc"
    "${SRC_DIR}/thread_win32.cc"
  )
  set(SPECIFIC_HEADERS
    "${INC_DIR}/condvar_win32.hh"
    "${INC_DIR}/mutex_win32.hh"
    "${INC_DIR}/read_write_lock_win32.hh"
    "${INC_DIR}/semaphore_win32.hh"
    "${INC_DIR}/thread_win32.hh"
  )
else ()
  set(SPECIFIC_SOURCES
    "${SRC_DIR}/condvar_posix.cc"
    "${SRC_DIR}/read_write_lock_posix.cc"
    "${SRC_DIR}/semaphore_posix.cc"
    "${SRC_DIR}/thread_posix.cc"
  )
  set(SPECIFIC_HEADERS
    "${INC_DIR}/condvar_posix.hh"
    "${INC_DIR}/mutex_posix.hh"
    "${INC_DIR}/read_write_lock_posix.hh"
    "${INC_DIR}/semaphore_posix.hh"
    "${INC_DIR}/thread_posix.hh"
  )
endif ()

# Set sources.
set(
  SOURCES
  ${SOURCES}
  ${SPECIFIC_SOURCES}
  "${SRC_DIR}/runnable.cc"
  "${SRC_DIR}/thread_pool.cc"
  PARENT_SCOPE
)

# Set headers.
set(
  HEADERS
  ${HEADERS}
  ${SPECIFIC_HEADERS}
  "${INC_DIR}/condvar.hh"
  "${INC_DIR}/locker.hh"
  "${INC_DIR}/mutex.hh"
  "${INC_DIR}/read_locker.hh"
  "${INC_DIR}/read_write_lock.hh"
  "${INC_DIR}/runnable.hh"
  "${INC_DIR}/semaphore.hh"
  "${INC_DIR}/thread.hh"
  "${INC_DIR}/thread_pool.hh"
  "${INC_DIR}/write_locker.hh"
  PARENT_SCOPE
)

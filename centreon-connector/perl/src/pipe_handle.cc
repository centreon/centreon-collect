/*
** Copyright 2012-2014 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include <cerrno>
#include <cstring>
#include <set>
#include <mutex>
#include <unistd.h>
#include "com/centreon/connector/perl/pipe_handle.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::connector::perl;

/**************************************
*                                     *
*           Local Objects             *
*                                     *
**************************************/

static std::multiset<int> gl_fds;
static std::mutex gl_fdsm;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] fd File descriptor.
 */
pipe_handle::pipe_handle(int fd) : _fd(-1) {
  set_fd(fd);
}

/**
 *  Copy constructor.
 *
 *  @param[in] ph Object to copy.
 */
pipe_handle::pipe_handle(pipe_handle const& ph) : handle(ph) {
  _internal_copy(ph);
}

/**
 *  Destructor.
 */
pipe_handle::~pipe_handle() throw () {
  close();
}

/**
 *  Assignment operator.
 *
 *  @param[in] ph Object to copy.
 *
 *  @return This object.
 */
pipe_handle& pipe_handle::operator=(pipe_handle const& ph) {
  if (this != &ph) {
    handle::operator=(ph);
    close();
    _internal_copy(ph);
  }
  return *this;
}

/**
 *  Close the file descriptor.
 */
void pipe_handle::close() throw () {
  if (_fd >= 0) {
    {
      std::lock_guard<std::mutex> lock(gl_fdsm);
      auto it = gl_fds.find(_fd);
      if (it != gl_fds.end())
        gl_fds.erase(it);
    }
    if (::close(_fd) != 0) {
      char const* msg(strerror(errno));
      log_error(logging::medium) << "could not close pipe FD: " << msg;
    }
    _fd = -1;
  }
}

/**
 *  Close all handles.
 */
void pipe_handle::close_all_handles() {
  std::lock_guard<std::mutex> lock(gl_fdsm);
  for (std::multiset<int>::const_iterator
         it(gl_fds.begin()),
         end(gl_fds.end());
       it != end;
       ++it) {
    int retval;
    do {
      retval = ::close(*it);
    } while ((retval != 0) && (EINTR == errno));
    if (retval != 0) {
      char const* msg(strerror(errno));
      gl_fds.erase(gl_fds.begin(), it);
      throw (basic_error() << msg);
    }
  }
  gl_fds.clear();
}

/**
 *  Get the native handle associated with this pipe handle.
 *
 *  @return Pipe FD.
 */
int pipe_handle::get_native_handle() throw () {
  return _fd;
}

/**
 *  Initialize static members of the pipe_handle class.
 */
void pipe_handle::load() {}

/**
 *  Read data from the file descriptor.
 *
 *  @param[out] data Destination buffer.
 *  @param[in]  size Maximum number of bytes to read.
 *
 *  @return Number of bytes actually read.
 */
unsigned long pipe_handle::read(void* data, unsigned long size) {
  ssize_t rb(::read(_fd, data, size));
  if (rb < 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "could not read from pipe: " << msg);
  }
  return rb;
}

/**
 *  Set file descriptor.
 *
 *  @param[in] fd New class file descriptor.
 */
void pipe_handle::set_fd(int fd) {
  close();
  _fd = fd;
  if (_fd >= 0) {
    std::lock_guard<std::mutex> lock(gl_fdsm);
    gl_fds.insert(fd);
  }
}

/**
 *  Cleanup static ressources used by the pipe_handle class.
 */
void pipe_handle::unload() {}

/**
 *  Write data to the pipe.
 *
 *  @param[in] data Source buffer.
 *  @param[in] size Maximum number of bytes to write.
 *
 *  @return Actual number of bytes written.
 */
unsigned long pipe_handle::write(void const* data, unsigned long size) {
  ssize_t wb(::write(_fd, data, size));
  if (wb <= 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "could not write to pipe: " << msg);
  }
  return wb;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy internal data members.
 *
 *  @param[in] ph Object to copy.
 */
void pipe_handle::_internal_copy(pipe_handle const& ph) {
  if (ph._fd >= 0) {
    _fd = dup(ph._fd);
    if (_fd < 0) {
      char const* msg(strerror(errno));
      throw (basic_error() << "could not duplicate pipe: " << msg);
    }
    {
    std::lock_guard<std::mutex> lock(gl_fdsm);
      gl_fds.insert(_fd);
    }
  }
  else
    _fd = -1;
}

/*
** Copyright 2012 Merethis
**
** This file is part of Centreon Connector Perl.
**
** Centreon Connector Perl is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector Perl is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector Perl. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "com/centreon/connector/perl/pipe_handle.hh"
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon;
using namespace com::centreon::connector::perl;

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
pipe_handle::pipe_handle(int fd) {
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
  return (*this);
}

/**
 *  Close the file descriptor.
 */
void pipe_handle::close() throw () {
  if (_fd >= 0) {
    ::close(_fd);
    _fd = -1;
  }
  return ;
}

/**
 *  Read data from the file descriptor.
 *
 *  @param[out] data Destination buffer.
 *  @param[in]  size Maximum number of bytes to read.
 *
 *  @return Number of bytes actually read.
 */
unsigned int pipe_handle::read(void* data, unsigned int size) {
  ssize_t rb(::read(_fd, data, size));
  if (rb < 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "could not read from pipe: " << msg);
  }
  return (rb);
}

/**
 *  Set file descriptor.
 *
 *  @param[in] fd New class file descriptor.
 */
void pipe_handle::set_fd(int fd) {
  close();
  _fd = fd;
  return ;
}

/**
 *  Write data to the pipe.
 *
 *  @param[in] data Source buffer.
 *  @param[in] size Maximum number of bytes to write.
 *
 *  @return Actual number of bytes written.
 */
unsigned int pipe_handle::write(void const* data, unsigned int size) {
  ssize_t wb(::write(_fd, data, size));
  if (wb <= 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "could not write to pipe: " << msg);
  }
  return (wb);
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
  }
  else
    _fd = -1;
  return ;
}

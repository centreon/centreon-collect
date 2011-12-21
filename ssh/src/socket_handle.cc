/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "com/centreon/connector/ssh/socket_handle.hh"
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon;
using namespace com::centreon::connector::ssh;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 *
 *  @param[in] internal_handle Native socket descriptor.
 */
socket_handle::socket_handle(native_handle internal_handle)
  : handle(internal_handle) {}

/**
 *  Destructor.
 */
socket_handle::~socket_handle() throw () {
  this->close();
}

/**
 *  Close socket descriptor.
 */
void socket_handle::close() {
  shutdown(_internal_handle, SHUT_RDWR);
  ::close(_internal_handle);
  _internal_handle = -1;
  return ;
}

/**
 *  Read from socket descriptor.
 *
 *  @param[out] data Where data will be stored.
 *  @param[in]  size How much data in bytes to read at most.
 *
 *  @return Number of bytes actually read.
 */
unsigned long socket_handle::read(void* data, unsigned long size) {
  ssize_t rb(::read(_internal_handle, data, size));
  if (rb < 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "socket read error: " << msg);
  }
  return (rb);
}

/**
 *  Set socket descriptor.
 *
 *  @param[in] internal_handle Native socket descriptor.
 */
void socket_handle::set_native_handle(native_handle internal_handle) {
  this->close();
  _internal_handle = internal_handle;
  return ;
}

/**
 *  Write to socket descriptor.
 *
 *  @param[in] data Data to write.
 *  @param[in] size How much data to write at most.
 *
 *  @return Number of bytes actually written.
 */
unsigned long socket_handle::write(
                               void const* data,
                               unsigned long size) {
  ssize_t wb(::write(_internal_handle, data, size));
  if (wb < 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "socket write error: " << msg);
  }
  return (wb);
}

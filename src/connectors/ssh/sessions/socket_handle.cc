/*
** Copyright 2011-2013 Centreon
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
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "com/centreon/connector/ssh/sessions/socket_handle.hh"
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon;
using namespace com::centreon::exceptions;
using namespace com::centreon::connector::ssh::sessions;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Default constructor.
 *
 *  @param[in] handl Native socket descriptor.
 */
socket_handle::socket_handle(native_handle handl) : _handl(handl) {}

/**
 *  Destructor.
 */
socket_handle::~socket_handle() noexcept { this->close(); }

/**
 *  Close socket descriptor.
 */
void socket_handle::close() {
  if (_handl != native_handle_null) {
    shutdown(_handl, SHUT_RDWR);
    ::close(_handl);
    _handl = native_handle_null;
  }
}

/**
 *  Get the native socket handle.
 *
 *  @return Native socket handle.
 */
native_handle socket_handle::get_native_handle() { return _handl; }

/**
 *  Read from socket descriptor.
 *
 *  @param[out] data Where data will be stored.
 *  @param[in]  size How much data in bytes to read at most.
 *
 *  @return Number of bytes actually read.
 */
unsigned long socket_handle::read(void* data, unsigned long size) {
  ssize_t rb(::read(_handl, data, size));
  if (rb < 0) {
    char const* msg(strerror(errno));
    throw basic_error("socket read error: {}", msg);
  }
  return rb;
}

/**
 *  Set socket descriptor.
 *
 *  @param[in] handl Native socket descriptor.
 */
void socket_handle::set_native_handle(native_handle handl) {
  this->close();
  _handl = handl;
}

/**
 *  Write to socket descriptor.
 *
 *  @param[in] data Data to write.
 *  @param[in] size How much data to write at most.
 *
 *  @return Number of bytes actually written.
 */
unsigned long socket_handle::write(void const* data, unsigned long size) {
  ssize_t wb(::write(_handl, data, size));
  if (wb < 0) {
    char const* msg(strerror(errno));
    throw basic_error("socket write error: {}", msg);
  }
  return wb;
}

/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/connector/icmp/interrupt.hh"

using namespace com::centreon;
using namespace com::centreon::connector::icmp;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
interrupt::interrupt() {
  if (pipe(_fd) == -1) {
    char const* msg(strerror(errno));
    throw (basic_error() << "interrupt constructor failed: " << msg);
  }
}

/**
 *  Copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
interrupt::interrupt(interrupt const& right)
  : handle_listener(right),
    handle(right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
interrupt::~interrupt() throw () {
  close();
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
interrupt& interrupt::operator=(interrupt const& right) {
  _internal_copy(right);
  return (*this);
}

/**
 *  Wake the current handle manager.
 */
void interrupt::wake() {
  write("\0", 1);
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Close pipe.
 */
void interrupt::close() {
  ::close(_fd[0]);
  ::close(_fd[1]);
}

/**
 *  Forward close to handle_listener.
 *
 *  @param[in,out] h  The handle to close;
 */
void interrupt::close(handle& h) {
  (void)h;
  return ;
}

/**
 *  Catch error.
 */
void interrupt::error(handle& h) {
  (void)h;
  close();
}

/**
 *  Return the handle associated to this class.
 *
 *  @return Handle associated with this class.
 */
native_handle interrupt::get_native_handle() {
  return (_fd[0]);
}

/**
 *  Read pipe data.
 *
 *  @param[out] data  The buffer to fill.
 *  @param[in]  size  The buffer size.
 *
 *  @return The total bytes was read.
 */
unsigned long interrupt::read(void* data, unsigned long size) {
  if (!data)
    throw (basic_error() << "read failed on interrupt: " \
           "invalid parameter (null pointer)");
  int ret;
  do {
    ret = ::read(_fd[0], data, size);
  } while (ret == -1 && errno == EINTR);
  if (ret < 0)
    throw (basic_error() << "read failed on interrupt: "
           << strerror(errno));
  return (static_cast<unsigned long>(ret));
}

/**
 *  Forward read to handle_listener.
 *
 *  @param[in,out] h  The handle to read.
 */
void interrupt::read(handle& h) {
  handle_listener::read(h);
}

/**
 *  Need to read input file descriptor.
 *
 *  @param[in] h  The input file descriptor.
 *
 *  @return Always true.
 */
bool interrupt::want_read(handle& h) {
  (void)h;
  return (true);
}

/**
 *  Write into the pipe.
 *
 *  @param[in] data  Data to write into the pipe.
 *  @param[in] size  Data size.
 *
 *  @return The total bytes written.
 */
unsigned long interrupt::write(void const* data, unsigned long size) {
  if (!data)
    throw (basic_error() << "write failed on interrupt: " \
           "invalid parameter (null pointer)");
  int ret;
  do {
    ret = ::write(_fd[1], data, size);
  } while (ret == -1 && errno == EINTR);
  if (ret < 0)
    throw (basic_error() << "write failed on interrupt: "
           << strerror(errno));
  return (static_cast<unsigned long>(ret));
}

/**
 *  Forward write to handle_listener.
 *
 *  @param[in,out] h  The handle to write.
 */
void interrupt::write(handle& h) {
  handle_listener::write(h);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 */
void interrupt::_internal_copy(interrupt const& right) {
  if (this != &right) {
    handle_listener::operator=(right);
    handle::operator=(right);
    close();
    for (unsigned int i(0); i < 2; ++i) {
      if ((_fd[i] = dup(right._fd[i])) == -1) {
        char const* msg(strerror(errno));
        throw (basic_error() << "interrupt copy failed: " << msg);
      }
    }
  }
}

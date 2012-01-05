/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Clib.
**
** Centreon Clib is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Clib is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Clib. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/connector/icmp/interrupt.hh"

using namespace com::centreon;
using namespace com::centreon::connector::icmp;

/**
 *  Default constructor.
 */
interrupt::interrupt()
  : handle() {
  if (pipe(_fd) == -1)
    throw (basic_error() << "interrupt constructor failed:"
           << strerror(errno));
  _internal_handle = _fd[0];
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
interrupt::interrupt(interrupt const& right)
  : handle(right) {
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
  return (_internal_copy(right));
}

/**
 *  Wake the current handle manager.
 */
void interrupt::wake() {
  write("\0", 1);
}

/**
 *  Close pipe.
 */
void interrupt::close() {
  ::close(_fd[0]);
  ::close(_fd[1]);
}

/**
 *  Catch error.
 */
void interrupt::error(handle& h) {
  (void)h;
  close();
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
    throw (basic_error() << "read failed on interrupt:" \
           "invalid parameter (null pointer)");
  int ret;
  do {
    ret = ::read(_internal_handle, data, size);
  } while (ret == -1 && errno == EINTR);
  if (ret < 0)
    throw (basic_error() << "read failed on interrupt:"
           << strerror(errno));
  return (static_cast<unsigned long>(ret));
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
    throw (basic_error() << "write failed on interrupt:" \
           "invalid parameter (null pointer)");
  int ret;
  do {
    ret = ::write(_fd[1], data, size);
  } while (ret == -1 && errno == EINTR);
  if (ret < 0)
    throw (basic_error() << "write failed on interrupt:"
           << strerror(errno));
  return (static_cast<unsigned long>(ret));
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
interrupt& interrupt::_internal_copy(interrupt const& right) {
  if (this != &right) {
    handle::operator=(right);
    close();
    for (unsigned int i(0); i < 2; ++i) {
      if ((_fd[0] = dup(right._fd[0])) == -1)
        throw (basic_error() << "interrupt copy failed:"
               << strerror(errno));
    }
    _internal_handle = _fd[0];
  }
  return (*this);
}

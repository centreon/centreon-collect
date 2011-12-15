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
#include <string.h>
#include <unistd.h>
#include "com/centreon/exception/basic.hh"
#include "com/centreon/io/standard_error.hh"

using namespace com::centreon::io;

/**
 *  Default constructor.
 */
standard_error::standard_error()
  : handle(2) {

}

/**
 *  Default copy constructor.
 */
standard_error::standard_error(standard_error const& right)
  : handle(right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
standard_error::~standard_error() throw () {

}

/**
 *  Default copy operator.
 */
standard_error& standard_error::operator=(standard_error const& right){
  return (_internal_copy(right));
}


/**
 *  Close the standard error.
 */
void standard_error::close() {
  int ret;
  do {
    ret = ::close(_internal_handle);
  } while (ret == -1 && errno == EINTR);
  _internal_handle = -1;
}

/**
 *  Impossible to read on the standard error.
 *
 *  @param[in] data  none
 *  @param[in] size  none
 *
 *  @return Throw basic exception.
 */
unsigned long standard_error::read(void* data, unsigned long size) {
  (void)data;
  (void)size;
  throw (basic_error() << "read failed on standard error:" \
         "not implemented");
  return (0);
}

/**
 *  Write on the standard error.
 *
 *  @param[in] data  Buffer to write on standard error.
 *  @param[in] size  Size of the buffer.
 *
 *  @return The number of bytes written on the standard error.
 */
unsigned long standard_error::write(
                                 void const* data,
                                 unsigned long size) {
  if (!data)
    throw (basic_error() << "write failed on standard error:" \
           "invalid parameter (null pointer)");
  int ret;
  do {
    ret = ::write(_internal_handle, data, size);
  } while (ret == -1 && errno == EINTR);
  if (ret < 0)
    throw (basic_error() << "write failed on standard error:"
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
standard_error& standard_error::_internal_copy(standard_error const& right) {
  handle::_internal_copy(right);
  return (*this);
}

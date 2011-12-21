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
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/standard_input.hh"

using namespace com::centreon::io;

/**
 *  Default constructor.
 */
standard_input::standard_input()
  : handle(0) {

}

/**
 *  Default copy constructor.
 */
standard_input::standard_input(standard_input const& right)
  : handle(right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
standard_input::~standard_input() throw () {

}

/**
 *  Default copy operator.
 */
standard_input& standard_input::operator=(standard_input const& right){
  return (_internal_copy(right));
}

/**
 *  Close the standard input.
 */
void standard_input::close() {
  int ret;
  do {
    ret = ::close(_internal_handle);
  } while (ret == -1 && errno == EINTR);
  _internal_handle = -1;
}

/**
 *  Read on the standard input.
 *
 *  @param[out] data  Buffer to fill.
 *  @param[in]  size  Buffer size.
 *
 *  @return The number of bytes was read on the standard input.
 */
unsigned long standard_input::read(void* data, unsigned long size) {
  if (!data)
    throw (basic_error() << "read failed on standard input:" \
           "invalid parameter (null pointer)");
  int ret;
  do {
    ret = ::read(_internal_handle, data, size);
  } while (ret == -1 && errno == EINTR);
  if (ret < 0)
    throw (basic_error() << "read failed on standard input:"
           << strerror(errno));
  return (static_cast<unsigned long>(ret));
}

/**
 *  Impossible to Write on the standard input.
 *
 *  @param[in] data  None.
 *  @param[in] size  None.
 *
 *  @return Throw basic exception.
 */
unsigned long standard_input::write(
                                 void const* data,
                                 unsigned long size) {
  (void)data;
  (void)size;
  throw (basic_error() << "write failed standard input:" \
         "not implemented");
  return (0);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
standard_input& standard_input::_internal_copy(standard_input const& right) {
  handle::_internal_copy(right);
  return (*this);
}

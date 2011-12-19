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
#include "com/centreon/io/standard_output.hh"

using namespace com::centreon::io;

/**
 *  Default constructor.
 */
standard_output::standard_output()
  : handle(1) {

}

/**
 *  Default copy constructor.
 */
standard_output::standard_output(standard_output const& right)
  : handle(right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
standard_output::~standard_output() throw () {

}

/**
 *  Default copy operator.
 */
standard_output& standard_output::operator=(standard_output const& right){
  return (_internal_copy(right));
}


/**
 *  Close the standard output.
 */
void standard_output::close() {
  int ret;
  do {
    ret = ::close(_internal_handle);
  } while (ret == -1 && errno == EINTR);
  _internal_handle = -1;
}

/**
 *  Impossible to read on the standard output.
 *
 *  @param[in] data  none
 *  @param[in] size  none
 *
 *  @return Throw basic exception.
 */
unsigned long standard_output::read(void* data, unsigned long size) {
  (void)data;
  (void)size;
  throw (basic_error() << "read failed on standard output:" \
         "not implemented");
  return (0);
}

/**
 *  Write on the standard output.
 *
 *  @param[in] data  Buffer to write on standard output.
 *  @param[in] size  Size of the buffer.
 *
 *  @return The number of bytes written on the standard output.
 */
unsigned long standard_output::write(
                                 void const* data,
                                 unsigned long size) {
  if (!data)
    throw (basic_error() << "write failed on standard output:" \
           "invalid parameter (null pointer)");
  int ret;
  do {
    ret = ::write(_internal_handle, data, size);
  } while (ret == -1 && errno == EINTR);
  if (ret < 0)
    throw (basic_error() << "write failed on standard output:"
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
standard_output& standard_output::_internal_copy(standard_output const& right) {
  handle::_internal_copy(right);
  return (*this);
}

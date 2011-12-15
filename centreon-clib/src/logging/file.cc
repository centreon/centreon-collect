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

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/exception/basic.hh"
#include "com/centreon/logging/file.hh"

using namespace com::centreon::concurrency;
using namespace com::centreon::logging;

/**
 *  Default constructor.
 *
 *  @param[in] file  The file to used.
 */
file::file(FILE* file)
  : _out(file) {

}

/**
 *  Constructor with file path name.
 *
 *  @param[in] path  The path of the file to used.
 */
file::file(std::string const& path)
  : _out(NULL) {
  if (!(_out = fopen(path.c_str(), "a")))
    throw (basic_error() << "failed to open file \"" << path << "\":"
           << strerror(errno));
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
file::file(file const& right)
  : backend(right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
file::~file() throw () {
  locker lock(&_mtx);
  if (_out) {
    int ret;
    do {
      ret = fclose(_out);
    } while (ret == -1 && errno == EINTR);
  }
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
file& file::operator=(file const& right) {
  return (_internal_copy(right));
}

/**
 *  Forces write buffered data.
 */
void file::flush() throw () {
  locker lock(&_mtx);
  if (_out) {
    int ret;
    do {
      ret = fflush(_out);
    } while (ret == -1 && errno == EINTR);
  }
}

/**
 *  Write message into the file.
 *
 *  @param[in] msg   The message to write.
 *  @param[in] size  The message's size.
 */
void file::log(char const* msg, unsigned int size) throw () {
  locker lock(&_mtx);
  if (_out) {
    size_t ret;
    do {
      clearerr(_out);
      ret = fwrite(msg, size, 1, _out);
    } while (ret != 1 && ferror(_out) && errno == EINTR);
  }
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
file& file::_internal_copy(file const& right) {
  (void)right;
  assert(!"impossible to copy logging::file");
  abort();
  return (*this);
}

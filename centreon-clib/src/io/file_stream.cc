/*
** Copyright 2012 Merethis
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
#ifdef _WIN32
#  include <io.h>
#  include <windows.h>
#else
#  include <unistd.h>
#endif // Windows or POSIX.
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_stream.hh"

using namespace com::centreon::io;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] stream     Associated stream.
 *  @param[in] auto_close Should the class automatically close the
 *                        stream ?
 */
file_stream::file_stream(FILE* stream, bool auto_close)
  : _auto_close(auto_close), _stream(stream) {}

/**
 *  Destructor.
 */
file_stream::~file_stream() throw () {
  close();
}

/**
 *  Close file stream.
 */
void file_stream::close() {
  if (_stream) {
    if (_auto_close)
      fclose(_stream);
    _stream = NULL;
  }
  return ;
}

/**
 *  Get native handle.
 *
 *  @return Native handle.
 */
com::centreon::native_handle file_stream::get_native_handle() {
  native_handle retval;
  if (!_stream)
    retval = native_handle_null;
#ifdef _WIN32
  else {
    retval = (HANDLE)_get_osfhandle(_fileno(_stream));
    if ((native_handle_null == retval)
        || (INVALID_HANDLE_VALUE == retval)) {
      char const* msg(strerror(errno));
      throw (basic_error() << "could not get native handle from "
             "file stream: " << msg);
    }
  }
#else
  else {
    retval = fileno(_stream);
    if (retval < 0) {
      char const* msg(strerror(errno));
      throw (basic_error() << "could not get native handle from " \
             "file stream: " << msg);
    }
  }
#endif // Windows or POSIX.
  return (retval);
}

/**
 *  Open file.
 */
void file_stream::open(char const* path, char const* mode) {
  close();
  _auto_close = true;
  _stream = fopen(path, mode);
  if (!_stream) {
    char const* msg(strerror(errno));
    throw (basic_error() << "could not open file '"
           << path << "': " << msg);
  }
  return ;
}

/**
 *  Read data from stream.
 *
 *  @param[out] data Destination buffer.
 *  @param[in]  size Maximum number of bytes to read.
 *
 *  @return Number of bytes actually read.
 */
unsigned long file_stream::read(void* data, unsigned long size) {
  if (!_stream)
    throw (basic_error() << "attempt to read from closed file stream");
  if (!data || !size)
    throw (basic_error() << "attempt to read from " \
           "file stream but do not except any result");
#ifdef _WIN32
  DWORD rb;
  bool success(ReadFile(
                 get_native_handle(),
                 data,
                 size,
                 &rb,
                 NULL) != FALSE);
  if (!success) {
    int errcode(GetLastError());
    throw (basic_error() << "could not read from file stream (error "
           << errcode << ")");
  }
  return (static_cast<unsigned long>(rb));
#else
  ssize_t rb(::read(get_native_handle(), data, size));
  if (rb < 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "could not read from file stream: " << msg);
  }
  return (static_cast<unsigned long>(rb));
#endif // Windows or POSIX.
}

/**
 *  Write data to stream.
 *
 *  @param[in] data Buffer.
 *  @param[in] size Maximum number of bytes to write.
 *
 *  @return Number of bytes actually written.
 */
unsigned long file_stream::write(void const* data, unsigned long size) {
  if (!_stream)
    throw (basic_error() << "attempt to write to a closed file stream");
  if (!data || !size)
    throw (basic_error() << "attempt to write no data to file stream");
#ifdef _WIN32
  DWORD wb;
  bool success(WriteFile(
                 get_native_handle(),
                 data,
                 size,
                 &wb,
                 NULL) != FALSE);
  if (!success) {
    int errcode(GetLastError());
    throw (basic_error() << "could not write to file stream (error "
           << errcode << ")");
  }
  return (static_cast<unsigned long>(wb));
#else
  ssize_t wb(::write(get_native_handle(), data, size));
  if (wb <= 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "could not write to file stream: " << msg);
  }
  return (static_cast<unsigned long>(wb));
#endif // Windows or POSIX.
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy constructor.
 *
 *  @param[in] fs Object to copy.
 */
file_stream::file_stream(file_stream const& fs) : handle(fs) {
  _internal_copy(fs);
}

/**
 *  Assignment operator.
 *
 *  @param[in] fs Object to copy.
 *
 *  @return This object.
 */
file_stream& file_stream::operator=(file_stream const& fs) {
  _internal_copy(fs);
  return (*this);
}

/**
 *  Copy internal data members.
 *
 *  @param[in] fs Unused.
 */
void file_stream::_internal_copy(file_stream const& fs) {
  (void)fs;
  assert(!"file stream is not copyable");
  abort();
  return ;
}

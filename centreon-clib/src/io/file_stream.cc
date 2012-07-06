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

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
 *  Check for file existance.
 *
 *  @param[in] path File to check.
 *
 *  @return true if file exists.
 */
bool file_stream::exists(char const* path) {
  if (!path)
    return (false);
#ifdef _WIN32
  return (!_access(path, 0));
#else
  return (!access(path, F_OK));
#endif // Windows or POSIX
}

/**
 *  Overload of exists method.
 */
bool file_stream::exists(std::string const& path) {
  return (exists(path.c_str()));
}

/**
 *  Flush the file to disk.
 */
void file_stream::flush() {
  if (fflush(_stream)) {
    char const* msg(strerror(errno));
    throw (basic_error() << "cannot flush stream: " << msg);
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
  if (!path)
    throw (basic_error() << "invalid argument path: null pointer");
  if (!mode)
    throw (basic_error() << "invalid argument mode: null pointer");

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
 *  Overload of open method.
 */
void file_stream::open(std::string const& path, char const* mode) {
  open(path.c_str(), mode);
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
 *  Remove a file.
 *
 *  @param[in] path Path to the file to remove.
 *
 *  @return true if file was successfully removed.
 */
bool file_stream::remove(char const* path) {
  if (!path)
    return (false);
  return (!::remove(path));
}

/**
 *  Overload of remove method.
 */
bool file_stream::remove(std::string const& path) {
  return (remove(path.c_str()));
}

/**
 *  Rename a file.
 *
 *  @param[in] old_filename  The current filename.
 *  @param[in] new_filename  The new filename.
 *
 *  @return True on success, otherwise false.
 */
bool file_stream::rename(
                    char const* old_filename,
                    char const* new_filename) {
  if (!old_filename || !new_filename)
    return (false);
  bool ret(!::rename(old_filename, new_filename));
  if (!ret) {
    if (errno != EXDEV)
      return (false);
    try {
      file_stream file_read(NULL, true);
      file_read.open(old_filename, "r");
      file_stream file_write(NULL, true);
      file_write.open(new_filename, "w");

      char data[4096];
      unsigned int len;
      while ((len = file_read.read(data, sizeof(data))))
        file_write.write(data, len);
    }
    catch (...) {
      return (false);
    }
  }
  return (true);
}

/**
 *  Overload of rename method.
 */
bool file_stream::rename(
                    std::string const& old_filename,
                    std::string const& new_filename) {
  return (rename(old_filename.c_str(), new_filename.c_str()));
}

/**
 *  Get file size.
 *
 *  @return File size.
 */
unsigned long file_stream::size() {
  // Get original offset.
  long original_offset(ftell(_stream));
  if (-1 == original_offset) {
    char const* msg(strerror(errno));
    throw (basic_error() << "cannot tell position within file: "
           << msg);
  }

  // Seek to end of file.
  if (fseek(_stream, 0, SEEK_END)) {
    char const* msg(strerror(errno));
    throw (basic_error() << "cannot seek to end of file: " << msg);
  }

  // Get position (size).
  long size(ftell(_stream));
  if (size < 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "cannot get file size: " << msg);
  }

  // Get back to original position.
  fseek(_stream, original_offset, SEEK_SET);

  return (size);
}

/**
 *  Get temporary name.
 *
 *  @return Temporary name.
 */
char* file_stream::temp_path() {
  char* ret(::tmpnam(static_cast<char*>(NULL)));
  if (!ret)
    throw (basic_error() << "could not generate temporary file name");
  return (ret);
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

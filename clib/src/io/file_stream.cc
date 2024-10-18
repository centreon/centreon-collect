/**
 * Copyright 2012-2013 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include "com/centreon/io/file_stream.hh"
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::io;
using com::centreon::exceptions::msg_fmt;

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
file_stream::~file_stream() noexcept {
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
  return;
}

/**
 *  Copy source file to the destination file.
 *
 *  @param[in] src The path of the source file.
 *  @param[in] dst The path of the destination file.
 */
void file_stream::copy(char const* src, char const* dst) {
  std::ifstream source(src, std::ios::binary);
  std::ofstream dest(dst, std::ios::binary);
  dest << source.rdbuf();
}

/**
 *  Overload of copy method.
 */
void file_stream::copy(std::string const& src, std::string const& dst) {
  copy(src.c_str(), dst.c_str());
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
    return false;
  return !access(path, F_OK);
}

/**
 *  Overload of exists method.
 */
bool file_stream::exists(std::string const& path) {
  return exists(path.c_str());
}

/**
 *  Flush the file to disk.
 */
void file_stream::flush() {
  if (fflush(_stream)) {
    char const* msg(strerror(errno));
    throw msg_fmt("cannot flush stream: {}", msg);
  }
  return;
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
  else {
    retval = fileno(_stream);
    if (retval < 0) {
      char const* msg(strerror(errno));
      throw msg_fmt("could not get native handle from file stream: {}", msg);
    }
  }
  return retval;
}

/**
 *  Open file.
 */
void file_stream::open(char const* path, char const* mode) {
  if (!path)
    throw msg_fmt("invalid argument path: null pointer");
  if (!mode)
    throw msg_fmt("invalid argument mode: null pointer");

  close();
  _auto_close = true;
  _stream = fopen(path, mode);
  if (!_stream) {
    char const* msg(strerror(errno));
    throw msg_fmt("could not open file '{}': {}", path, msg);
  }
  int fd(fileno(_stream));
  int flags(0);
  while ((flags = fcntl(fd, F_GETFD)) < 0) {
    if (errno == EINTR)
      continue;
    return;
  }
  while (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) {
    if (errno == EINTR)
      continue;
    return;
  }
  return;
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
    throw msg_fmt("attempt to read from closed file stream");
  if (!data || !size)
    throw msg_fmt(
        "attempt to read from file stream but do not except any result");
  ssize_t rb(::read(get_native_handle(), data, size));
  if (rb < 0) {
    char const* msg(strerror(errno));
    throw msg_fmt("could not read from file stream: {}", msg);
  }
  return static_cast<unsigned long>(rb);
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
    return false;
  return !::remove(path);
}

/**
 *  Overload of remove method.
 */
bool file_stream::remove(std::string const& path) {
  return remove(path.c_str());
}

/**
 *  Rename a file.
 *
 *  @param[in] old_filename  The current filename.
 *  @param[in] new_filename  The new filename.
 *
 *  @return True on success, otherwise false.
 */
bool file_stream::rename(char const* old_filename, char const* new_filename) {
  if (!old_filename || !new_filename)
    return false;
  bool ret(!::rename(old_filename, new_filename));
  if (!ret) {
    if (errno != EXDEV)
      return false;
    try {
      file_stream file_read(NULL, true);
      file_read.open(old_filename, "r");
      file_stream file_write(NULL, true);
      file_write.open(new_filename, "w");

      char data[4096];
      unsigned int len;
      while ((len = file_read.read(data, sizeof(data))))
        file_write.write(data, len);
    } catch (...) {
      return false;
    }
  }
  return true;
}

/**
 *  Overload of rename method.
 */
bool file_stream::rename(std::string const& old_filename,
                         std::string const& new_filename) {
  return rename(old_filename.c_str(), new_filename.c_str());
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
    throw msg_fmt("cannot tell position within file: {}", msg);
  }

  // Seek to end of file.
  if (fseek(_stream, 0, SEEK_END)) {
    char const* msg(strerror(errno));
    throw msg_fmt("cannot seek to end of file: {}", msg);
  }

  // Get position (size).
  long size(ftell(_stream));
  if (size < 0) {
    char const* msg(strerror(errno));
    throw msg_fmt("cannot get file size: {}", msg);
  }

  // Get back to original position.
  fseek(_stream, original_offset, SEEK_SET);

  return size;
}

/**
 *  Get temporary name.
 *
 *  @return Temporary name.
 */
char* file_stream::temp_path() {
  char* ret = ::tmpnam(static_cast<char*>(nullptr));
  if (!ret)
    throw msg_fmt("could not generate temporary file name");
  return ret;
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
    throw msg_fmt("attempt to write to a closed file stream");
  if (!data || !size)
    throw msg_fmt("attempt to write no data to file stream");
  ssize_t wb(::write(get_native_handle(), data, size));
  if (wb <= 0) {
    char const* msg(strerror(errno));
    throw msg_fmt("could not write to file stream: {}", msg);
  }
  return static_cast<unsigned long>(wb);
}

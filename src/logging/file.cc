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

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/file.hh"
#include "com/centreon/misc/stringifier.hh"

using namespace com::centreon::logging;

/**
 *  Default constructor.
 *
 *  @param[in] file            The file to used.
 *  @param[in] is_sync         Enable synchronization.
 *  @param[in] show_pid        Enable show pid.
 *  @param[in] show_timestamp  Enable show timestamp.
 *  @param[in] show_thread_id  Enable show thread id.
 */
file::file(
        FILE* file,
        bool is_sync,
        bool show_pid,
        time_precision show_timestamp,
        bool show_thread_id)
  : backend(is_sync, show_pid, show_timestamp, show_thread_id),
    _out(file) {

}

/**
 *  Constructor with file path name.
 *
 *  @param[in] path  The path of the file to used.
 *  @param[in] is_sync         Enable synchronization.
 *  @param[in] show_pid        Enable show pid.
 *  @param[in] show_timestamp  Enable show timestamp.
 *  @param[in] show_thread_id  Enable show thread id.
 */
file::file(
        std::string const& path,
        bool is_sync,
        bool show_pid,
        time_precision show_timestamp,
        bool show_thread_id)
  : backend(is_sync, show_pid, show_timestamp, show_thread_id),
    _path(path),
    _out(NULL) {
  open();
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
  close();
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
 *  Close file.
 */
void file::close() throw () {
  concurrency::locker lock(&_lock);

  if (!_out || _out == stdout || _out == stderr)
    return;

  int ret;
  do {
    ret = fclose(_out);
  } while (ret == -1 && errno == EINTR);
  _out = NULL;
}

/**
 *  Get filename.
 *
 *  @return The filename string.
 */
std::string const& file::filename() const throw () {
  return (_path);
}

/**
 *  Write message into the file.
 *  @remark This method is thread safe.
 *
 *  @param[in] type     Logging types.
 *  @param[in] verbose  Verbosity level.
 *  @param[in] msg      The message to write.
 *  @param[in] size     The message's size.
 */
void file::log(
             unsigned long long types,
             unsigned int verbose,
             char const* msg,
             unsigned int size) throw () {
  (void)types;
  (void)verbose;
  (void)size;

  misc::stringifier header;
  _build_header(header);

  // Split msg by line.
  misc::stringifier buffer;
  unsigned int i(0);
  unsigned int last(0);
  while (msg[i]) {
    if (msg[i] == '\n') {
      buffer << header;
      buffer.append(msg + last, i - last) << "\n";
      last = i + 1;
    }
    ++i;
  }
  if (last != i) {
    buffer << header;
    buffer.append(msg + last, i - last) << "\n";
  }

  concurrency::locker lock(&_lock);
  if (_out) {
    size_t ret;
    do {
      clearerr(_out);
      ret = fwrite(buffer.data(), buffer.size(), 1, _out);
    } while (ret != 1 && ferror(_out) && errno == EINTR);

    // Flush data if is necessary.
    while (_is_sync && fflush(_out) < 0 && errno == EINTR)
      ;
  }
}

/**
 *  Open file.
 */
void file::open() {
  concurrency::locker lock(&_lock);

  if (_out && _path.empty())
    return;

  if (!(_out = fopen(_path.c_str(), "a")))
    throw (basic_error() << "failed to open file \"" << _path << "\":"
           << strerror(errno));
}

/**
 *  Close and open file.
 */
void file::reopen() {
  concurrency::locker lock(&_lock);

  if (!_out || _out == stdout || _out == stderr)
    return;

  int ret;
  do {
    ret = fclose(_out);
  } while (ret == -1 && errno == EINTR);

  if (!(_out = fopen(_path.c_str(), "a")))
    throw (basic_error() << "failed to open file \"" << _path << "\":"
           << strerror(errno));
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

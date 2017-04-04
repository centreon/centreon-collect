/*
** Copyright 2017 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include <arpa/inet.h>
#include <limits>
#include <memory>
#include <sstream>
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/file/cfile.hh"
#include "com/centreon/broker/file/qt_fs_browser.hh"
#include "com/centreon/broker/file/splitter.hh"
#include "com/centreon/broker/io/exceptions/shutdown.hh"
#include "com/centreon/broker/logging/logging.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::file;

/**
 *  Build a new splitter.
 *
 *  @param[in] path           Base path to file.
 *  @param[in] mode           Ignored. Files will always be open
 *                            read/write.
 *  @param[in] file_factory   fs_file_factory to work with single files.
 *  @param[in] fs             fs_browser object to manipulate file
 *                            system.
 *  @param[in] max_file_size  Maximum single file size.
 *  @param[in] auto_delete    True to delete file parts as they are
 *                            read.
 */
splitter::splitter(
            std::string const& path,
            fs_file::open_mode mode,
            fs_file_factory* file_factory,
            fs_browser* fs,
            long max_file_size,
            bool auto_delete)
  : _auto_delete(auto_delete),
    _base_path(path),
    _file_factory(file_factory),
    _fs(fs),
    _max_file_size(max_file_size),
    _rid(0),
    _roffset(0),
    _wid(0),
    _woffset(0) {
  (void)mode;

  // Set max file size.
  static long min_file_size(10000);
  if (!_max_file_size)
    _max_file_size = std::numeric_limits<long>::max();
  else if (_max_file_size < min_file_size)
    _max_file_size = min_file_size;

  // Get IDs of already existing file parts. File parts are suffixed
  // with their order number. A file named /var/lib/foo would have
  // parts named /var/lib/foo, /var/lib/foo1, /var/lib/foo2, ...
  // in this order.
  std::string base_dir;
  std::string base_name;
  {
    size_t last_slash(_base_path.find_last_of('/'));
    if (last_slash == std::string::npos) {
      base_dir = ".";
      base_name = _base_path;
    }
    else {
      base_dir = _base_path.substr(0, last_slash).c_str();
      base_name = _base_path.substr(last_slash + 1).c_str();
    }
  }
  base_name.append("*");
  fs_browser::entry_list parts(_fs->read_directory(
                                      base_dir,
                                      base_name));
  _rid = std::numeric_limits<int>::max();
  _wid = 0;
  for (fs_browser::entry_list::iterator
         it(parts.begin()),
         end(parts.end());
       it != end;
       ++it) {
    char const* ptr(it->c_str() + base_name.size());
    int val(0);
    if (*ptr) { // Not, empty, conversion needed.
      char* endptr(NULL);
      val = strtol(ptr, &endptr, 10);
      if (ptr && *ptr) // Invalid conversion.
        continue ;
    }

    if (val < _rid)
      _rid = val;
    if (val > _wid)
      _wid = val;
  }
  if (_rid == std::numeric_limits<int>::max())
    _rid = 0;

  // File IDs will be incremented when opening next files.
  --_rid;
  --_wid;
}

/**
 *  Destructor.
 */
splitter::~splitter() {}

/**
 *  Read data.
 *
 *  @param[out] buffer    Output buffer.
 *  @param[in]  max_size  Maximum number of bytes that can be read.
 *
 *  @return Number of bytes read.
 */
long splitter::read(void* buffer, long max_size) {
  // Check if we should read.
  if (_rfile.isNull()) {
    // If read-ID equals write-ID, then we're finished.
    if (_rid >= _wid) {
      _wfile.clear();
      throw (io::exceptions::shutdown(true, true)
             << "end of file");
    }
    // Otherwise open next ID.
    _open_next_read();
  }
  // Seek to position.
  else
    _rfile->seek(_roffset);

  // Read data.
  long rb;
  try {
    rb = _rfile->read(buffer, max_size);
  }
  catch (io::exceptions::shutdown const& e) {
    (void)e;
    if (_wid == _rid) {
      _rfile.clear();
      _wfile.clear();
      std::string file_path(_file_path(_rid));
      if (_auto_delete) {
        logging::info(logging::high) << "file: end of last file '"
          << file_path.c_str() << "' reached, closing and erasing file";
        ::remove(file_path.c_str());
      }
      else {
        logging::info(logging::high) << "file: end of last file '"
          << file_path.c_str() << "' reached, closing but NOT erasing file";
      }
      throw ;
    }
    _open_next_read();
    rb = _rfile->read(buffer, max_size);
  }

  // Process data.
  logging::debug(logging::low) << "file: read " << rb << " bytes from '"
    << _file_path(_rid).c_str() << "'";
  _roffset += rb;
  return (rb);
}

/**
 *  Throw an exception.
 *
 *  @param[in] offset  Unused.
 *  @param[in] whence  Unused.
 */
void splitter::seek(long offset, fs_file::seek_whence whence) {
  (void)offset;
  (void)whence;
  throw (exceptions::msg() << "cannot seek within a splitted file");
}

/**
 *  Get current position.
 *
 *  @return Current position in file.
 */
long splitter::tell() {
  return (_roffset);
}

/**
 *  Write data.
 *
 *  @param[in] buffer  Data.
 *  @param[in] size    Number of bytes in buffer.
 *
 *  @return Number of bytes written.
 */
long splitter::write(void const* buffer, long size) {
  // Open next write file if necessary.
  if (_wfile.isNull()
      || (_woffset + size) > _max_file_size)
    _open_next_write();
  // Otherwise seek to end of file.
  else
    _wfile->seek(_woffset);

  // Debug message.
  logging::debug(logging::low) << "file: write request of "
    << size << " bytes for '" << _file_path(_wid).c_str() << "'";

  // Write data.
  while (size > 0) {
    unsigned long wb(_wfile->write(buffer, size));
    size -= wb;
    _woffset += wb;
    buffer = static_cast<char const*>(buffer) + wb;
  }

  return (size);
}

/**
 *  Get the file path matching the ID.
 *
 *  @param[in] id Current ID.
 */
std::string splitter::_file_path(int id) const {
  if (id) {
    std::ostringstream oss;
    oss << _base_path << id;
    return (oss.str());
  }
  else
    return (_base_path);
}

/**
 *  Open the next readable file.
 */
void splitter::_open_next_read() {
  // Did we reached the write file ?
  _rfile.clear();
  ++_rid;
  if (_rid == _wid)
    _rfile = _wfile;
  else {
    // Open next file.
    std::string file_path(_file_path(_rid));
    {
      misc::shared_ptr<fs_file>
        new_file(_file_factory->new_fs_file(
                                  file_path,
                                  fs_file::open_read_write_no_create));
      _rfile = new_file;
    }
  }
  _roffset = 2 * sizeof(uint32_t);
  _rfile->seek(_roffset);

  // Remove previous file.
  std::string file_path(_file_path(_rid - 1));
  if (_auto_delete) {
    logging::info(logging::high) << "file: end of file '"
      << file_path.c_str() << "' reached, erasing file";
    ::remove(file_path.c_str());
  }
  else {
    logging::info(logging::high) << "file: end of file '"
      << file_path.c_str() << "' reached, NOT erasing file";
  }

  return ;
}

/**
 *  Open the next writable file.
 */
void splitter::_open_next_write() {
  // Open file.
  _wfile.clear();
  ++_wid;
  std::string file_path(_file_path(_wid));
  logging::info(logging::high) << "file: opening new file '"
    << file_path.c_str() << "'";
  {
    try {
      _wfile = _file_factory->new_fs_file(
                                file_path,
                                fs_file::open_read_write_no_create);
    }
    catch (exceptions::msg const& e) {
      _wfile = _file_factory->new_fs_file(
                                file_path,
                                fs_file::open_read_write_truncate);
    }
  }

  // Position.
  _wfile->seek(0, fs_file::seek_end);
  _woffset = _wfile->tell();

  if (_woffset < static_cast<long>(2 * sizeof(uint32_t))) {
    // Rewind to file beginning.
    _wfile->seek(0);

    // Write read offset.
    union {
      char     bytes[2 * sizeof(uint32_t)];
      uint32_t integers[2];
    } header;
    header.integers[0] = 0;
    header.integers[1] = htonl(2 * sizeof(uint32_t));
    unsigned int size(0);
    while (size < sizeof(header))
      size += _wfile->write(header.bytes + size, sizeof(header) - size);

    // Set current offset.
    _woffset = 2 * sizeof(uint32_t);
  }

  return ;
}

/**
 *  Build a new default splitter.
 *
 *  @param[in] path  Path to file.
 *  @param[in] mode  Open mode (ignored).
 *
 *  @return A new default splitter.
 */
fs_file* splitter_factory::new_fs_file(
                             std::string const& path,
                             fs_file::open_mode mode) {
  return (new_cfile_splitter(path, mode));
}

/**
 *  Build a new cfile splitter.
 *
 *  @param[in] path           Path to file.
 *  @param[in] mode           Open mode (ignored).
 *  @param[in] max_file_size  Max single file size.
 *  @param[in] auto_delete    True to delete file parts as they are
 *                            read.
 *
 *  @return A new cfile splitter.
 */
splitter* splitter_factory::new_cfile_splitter(
                              std::string const& path,
                              fs_file::open_mode mode,
                              long max_file_size,
                              bool auto_delete) {
  std::auto_ptr<fs_file_factory> f(new cfile_factory());
  std::auto_ptr<fs_browser> b(new qt_fs_browser());
  splitter* s(new splitter(
                    path,
                    mode,
                    f.get(),
                    b.get(),
                    max_file_size,
                    auto_delete));
  f.release();
  b.release();
  return (s);
}

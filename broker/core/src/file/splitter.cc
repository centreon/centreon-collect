/**
 * Copyright 2020-2023 Centreon
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

#include "com/centreon/broker/file/splitter.hh"

#include <arpa/inet.h>

#include <cassert>

#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/file/cfile.hh"
#include "com/centreon/broker/file/disk_accessor.hh"
#include "com/centreon/broker/misc/filesystem.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::file;

using com::centreon::common::log_v2::log_v2;

/**
 *  Build a new splitter.
 *
 *  @param[in] path           Base path to file.
 *  @param[in] mode           Ignored. Files will always be open
 *                            read/write.
 *  @param[in] max_file_size  Maximum single file size.
 *  @param[in] auto_delete    True to delete file parts as they are
 *                            read.
 */
splitter::splitter(std::string const& path,
                   uint32_t max_file_size,
                   bool auto_delete)
    : _auto_delete{auto_delete},
      _base_path{path},
      _max_file_size{max_file_size == 0u ? std::numeric_limits<uint32_t>::max()
                                         : std::max(max_file_size, 10000u)},
      _rfile{},
      _roffset{0},
      _write_m{},
      _wfile{},
      _woffset{0} {
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
    } else {
      base_dir = _base_path.substr(0, last_slash);
      base_name = _base_path.substr(last_slash + 1);
    }
  }
  std::list<std::string> parts{
      misc::filesystem::dir_content_with_filter(base_dir, base_name + '*')};
  _rid = std::numeric_limits<int>::max();
  _wid = 0;
  size_t offset{base_dir.size() + base_name.size()};
  if (!base_dir.empty() && base_dir.back() != '/')
    offset++;
  size_t size = 0;
  struct stat file_stat;
  for (auto& f : parts) {
    const char* ptr{f.c_str() + offset};
    int val = 0;
    if (*ptr) {  // Not empty, conversion needed.
      char* endptr = nullptr;
      val = strtol(ptr, &endptr, 10);
      if (endptr && *endptr)  // Invalid conversion.
        continue;
    }

    if (val < _rid)
      _rid = val;
    if (val > _wid)
      _wid = val;

    if (stat(f.c_str(), &file_stat) == 0)
      size += file_stat.st_size;
  }
  disk_accessor::instance().set_current_size(size);

  if (_rid == std::numeric_limits<int>::max() || _rid < 0)
    _rid = 0;

  assert(_rid <= _wid);

  _open_write_file();
}

/**
 *  Destructor.
 */
splitter::~splitter() {
  close();
}

/**
 *  Close files open by splitter.
 *  If no files are open, nothing is done.
 */
void splitter::close() {
  std::lock_guard<std::mutex> lck(_write_m);
  if (_rfile)
    _rfile.reset();

  if (_wfile)
    _wfile.reset();
}

/**
 *  Read data.
 *
 *  @param[out] buffer    Output buffer.
 *  @param[in]  max_size  Maximum number of bytes that can be read.
 *
 *  @return Number of bytes read.
 */
long splitter::read(void* buffer, long max_size) {
  /* No lock here, there is only one consumer. */
  if (!_rfile) {
    _open_read_file();
    if (!_rfile)
      return 0;
  }

  /* We introduce the locker, but don't lock if not necessary */
  std::unique_lock<std::mutex> lck(_write_m, std::defer_lock);

  /* Here, _wid is atomic so we can read it. Maybe when we'll lock _wid will
   * be greater but it is not so important. Usually, if _rid == _wid, we read
   * and write in the same file, so we have to lock the mutex. */
  if (_rid == _wid)
    lck.lock();

  // Seek to current read position.
  fseek(_rfile.get(), _roffset, SEEK_SET);

  auto logger = log_v2::instance().get(log_v2::BBDO);
  // Read data.
  long rb = disk_accessor::instance().fread(buffer, 1, max_size, _rfile.get());
  std::string file_path(get_file_path(_rid));
  logger->debug("splitter: read {} bytes at offset {} from '{}'", rb, _roffset,
                file_path);
  _roffset += rb;
  if (rb == 0) {
    if (feof(_rfile.get())) {
      if (_auto_delete) {
        logger->info("file: end of file '{}' reached, erasing it", file_path);
        /* Here we have to really verify that _wfile and _rfile are the same,
         * and then we close files before removing them. */
        if (lck.owns_lock() && _wfile == _rfile) {
          _rfile.reset();
          _wfile.reset();
        } else
          _rfile.reset();
        disk_accessor::instance().remove(file_path);
      }
      if (_rid < _wid) {
        _rid++;
        /* As we said earlier, maybe we locked lck abusively while _rid < _wid
         */
        if (lck.owns_lock())
          lck.unlock();
        return read(buffer, max_size);
      } else
        throw exceptions::shutdown("No more data to read");
    } else {
      if (errno == EAGAIN || errno == EINTR)
        return 0;
      else {
        char msg[1024];
        throw msg_fmt("error while reading file '{}': {}", file_path,
                      strerror_r(errno, msg, sizeof(msg)));
      }
    }
  }
  return rb;
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
  throw msg_fmt("cannot seek within a splitted file");
}

/**
 *  Get current position.
 *
 *  @return Current position in file.
 */
long splitter::tell() {
  return _roffset;
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
  /* It is impossible for two threads to write at the same time. */
  std::lock_guard<std::mutex> lck(_write_m);
  if (!_wfile)
    if (!_open_write_file())
      return 0;

  auto logger = log_v2::instance().get(log_v2::BBDO);
  // Open next write file is max file size is reached.
  if ((_woffset + size) > _max_file_size) {
    if (fflush(_wfile.get())) {
      logger->error("splitter: cannot flush file '{}'", get_file_path(_wid));
      char msg[1024];
      throw msg_fmt("cannot flush file '{}': {}", get_file_path(_wid),
                    strerror_r(errno, msg, sizeof(msg)));
    }
    ++_wid;
    if (!_open_write_file())
      return 0;
  }
  // Otherwise seek to end of file.

  fseek(_wfile.get(), _woffset, SEEK_SET);

  // Debug message.
  logger->debug("file: write request of {} bytes for '{}'", size,
                get_file_path(_wid));

  // Write data.
  long wb = disk_accessor::instance().fwrite(buffer, 1, size, _wfile.get());
  if (wb != size) {
    std::string wfile(get_file_path(_wid));
    char msg[1024];
    logger->critical("splitter: cannot write to file '{}': {}", wfile,
                     strerror_r(errno, msg, sizeof(msg)));
    return 0;
  }
  _woffset += size;
  return size;
}

/**
 *  Flush the write stream.
 */
void splitter::flush() {
  std::lock_guard<std::mutex> lck(_write_m);
  if (fflush(_wfile.get()) == EOF) {
    char msg[1024];
    throw msg_fmt("error while writing the file '{}' content: {}",
                  get_file_path(_wid), strerror_r(errno, msg, sizeof(msg)));
  }
}

/**
 *  Get the file path matching the ID.
 *
 *  @param[in] id Current ID.
 */
std::string splitter::get_file_path(int id) const {
  if (id)
    return fmt::format("{}{}", _base_path, id);
  else
    return _base_path;
}

/**
 *  Get max file size.
 *
 *  @return Max file size.
 */
size_t splitter::max_file_size() const {
  return _max_file_size;
}

/**
 *  Get current read ID.
 *
 *  @return Current read ID.
 */
int splitter::get_rid() const {
  return _rid;
}

/**
 *  Get current read offset.
 *
 *  @return Current read offset.
 */
long splitter::get_roffset() const {
  return _roffset;
}

/**
 *  Get current write ID.
 *
 *  @return Current write ID.
 */
int splitter::get_wid() const {
  return _wid;
}

/**
 *  Get current write offset.
 *
 *  @return Current write offset.
 */
long splitter::get_woffset() const {
  return _woffset;
}

/**
 *  Remove all the files the splitter is concerned by.
 */
void splitter::remove_all_files() {
  std::lock_guard<std::mutex> lck(_write_m);
  if (_rfile)
    _rfile.reset();

  if (_wfile)
    _wfile.reset();
  std::string base_dir;
  std::string base_name;
  {
    size_t last_slash(_base_path.find_last_of('/'));
    if (last_slash == std::string::npos) {
      base_dir = "./";
      base_name = _base_path;
    } else {
      base_dir = _base_path.substr(0, last_slash + 1);
      base_name = _base_path.substr(last_slash + 1);
    }
  }
  std::list<std::string> parts{
      misc::filesystem::dir_content_with_filter(base_dir, base_name + '*')};
  for (const std::string& f : parts)
    disk_accessor::instance().remove(f);

  /* No more files, we reset rid and wid. */
  _rid = 0;
  _wid = 0;
}

/**
 * @brief Open the splitter in read mode.
 */
void splitter::_open_read_file() {
  bool done = false;
  {
    std::lock_guard<std::mutex> lck(_write_m);
    if (_rid == _wid && _wfile) {
      _rfile = _wfile;
      done = true;
    }
  }

  if (!done) {
    std::string fname(get_file_path(_rid));
    FILE* f = disk_accessor::instance().fopen(fname, "r+b");
    auto logger = log_v2::instance().get(log_v2::BBDO);
    if (f)
      logger->debug("splitter: read open '{}'", fname);
    else
      logger->error("splitter: read fail open '{}'", fname);

    _rfile = f ? std::shared_ptr<FILE>(f, fclose) : std::shared_ptr<FILE>();
  }

  if (!_rfile) {
    if (errno == ENOENT)
      return;
    else {
      char msg[1024];
      throw msg_fmt("cannot open '{}' to read: {}", get_file_path(_rid),
                    strerror_r(errno, msg, sizeof(msg)));
    }
  }
  _roffset = 2 * sizeof(uint32_t);
  fseek(_rfile.get(), _roffset, SEEK_SET);
}

/**
 * @brief Open the splitter in write mode. This call must be protected by the
 * _write_m mutex.
 *
 * @return True on success, False otherwise.
 */
bool splitter::_open_write_file() {
  std::string fname(get_file_path(_wid));
  FILE* f = disk_accessor::instance().fopen(fname, "a+b");
  auto logger = log_v2::instance().get(log_v2::BBDO);
  if (f)
    logger->debug("splitter: write open '{}'", fname);
  else
    logger->error("splitter: write fail open '{}'", fname);

  _wfile = f ? std::shared_ptr<FILE>(f, fclose) : std::shared_ptr<FILE>();

  if (!_wfile) {
    char msg[1024];
    throw msg_fmt("cannot open '{}' to read/write: {}", get_file_path(_wid),
                  strerror_r(errno, msg, sizeof(msg)));
  }

  fseek(_wfile.get(), 0, SEEK_END);
  _woffset = ftell(_wfile.get());

  // Ensure 8-bytes header is written at file beginning.
  if (_woffset < static_cast<uint32_t>(2 * sizeof(uint32_t))) {
    fseek(_wfile.get(), 0, SEEK_SET);
    union {
      char bytes[2 * sizeof(uint32_t)];
      uint32_t integers[2];
    } header;
    header.integers[0] = 0;
    header.integers[1] = htonl(2 * sizeof(uint32_t));
    size_t size = disk_accessor::instance().fwrite(
        header.bytes, 1, sizeof(header), _wfile.get());
    if (size != sizeof(header)) {
      std::string wfile(get_file_path(_wid));
      char msg[1024];
      logger->critical("splitter: cannot write to file '{}': {}", wfile,
                       strerror_r(errno, msg, sizeof(msg)));
      _wfile.reset();
      return false;
    }
    _woffset = 2 * sizeof(uint32_t);
  }
  return true;
}

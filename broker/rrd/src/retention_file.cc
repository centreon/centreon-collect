/**
 * Copyright 2026 Centreon
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

#include <absl/cleanup/cleanup.h>
#include <sys/mman.h>
#include <filesystem>
#include <system_error>

#include "com/centreon/broker/rrd/retention_file.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker::rrd;

/**
 * @brief Create a new retention file for a metric and write its first point.
 *
 * The file is named <metric_id>_metric.ret and created in rrd_data_dir. Its
 * header (value type, rrd_len, interval) is initialized from first_metric.
 *
 * @param rrd_data_dir Directory where the file is created.
 * @param first_metric First metric point, also written in the file.
 */
retention_file::retention_file(const std::filesystem::path& rrd_data_dir,
                               const Metric& first_metric)
    : _last_activity(time(nullptr)), _nb_samples(0) {
  _file_path = rrd_data_dir / fmt::format("{}.ret", first_metric.metric_id());

  _header.id = first_metric.metric_id();
  _header.is_status = false;
  _header.value_type = first_metric.value_type();
  _header.rrd_len = first_metric.rrd_len();
  _header.interval = first_metric.interval();
  _header.first_ts = std::numeric_limits<uint64_t>::max();
  _header.last_ts = 0;

  _create_file();
  add_metric(first_metric);
}

/**
 * @brief Create a new retention file for a status and write its first point.
 *
 * The file is named <index_id>_status.ret and created in rrd_data_dir. Status
 * RRD files are always GAUGE.
 *
 * @param rrd_data_dir Directory where the file is created.
 * @param first_status First status point, also written in the file.
 */
retention_file::retention_file(const std::filesystem::path& rrd_data_dir,
                               const Status& first_status)
    : _last_activity(time(nullptr)), _nb_samples(0) {
  _file_path = rrd_data_dir / fmt::format("{}.ret", first_status.index_id());
  _header.id = first_status.index_id();
  _header.is_status = true;
  _header.value_type = Metric::ValueType::Metric_ValueType_GAUGE;
  _header.rrd_len = first_status.rrd_len();
  _header.interval = first_status.interval();
  _header.first_ts = std::numeric_limits<uint64_t>::max();
  _header.last_ts = 0;
  _create_file();
  add_status(first_status);
}

/**
 * @brief Open an existing retention file and load its header.
 *
 * If the header can't be read, the file is removed and an exception is thrown.
 *
 * @param file_path Path of the retention file.
 */
retention_file::retention_file(const std::filesystem::path& file_path)
    : _file_path(file_path), _last_activity(time(nullptr)) {
  int fd = ::open(_file_path.c_str(), O_RDWR);
  if (fd < 0) {
    throw exceptions::msg_fmt("fail to open {}: {}", _file_path.string(),
                              strerror(errno));
  }
  absl::Cleanup closer = [fd]() { ::close(fd); };

  int nb_read = ::read(fd, &_header, sizeof(header));
  if (nb_read != sizeof(header)) {
    int save_errno = errno;
    ::remove(_file_path.c_str());
    throw exceptions::msg_fmt("fail to read header from file {}: {}",
                              _file_path.string(), strerror(save_errno));
  }
  std::error_code err;
  size_t file_size = std::filesystem::file_size(_file_path, err);
  if (err) {
    _nb_samples = (file_size - sizeof(header)) / sizeof(sample);
  }
}

/**
 * @brief Append a slot at the end of the file.
 *
 * Slots are not sorted: they are written in their arrival order. If the slot
 * timestamp extends the [first_ts, last_ts] range, the header is updated and
 * rewritten at the beginning of the file.
 *
 * @param to_append The slot to write.
 */
void retention_file::_append(const sample& to_append) {
  int fd = ::open(_file_path.c_str(), O_WRONLY);
  if (fd < 0) {
    throw exceptions::msg_fmt("fail to open {}: {}", _file_path.string(),
                              strerror(errno));
  }

  off_t pos = lseek(fd, 0, SEEK_END);
  if (pos == (off_t)-1) {
    int save_errno = errno;
    ::close(fd);
    throw exceptions::msg_fmt("fail to write metric at the end of file {}: {}",
                              _file_path.string(), strerror(save_errno));
  }

  int nb_write = ::write(fd, &to_append, sizeof(to_append));
  if (nb_write != sizeof(to_append)) {
    int save_errno = errno;
    ::close(fd);
    throw exceptions::msg_fmt("fail to write metric to file {}: {}",
                              _file_path.string(), strerror(save_errno));
  }

  auto update_header = [&, this]() {
    lseek(fd, 0, SEEK_SET);
    nb_write = ::write(fd, &_header, sizeof(header));
    if (nb_write != sizeof(header)) {
      int save_errno = errno;
      ::close(fd);
      throw exceptions::msg_fmt("fail to udpate header to file {}: {}",
                                _file_path.string(), strerror(save_errno));
    }
  };

  if (to_append.ts < _header.first_ts || _header.last_ts < to_append.ts) {
    if (_header.first_ts > to_append.ts) {
      _header.first_ts = to_append.ts;
    }
    if (_header.last_ts < to_append.ts) {
      _header.last_ts = to_append.ts;
    }
    update_header();
  }
  ::close(fd);
  ++_nb_samples;
  _last_activity = time(nullptr);
}

/**
 * @brief Append a metric point (time, value) to the file.
 *
 * @param metric The metric point, its value is stored as a double.
 * @throw msg_fmt if this file is a status file.
 */
void retention_file::add_metric(const Metric& metric) {
  if (_header.is_status) {
    throw exceptions::msg_fmt("can not write metric in status file {}",
                              _file_path);
  }
  _append(sample{.ts = metric.time(), .val = {.d_value = metric.value()}});
}

constexpr int64_t UNDEF_STATUS = 0xFFFF;

/**
 * @brief Append a status point (time, state) to the file.
 *
 * @param status The status point, its state is stored as an integer.
 * @throw msg_fmt if this file is a metric file.
 */
void retention_file::add_status(const Status& status) {
  if (!_header.is_status) {
    throw exceptions::msg_fmt("can not write status in metric file {}",
                              _file_path);
  }

  switch (status.state()) {
    case 0:
      _append(sample{.ts = status.time(), .val = {.i_value = 100}});
      break;
    case 1:
      _append(sample{.ts = status.time(), .val = {.i_value = 75}});
      break;
    case 2:
      _append(sample{.ts = status.time(), .val = {.i_value = 0}});
      break;
    default:
      _append(sample{.ts = status.time(), .val = {.i_value = UNDEF_STATUS}});
      break;
  }
}

/**
 * @brief Create the file (or open it if it exists) and write _header at its
 * beginning.
 *
 * If the header can't be written, the file is removed and an exception is
 * thrown.
 */
void retention_file::_create_file() {
  int fd = ::open(_file_path.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
  if (fd < 0) {
    throw exceptions::msg_fmt("fail to open {}: {}", _file_path.string(),
                              strerror(errno));
  }
  absl::Cleanup closer = [fd]() { ::close(fd); };
  int nb_written = ::write(fd, &_header, sizeof(header));
  if (nb_written != sizeof(header)) {
    int save_errno = errno;
    ::remove(_file_path.c_str());
    throw exceptions::msg_fmt("fail to write header of file {}: {}",
                              _file_path.string(), strerror(save_errno));
  }
}

void retention_file::_to_rrd_file(const std::filesystem::path& dst_path,
                                  lib& rrd_lib) {
  int fd = ::open(_file_path.c_str(), O_RDWR);
  if (fd < 0) {
    throw exceptions::msg_fmt("fail to open {}: {}", _file_path.string(),
                              strerror(errno));
  }

  absl::Cleanup closer = [fd]() { ::close(fd); };

  struct stat file_stat;
  if (::fstat(fd, &file_stat)) {
    throw exceptions::msg_fmt("fail to get size of file {}: {}",
                              _file_path.string(), strerror(errno));
  }
  size_t file_size = file_stat.st_size;
  if (file_size < sizeof(header)) {
    throw exceptions::msg_fmt("file {} is too small: {} bytes",
                              _file_path.string(), file_size);
  }

  std::deque<std::string> samples;

  if (file_size > sizeof(header)) {
    void* addr =
        ::mmap(nullptr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
      throw exceptions::msg_fmt("fail to map file {}: {}", _file_path.string(),
                                strerror(errno));
    }
    file_content* data = static_cast<file_content*>(addr);
    sample* begin = data->data;
    sample* end = begin + (file_size - sizeof(header)) / sizeof(sample);
    std::sort(begin, end, [](const sample& left, const sample& right) {
      return left.ts < right.ts;
    });
    ::munmap(data, file_size);
    if (_header.is_status) {
      for (; begin != end; ++begin) {
        if (begin->val.i_value == UNDEF_STATUS) {
          samples.emplace_back(fmt::format("{}:U", begin->ts));
        } else {
          samples.emplace_back(
              fmt::format("{}:{}", begin->ts, begin->val.i_value));
        }
      }
    } else {
      for (; begin != end; ++begin) {
        samples.emplace_back(
            fmt::format("{}:{}", begin->ts, begin->val.d_value));
      }
    }
  }
  rrd_lib.merge_create_temp(dst_path, _header.rrd_len, _header.first_ts,
                            _header.interval, _header.value_type, samples);
}

void retention_file::merge(const std::filesystem::path& rrd_path,
                           lib& rrd_lib) {
  time_t first_ts = rrd_lib.first_timestamp(rrd_path);

  if (first_ts <= 0 || (uint64_t)first_ts > _header.first_ts) {
    first_ts = _header.first_ts;
  }
  std::filesystem::path to_merge_path = rrd_path;
  to_merge_path += ".to_merge";
  std::filesystem::path tmp_merge_path = rrd_path;
  tmp_merge_path += ".tmp";
  absl::Cleanup closer = [&to_merge_path, &tmp_merge_path]() {
    ::remove(to_merge_path.c_str());
    ::remove(tmp_merge_path.c_str());
  };
  if (!_abort_merge) {
    // first, we create rrd file to merge in dst file
    _to_rrd_file(to_merge_path, rrd_lib);
  }
  if (!_abort_merge) {
    rrd_lib.merge_files(tmp_merge_path, to_merge_path, _header.rrd_len,
                        first_ts, _header.interval, _header.value_type);
  }
  if (!_abort_merge) {
    ::rename(tmp_merge_path.c_str(), rrd_path.c_str());
  }
}

/**
 * @brief Map the slots of the file in read only mode.
 *
 * @return An object that gives access to the slots through begin() and end()
 * and unmaps the file when it is destroyed.
 */
// const retention_file::mapped_sample retention_file::sort_and_get_samples() {
//   return mapped_sample(_file_path);
// }

// retention_file::mapped_sample::mapped_sample()
//     : _data(nullptr), _file_size(0), _begin(nullptr), _end(nullptr) {}

// retention_file::mapped_sample::mapped_sample(mapped_sample&& src)
//     : _data(src._data),
//       _file_size(src._file_size),
//       _begin(src._begin),
//       _end(src._end) {
//   src._data = nullptr;
//   src._file_size = 0;
//   src._begin = nullptr;
//   src._end = nullptr;
// }

// /**
//  * @brief Map in read only mode the slots of a retention file.
//  *
//  * If the file contains no slot, nothing is mapped and begin() == end() ==
//  * nullptr.
//  *
//  * @param path Path of the retention file.
//  */
// retention_file::mapped_sample::mapped_sample(const std::filesystem::path&
// path)
//     : _data(nullptr), _file_size(0), _begin(nullptr), _end(nullptr) {
//   int fd = ::open(path.c_str(), O_RDWR);
//   if (fd < 0) {
//     throw exceptions::msg_fmt("fail to open {}: {}", path.string(),
//                               strerror(errno));
//   }

//   absl::Cleanup closer = [fd]() { ::close(fd); };

//   struct stat file_stat;
//   if (::fstat(fd, &file_stat)) {
//     throw exceptions::msg_fmt("fail to get size of file {}: {}",
//     path.string(),
//                               strerror(errno));
//   }
//   _file_size = file_stat.st_size;
//   if (_file_size < sizeof(header)) {
//     throw exceptions::msg_fmt("file {} is too small: {} bytes",
//     path.string(),
//                               _file_size);
//   }
//   // no slot => nothing to map
//   if (_file_size == sizeof(header))
//     return;

//   void* addr =
//       ::mmap(nullptr, _file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
//   if (addr == MAP_FAILED) {
//     throw exceptions::msg_fmt("fail to map file {}: {}", path.string(),
//                               strerror(errno));
//   }
//   _data = static_cast<file_content*>(addr);
//   sample* begin = _data->data;
//   _begin = begin;
//   sample* end = begin + (_file_size - sizeof(header)) / sizeof(sample);
//   _end = end;
//   std::sort(begin, end, [](const sample& left, const sample& right) {
//     return left.ts < right.ts;
//   });
// }

// /**
//  * @brief Unmap the file (the file descriptor is closed by the constructor).
//  */
// retention_file::mapped_sample::~mapped_sample() {
//   if (_data) {
//     ::munmap(_data, _file_size);
//   }
// }
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

#include "com/centreon/broker/rrd/retention_file.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker::rrd;

/**
 * @brief Create a new retention file for a metric and write its first point.
 *
 * The file is named <metric_id>.ret and created in rrd_data_dir. Its header
 * (value type, rrd_len, interval) is initialized from first_metric.
 *
 * @param rrd_data_dir Directory where the file is created.
 * @param first_metric First metric point, also written in the file.
 * @throw msg_fmt if the file already exists or can't be written.
 */
retention_file::retention_file(const std::filesystem::path& rrd_data_dir,
                               const Metric& first_metric)
    : _last_activity(time(nullptr)),
      _nb_samples(0),
      _first_ts(std::numeric_limits<uint64_t>::max()),
      _last_ts(0) {
  _file_path = rrd_data_dir / fmt::format("{}.ret", first_metric.metric_id());

  _header.id = first_metric.metric_id();
  _header.is_status = false;
  _header.value_type = first_metric.value_type();
  _header.rrd_len = first_metric.rrd_len();
  _header.interval = first_metric.interval();

  _create_file();
  add_metric(first_metric);
}

/**
 * @brief Create a new retention file for a status and write its first point.
 *
 * The file is named <index_id>.ret and created in rrd_data_dir. Status RRD
 * files are always GAUGE.
 *
 * @param rrd_data_dir Directory where the file is created.
 * @param first_status First status point, also written in the file.
 * @throw msg_fmt if the file already exists or can't be written.
 */
retention_file::retention_file(const std::filesystem::path& rrd_data_dir,
                               const Status& first_status)
    : _last_activity(time(nullptr)),
      _nb_samples(0),
      _first_ts(std::numeric_limits<uint64_t>::max()),
      _last_ts(0) {
  _file_path = rrd_data_dir / fmt::format("{}.ret", first_status.index_id());
  _header.id = first_status.index_id();
  _header.is_status = true;
  _header.value_type = Metric::ValueType::Metric_ValueType_GAUGE;
  _header.rrd_len = first_status.rrd_len();
  _header.interval = first_status.interval();
  _create_file();
  add_status(first_status);
}

/**
 * @brief Open an existing retention file and load its header.
 *
 * The number of samples is deduced from the file size, first_ts and last_ts
 * are computed from the samples. If the header can't be read, the file is
 * removed and an exception is thrown.
 *
 * @param file_path Path of the retention file.
 */
retention_file::retention_file(const std::filesystem::path& file_path)
    : _file_path(file_path),
      _last_activity(time(nullptr)),
      _nb_samples(0),
      _first_ts(std::numeric_limits<uint64_t>::max()),
      _last_ts(0) {
  int fd = ::open(_file_path.c_str(), O_RDONLY);
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
    ::remove(_file_path.c_str());
    throw exceptions::msg_fmt("file {} is too small: {} bytes",
                              _file_path.string(), file_size);
  }

  void* addr = ::mmap(nullptr, file_size, PROT_READ, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    throw exceptions::msg_fmt("fail to map file {}: {}", _file_path.string(),
                              strerror(errno));
  }
  absl::Cleanup unmapper = [addr, file_size]() { ::munmap(addr, file_size); };
  const file_content* data = static_cast<const file_content*>(addr);
  memcpy(&_header, data, sizeof(header));
  _nb_samples = (file_size - sizeof(header)) / sizeof(sample);
  const sample* end = data->data + _nb_samples;
  for (const sample* begin = data->data; begin < end; ++begin) {
    if (begin->ts < _first_ts) {
      _first_ts = begin->ts;
    }
    if (begin->ts > _last_ts) {
      _last_ts = begin->ts;
    }
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

  absl::Cleanup closer = [fd] { ::close(fd); };
  // clean file by rounding pos
  struct stat stat_file;
  if (fstat(fd, &stat_file)) {
    throw exceptions::msg_fmt("fail to get size of  {}: {}",
                              _file_path.string(), strerror(errno));
  }

  _nb_samples = (stat_file.st_size - sizeof(_header)) / sizeof(sample);

  if (pwrite(fd, &to_append, sizeof(sample),
             sizeof(header) + _nb_samples * sizeof(sample)) != sizeof(sample)) {
    throw exceptions::msg_fmt("fail to write metric at the end of file {}: {}",
                              _file_path.string(), strerror(errno));
  }

  ++_nb_samples;
  if (to_append.ts < _first_ts) {
    _first_ts = to_append.ts;
  }
  if (to_append.ts > _last_ts) {
    _last_ts = to_append.ts;
  }

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

/**
 * @brief Get the parameters (interval, rrd_len and value_type) of the rrd
 * file built by a merge.
 *
 * The merge must not change the rrd file layout, so if @p rrd_path exists,
 * its parameters are used: the check interval or the retention of the service
 * may have changed since its creation. Otherwise, the parameters of the
 * header are used, with an interval of 60s if it is 0 as in stream.
 *
 * @param rrd_path The rrd file to merge into.
 * @param rrd_lib The lib used to read the rrd file.
 *
 * @return The parameters, the interval being in the step field.
 */
rrd_existing_data retention_file::_get_rrd_params(
    const std::filesystem::path& rrd_path,
    lib& rrd_lib) const {
  std::error_code ec;
  if (std::filesystem::exists(rrd_path, ec)) {
    rrd_existing_data params = rrd_lib.info(rrd_path);
    if (params.step) {
      if (params.step != _header.interval ||
          params.rrd_len != _header.rrd_len ||
          params.value_type != _header.value_type)
        SPDLOG_LOGGER_INFO(
            log_v2::instance().get(log_v2::RRD),
            "{}: rrd file {} has interval {}, rrd_len {} and value_type {} "
            "instead of {}, {} and {}, rrd file ones are kept",
            _file_path.string(), rrd_path.string(), params.step, params.rrd_len,
            params.value_type, _header.interval, _header.rrd_len,
            static_cast<int>(_header.value_type));
      return params;
    }
  }
  rrd_existing_data params;
  params.step = _header.interval ? _header.interval : 60;
  params.rrd_len = _header.rrd_len;
  params.value_type = _header.value_type;
  return params;
}

/**
 * @brief Sort the samples of the file by timestamp (the sort is written in
 * the file) and write them in a new rrd file.
 *
 * Status samples equal to UNDEF_STATUS are written as unknown values.
 *
 * @param dst_path The rrd file to create, overwritten if it exists.
 * @param rrd_lib The lib used to create the rrd file.
 * @param rrd_params Interval (step field), rrd_len and value_type of the rrd
 * file to create, given by _get_rrd_params().
 */
void retention_file::_to_rrd_file(const std::filesystem::path& dst_path,
                                  lib& rrd_lib,
                                  const rrd_existing_data& rrd_params) {
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
    absl::Cleanup unmapper = [addr, file_size]() { ::munmap(addr, file_size); };
    file_content* data = static_cast<file_content*>(addr);
    sample* begin = data->data;
    sample* end = begin + (file_size - sizeof(header)) / sizeof(sample);
    std::sort(begin, end, [](const sample& left, const sample& right) {
      return left.ts < right.ts;
    });
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
      // same format as stream::append_metric_to_rrd(), librrd only accepts
      // integers for COUNTER, DERIVE and ABSOLUTE
      for (; begin != end; ++begin) {
        switch (rrd_params.value_type) {
          case Metric_ValueType_COUNTER:
          case Metric_ValueType_ABSOLUTE:
            samples.emplace_back(fmt::format(
                "{}:{}", begin->ts, static_cast<uint64_t>(begin->val.d_value)));
            break;
          case Metric_ValueType_DERIVE:
            samples.emplace_back(fmt::format(
                "{}:{}", begin->ts, static_cast<int64_t>(begin->val.d_value)));
            break;
          default:
            samples.emplace_back(
                fmt::format("{}:{:f}", begin->ts, begin->val.d_value));
            break;
        }
      }
    }
  }
  // creator sets the last update of the file to from - 1: the file starts one
  // interval before the first sample so that its row is fully known
  rrd_lib.merge_create_temp(dst_path, rrd_params.rrd_len,
                            _first_ts - rrd_params.step + 1, rrd_params.step,
                            rrd_params.value_type, samples);
}

/**
 * @brief Merge the samples of this file into the rrd file @p rrd_path.
 *
 * The samples are written in a temporary rrd file (<rrd_path>.to_merge), then
 * a new rrd file (<rrd_path>.tmp) is created from the data of @p rrd_path and
 * of this temporary file and is renamed to @p rrd_path. If @p rrd_path
 * doesn't exist, it is created from the samples only. Temporary files are
 * always removed. The retention file itself is not modified.
 *
 * The interval, rrd_len and value_type of @p rrd_path are kept, see
 * _get_rrd_params().
 *
 * abort_merge() can be called from another thread, the merge then stops at
 * the next step and @p rrd_path is left untouched.
 *
 * @param rrd_path The rrd file to update.
 * @param rrd_lib The lib used to create the rrd files.
 */
void retention_file::merge(const std::filesystem::path& rrd_path,
                           lib& rrd_lib) {
  // the merged file must accept data after the last one of both files
  time_t last_ts = rrd_lib.last_update(rrd_path);
  if (last_ts <= 0 || (uint64_t)last_ts < _last_ts) {
    last_ts = _last_ts;
  }
  const rrd_existing_data rrd_params = _get_rrd_params(rrd_path, rrd_lib);
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
    _to_rrd_file(to_merge_path, rrd_lib, rrd_params);
  }
  if (!_abort_merge) {
    // creator sets the last update of the file to from - 1
    rrd_lib.merge_files(tmp_merge_path, rrd_path, to_merge_path,
                        rrd_params.rrd_len, last_ts + 1, rrd_params.step,
                        rrd_params.value_type);
  }
  if (!_abort_merge) {
    if (::rename(tmp_merge_path.c_str(), rrd_path.c_str())) {
      throw exceptions::msg_fmt("fail to rename {} to {}: {}",
                                tmp_merge_path.string(), rrd_path.string(),
                                strerror(errno));
    }
  }
}

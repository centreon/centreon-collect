/**
 * Copyright 2011-2013, 2021-2024 Centreon
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

#ifndef CCB_RRD_BACKEND_HH
#define CCB_RRD_BACKEND_HH

#include "common/log_v2/log_v2.hh"

namespace com::centreon::broker {

using log_v2 = com::centreon::common::log_v2::log_v2;

namespace rrd {
/**
 *  @class backend backend.hh "com/centreon/broker/rrd/backend.hh"
 *  @brief Generic access to RRD files.
 *
 *  Provide a unified access to RRD files. Files can be accessed
 *  either through librrd or with rrdcached.
 *
 *  @see rrd::lib
 *  @see rrd::cached
 */
/**
 * @brief Metadata and existing data-points read from an RRD file.
 *
 * Returned by backend::fetch_existing().  The @c points vector contains only
 * non-NaN (known) values, sorted by ascending timestamp.
 */
struct rrd_existing_data {
  uint32_t step = 0;        ///< Time step in seconds (0 = unknown/error)
  uint32_t rrd_len = 0;     ///< Total retention in seconds
  short value_type = 0;     ///< 0=GAUGE, 1=COUNTER, 2=DERIVE, 3=ABSOLUTE
  /// Known data points: (unix_timestamp, value), sorted ascending.
  std::vector<std::pair<uint64_t, double>> points;
};

class backend {
 protected:
  std::shared_ptr<spdlog::logger> _logger;

 public:
  backend() : _logger{log_v2::instance().get(log_v2::RRD)} {}
  backend(backend const& b) = delete;
  virtual ~backend() noexcept = default;
  backend& operator=(backend const& b) = delete;
  virtual void begin() = 0;
  virtual void clean() = 0;
  virtual void close() = 0;
  virtual void commit() = 0;
  virtual void open(const std::filesystem::path& filename) = 0;
  virtual void open(const std::filesystem::path& filename,
                    uint32_t length,
                    time_t from,
                    uint32_t step,
                    short value_type = 0,
                    bool without_cache = false) = 0;
  virtual void remove(const std::filesystem::path& filename) = 0;
  virtual void update(time_t t, std::string const& value) = 0;
  virtual void update(const std::deque<std::string>& pts) = 0;

  /**
   * @brief Flush pending rrdcached writes for @p filename to disk before a
   *        merge-read.  No-op for the lib backend.
   */
  virtual void pre_merge_flush(const std::filesystem::path& /*filename*/) {}

  /**
   * @brief Read file metadata (step, rrd_len, value_type) and all non-NaN
   *        data points in [@p from_ts, @p to_ts] from an RRD file.
   *
   * Uses librrd directly (bypasses rrdcached).  Caller must call
   * pre_merge_flush() first when using the cached backend.
   *
   * @return rrd_existing_data with step == 0 on failure.
   */
  virtual rrd_existing_data fetch_existing(const std::filesystem::path& filename,
                                           uint64_t from_ts,
                                           uint64_t to_ts) = 0;

  /**
   * @brief Create a temporary RRD file and write @p batch into it using
   *        librrd directly (bypasses rrdcached socket).
   *
   * Used during merge to produce the new file before an atomic rename.
   * The caller is responsible for the subsequent rename and
   * post_merge_forget().
   */
  virtual void merge_create_temp(const std::filesystem::path& tmp_path,
                                 uint32_t rrd_len,
                                 time_t from,
                                 uint32_t step,
                                 short value_type,
                                 const std::deque<std::string>& batch) = 0;

  /**
   * @brief Invalidate rrdcached's in-memory queue for @p filename after an
   *        atomic rename.  No-op for the lib backend.
   */
  virtual void post_merge_forget(const std::filesystem::path& /*filename*/) {}
};
}  // namespace rrd

}  // namespace com::centreon::broker

#endif  // !CCB_RRD_BACKEND_HH

/**
 * Copyright 2011-2013 Centreon
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

#ifndef CCB_RRD_STREAM_HH
#define CCB_RRD_STREAM_HH

#include "bbdo/rebuild_message.pb.h"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/rrd/backend.hh"
#include "com/centreon/broker/rrd/cached.hh"
#include "com/centreon/broker/rrd/lib.hh"
#include "com/centreon/broker/rrd/retention_manager.hh"

namespace com::centreon::broker {

namespace rrd {
/**
 *  @class stream stream.hh "com/centreon/broker/rrd/stream.hh"
 *  @brief RRD stream class.
 *
 *  Write RRD files.
 */
template <typename T>
class stream : public io::stream {
  using rebuild_cache =
      std::unordered_map<std::string, std::list<std::shared_ptr<io::data>>>;

  using rebuild_metric_to_index =
      boost::container::flat_map<uint64_t, uint64_t>;

  bool _ignore_update_errors;
  std::filesystem::path _metrics_path;
  rebuild_cache _metrics_rebuild;
  rebuild_metric_to_index _metrics_to_index_rebuild;
  std::filesystem::path _status_path;
  rebuild_cache _status_rebuild;
  const bool _write_metrics;
  const bool _write_status;
  T _backend;

  /* Retention buffer */
  retention_manager _retention;

  /**
   * @brief Earliest current-data timestamp seen per metric/status since this
   *        stream session started.
   *
   * Set in write() when a data point is routed to the RRD backend (i.e. its
   * timestamp is recent enough: t >= now - step).  Used by Step 3 (junction
   * detection) to determine when the buffered backfill data has caught up with
   * the live stream, at which point a merge can be triggered.
   *
   * Cleared on successful merge (_do_metric/status_merge) and on graph removal.
   */
  absl::flat_hash_map<uint64_t, uint64_t> _metric_earliest_current;
  absl::flat_hash_map<uint64_t, uint64_t> _status_earliest_current;

  /* Loggers */
  std::shared_ptr<spdlog::logger> _logger;

  void _rebuild_data(const RebuildMessage& rm);
  void _do_metric_merge(uint64_t metric_id, const std::string& rrd_path);
  void _do_status_merge(uint64_t index_id, const std::string& rrd_path);

 public:
  stream(std::filesystem::path metrics_path,
         std::filesystem::path status_path,
         uint32_t cache_size,
         bool ignore_update_errors,
         retention_config retention_cfg = {},
         bool write_metrics = true,
         bool write_status = true);
  stream(std::filesystem::path metrics_path,
         std::filesystem::path status_path,
         uint32_t cache_size,
         bool ignore_update_errors,
         std::string const& local,
         retention_config retention_cfg = {},
         bool write_metrics = true,
         bool write_status = true);
  stream(std::filesystem::path metrics_path,
         std::filesystem::path status_path,
         uint32_t cache_size,
         bool ignore_update_errors,
         unsigned short port,
         retention_config retention_cfg = {},
         bool write_metrics = true,
         bool write_status = true);
  stream(const stream&) = delete;
  stream& operator=(const stream&) = delete;
  ~stream() noexcept = default;
  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  void update() override;
  int32_t write(std::shared_ptr<io::data> const& d) override;
  int32_t stop() override { return 0; }
};

}  // namespace rrd

}  // namespace com::centreon::broker

#endif  // !CCB_RRD_STREAM_HH

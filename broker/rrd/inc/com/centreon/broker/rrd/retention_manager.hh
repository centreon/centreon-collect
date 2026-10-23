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

#ifndef CCB_RRD_RETENTION_MANAGER_HH
#define CCB_RRD_RETENTION_MANAGER_HH

#include <absl/container/btree_set.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <boost/asio/deadline_timer.hpp>
#include <memory>
#include "retention_file.hh"

namespace com::centreon::broker::rrd {

/**
 * @brief Configuration for the retention buffer.
 *
 * All fields are read from the RRD endpoint JSON configuration block.
 */
struct retention_config {
  std::filesystem::path metrics_dir;  ///< Directory for metric .prot files
  std::filesystem::path status_dir;   ///< Directory for status .prot files
  uint32_t orphan_interval = 3600;    ///< Inactivity seconds before cleanup
  /// Time window (seconds) that must elapse between two successive partial
  /// merges for the same metric/status (Step 3.3).  Default: 1 day.
  uint64_t partial_merge_interval = 86400;
  // In order to avoid to rebuild all rrd file
  // between successive partial rebuilds due to not ordered samples, we rebuild
  // until last_ts - partial_merge_before_last_sample_delay
  uint64_t partial_merge_before_last_sample_delay = 3600;
};

/**
 * @class retention_manager
 * @brief Manages in-memory + on-disk retention buffers for RRD metrics and
 *        statuses.
 *
 * Data flow:
 *   write_metric/status()  →  add point to in-memory protobuf batch
 *   combined points_size() >= max_pending_points  →  serialise batch to rotated
 * file ~retention_manager()  →  serialise remaining batch to {id}.prot
 *
 * On-disk layout (co-located with .rrd files):
 *   {metrics_dir}/{id}.prot         (graceful-shutdown flush)
 *   {metrics_dir}/{id}.{ts}.prot    (rotated, immutable)
 *   {status_dir}/{id}.prot
 *   {status_dir}/{id}.{ts}.prot
 *
 * File format: each file is exactly one serialised MetricRetentionBatch or
 * StatusRetentionBatch protobuf message.
 *
 * A merge is triggered when the rotated-file count reaches max_files.  The
 * stream calls get_metric_merge_points() / get_status_merge_points() (which
 * return on-disk + in-memory data), replays the points into the RRD backend,
 * then calls metric_merge_done() / status_merge_done() to clear everything.
 */
class retention_manager
    : public std::enable_shared_from_this<retention_manager> {
  retention_config _config;
  std::shared_ptr<boost::asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;
  boost::asio::deadline_timer _minute_timer ABSL_GUARDED_BY(_data_m);

  mutable absl::Mutex _data_m;

  using data_cont = absl::flat_hash_map<uint64_t, retention_file>;
  data_cont _metrics ABSL_GUARDED_BY(_data_m);
  data_cont _statuses ABSL_GUARDED_BY(_data_m);

  using id_to_ts = absl::flat_hash_map<uint64_t, uint64_t>;
  id_to_ts _earliest_rt_metric ABSL_GUARDED_BY(_data_m);
  id_to_ts _earliest_rt_status ABSL_GUARDED_BY(_data_m);

  bool _check_partial_merge(uint64_t metric_id_or_index_id,
                            const data_cont& state) const;

  retention_manager(const retention_config& config,
                    const std::shared_ptr<boost::asio::io_context>& io_context,
                    const std::shared_ptr<spdlog::logger>& logger);

  void _init();

  void _merge_metric(retention_file&& to_merge)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_data_m);
  void _merge_status(retention_file&& to_merge)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_data_m);

  void _check_merge_metric(uint64_t metric_id);
  void _check_merge_status(uint64_t index_id);

  void _start_minute_timer();

  void _minute_timer_handler();

 public:
  static std::shared_ptr<retention_manager> load(
      const retention_config& config,
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger);

  bool enabled() const noexcept {
    return !_config.metrics_dir.empty() || !_config.status_dir.empty();
  }

  /**
   * @brief Append a metric data point to the in-memory retention batch.
   *
   * @return true  A merge should be triggered now.
   * @return false No merge needed yet.
   */
  void write_metric(const Metric& metric);

  /**
   * @brief Append a status data point to the in-memory retention batch.
   *
   * @return true  A merge should be triggered now.
   * @return false No merge needed yet.
   */
  void write_status(const Status& status);

  void update_earliest_real_time_metric(uint64_t ts);
  void update_earliest_real_time_status(uint64_t ts);
};

}  // namespace com::centreon::broker::rrd

#endif  // CCB_RRD_RETENTION_MANAGER_HH

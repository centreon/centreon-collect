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
#include <absl/container/flat_hash_set.h>
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
class retention_manager {
  retention_config _config;
  std::shared_ptr<spdlog::logger> _logger;

  mutable absl::Mutex _data_m;

  using data_cont = absl::flat_hash_map<uint64_t, retention_file>;
  data_cont _metrics ABSL_GUARDED_BY(_data_m);

  data_cont _statuses ABSL_GUARDED_BY(_data_m);

  bool _check_partial_merge(uint64_t metric_id_or_index_id,
                            const data_cont& state) const;

 public:
  explicit retention_manager(retention_config config,
                             std::shared_ptr<spdlog::logger> logger);

  bool enabled() const noexcept {
    return !_config.metrics_dir.empty() || !_config.status_dir.empty();
  }

  /**
   * @brief Initialise the manager: scan dirs for existing buffer files.
   *
   * Must be called once before write_metric / write_status.
   */
  void init();

  void lock() const { _data_m.Lock(); }

  void unlock() const { _data_m.Unlock(); }

  /**
   * @brief Append a metric data point to the in-memory retention batch.
   *
   * @return true  A merge should be triggered now.
   * @return false No merge needed yet.
   */
  void write_metric(const Metric metric);

  /**
   * @brief Append a status data point to the in-memory retention batch.
   *
   * @return true  A merge should be triggered now.
   * @return false No merge needed yet.
   */
  void write_status(const Status status);

  /**
   * @brief Return all buffered (time, value) pairs for a metric.
   *
   * Includes both on-disk rotated files and in-memory pending batch.
   */
  retention_file::mapped_slot get_metric_merge_points(uint64_t metric_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_data_m);

  /**
   * @brief Return all buffered (time, status) pairs for a status index.
   *
   * Includes both on-disk rotated files and in-memory pending batch.
   */
  retention_file::mapped_slot get_status_merge_points(uint64_t index_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_data_m);

  /**
   * @brief Clear the retention buffer for a metric after a successful merge.
   */
  void metric_merge_done(uint64_t metric_id, uint64_t last_ts_merged);
  void status_merge_done(uint64_t index_id, uint64_t last_ts_merged);

  /**
   * @brief Remove all retention files for a metric (on remove_graph event).
   */
  void remove_metric(uint64_t metric_id);
  void remove_status(uint64_t index_id);

  /**
   * @brief Return the last buffered timestamp for a metric/status (0 if none).
   *
   * Used by the stream write path to detect gaps between consecutive old-data
   * points (Step 3.2).
   */
  uint64_t last_metric_time(uint64_t metric_id);
  uint64_t last_status_time(uint64_t index_id);

  /**
   * @brief Check whether a partial merge should be triggered.
   *
   * Returns true when the buffer has accumulated at least @c
   * partial_merge_interval seconds of data since the previous (partial) merge:
   *   last_retention_time - last_partial_merge_ts >= partial_merge_interval
   *
   * Returns false if the state is not yet initialised (@c last_partial_merge_ts
   * == 0) to avoid a spurious trigger on the very first write.
   */
  inline bool check_metric_partial_merge(uint64_t metric_id) const {
    absl::ReaderMutexLock lk(&_data_m);
    return _check_partial_merge(metric_id, _metrics);
  }

  inline bool check_status_partial_merge(uint64_t index_id) const {
    absl::ReaderMutexLock lk(&_data_m);
    return _check_partial_merge(index_id, _statuses);
  }

  /**
   * @brief Check whether the junction condition is met for a metric.
   *
   * The junction is reached when the last buffered point is within one step of
   * the first current-data point:
   *   last_retention_time + step >= earliest_current_time
   *
   * Called from the current-data write path after updating
   * earliest_current_time, so that a merge can be triggered as soon as the
   * buffer catches up.
   *
   * @param metric_id              Metric identifier.
   * @param earliest_current_time  Earliest current-data timestamp seen for this
   *                               metric (0 = unknown → returns false).
   * @return true  Junction reached → merge should be triggered.
   */
  bool check_metric_junction(uint64_t metric_id,
                             uint64_t earliest_current_time);

  /**
   * @brief Same as check_metric_junction() for a status index.
   */
  bool check_status_junction(uint64_t index_id, uint64_t earliest_current_time);

  /**
   * @brief Return the IDs of all metrics that have at least one buffered point
   *        (rotated file, current file, or pending batch).
   *
   * Used at startup to schedule merges for data recovered by init().
   */
  std::vector<uint64_t> metric_ids_with_data();

  /**
   * @brief Return the IDs of all status indices that have at least one buffered
   *        point.
   */
  std::vector<uint64_t> status_ids_with_data();

  /**
   * @brief Remove retention buffers for metrics inactive longer than
   *        orphan_interval seconds.
   *
   * @param now_seconds  Current Unix timestamp.
   */
  void cleanup_orphans(uint64_t now_seconds);
};

}  // namespace com::centreon::broker::rrd

#endif  // CCB_RRD_RETENTION_MANAGER_HH

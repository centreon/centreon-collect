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

#include <absl/container/flat_hash_map.h>
#include <filesystem>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "rrd_retention.pb.h"
#include "spdlog/logger.h"

namespace com::centreon::broker::rrd {

/**
 * @brief Configuration for the retention buffer.
 *
 * All fields are read from the RRD endpoint JSON configuration block.
 */
struct retention_config {
  std::filesystem::path metrics_dir;  ///< Directory for metric .prot files
  std::filesystem::path status_dir;   ///< Directory for status .prot files
  /// Combined point count threshold (metrics + statuses) that triggers a
  /// rotation.  Default: 144 = 12 h × 1 point/5 min.
  uint32_t max_pending_points = 144;
  uint32_t max_files = 5;           ///< Max rotated files before forced merge
  uint32_t orphan_interval = 3600;  ///< Inactivity seconds before cleanup
};

/**
 * @brief Per-metric or per-status-index retention buffer state.
 *
 * Points accumulate in the in-memory @c pending protobuf batch.  They are
 * written to disk only when the combined point count across metrics and
 * statuses reaches @c max_pending_points (rotation) or when the manager is
 * destroyed (graceful-shutdown flush).  A crash therefore loses at most one
 * batch worth of points.
 *
 * @tparam BatchT  Either MetricRetentionBatch or StatusRetentionBatch.
 */
template <typename BatchT>
struct retention_state {
  absl::Mutex mutex;
  uint32_t step = 0;                   ///< Metric step in seconds
  uint64_t last_retention_time = 0;    ///< Timestamp of last buffered point
  uint64_t last_activity_time = 0;     ///< Wall-clock time of last write
  std::filesystem::path current_path;  ///< Path for the shutdown flush
  std::vector<std::filesystem::path>
      rotated_files;   ///< Immutable on-disk files (oldest first)
  BatchT pending;      ///< In-memory batch, not yet serialised to disk
                       ///< points_size() drives the rotation threshold
};

using metric_retention_state = retention_state<MetricRetentionBatch>;
using status_retention_state = retention_state<StatusRetentionBatch>;

/**
 * @class retention_manager
 * @brief Manages in-memory + on-disk retention buffers for RRD metrics and
 *        statuses.
 *
 * Data flow:
 *   write_metric/status()  →  add point to in-memory protobuf batch
 *   combined points_size() >= max_pending_points  →  serialise batch to rotated file
 *   ~retention_manager()  →  serialise remaining batch to {id}.prot
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

  absl::Mutex _metrics_mutex;
  absl::flat_hash_map<uint64_t, std::unique_ptr<metric_retention_state>>
      _metrics ABSL_GUARDED_BY(_metrics_mutex);

  absl::Mutex _statuses_mutex;
  absl::flat_hash_map<uint64_t, std::unique_ptr<status_retention_state>>
      _statuses ABSL_GUARDED_BY(_statuses_mutex);

  // ---- internal helpers ----

  template <typename StateT>
  StateT& _get_or_create(
      absl::Mutex& map_mutex,
      absl::flat_hash_map<uint64_t, std::unique_ptr<StateT>>& map,
      uint64_t id,
      const std::filesystem::path& dir) ABSL_LOCKS_EXCLUDED(map_mutex);

  /// Serialise pending batch to a new rotated file; clears batch afterwards.
  template <typename StateT>
  void _flush_to_rotated(StateT& state)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(state.mutex);

  /// Serialise remaining pending batch to {id}.prot; used at shutdown.
  template <typename StateT>
  void _flush_pending(StateT& state)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(state.mutex);

  /// Check if pending batch has grown past max_pending_points and rotate if needed.
  template <typename StateT>
  bool _check_and_rotate(StateT& state)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(state.mutex);

  template <typename StateT>
  void _clear_merge(StateT& state)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(state.mutex);

  template <typename StateT>
  void _remove_state(StateT& state)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(state.mutex);

  static std::vector<std::pair<uint64_t, double>> _read_metric_points(
      const std::vector<std::filesystem::path>& files,
      const std::filesystem::path& current,
      std::shared_ptr<spdlog::logger> logger);
  static std::vector<std::pair<uint64_t, uint32_t>> _read_status_points(
      const std::vector<std::filesystem::path>& files,
      const std::filesystem::path& current,
      std::shared_ptr<spdlog::logger> logger);

  static std::filesystem::path _rotated_path(
      const std::filesystem::path& current,
      uint64_t rotation_ts);

  static std::filesystem::path _current_path(const std::filesystem::path& dir,
                                             uint64_t id);

 public:
  explicit retention_manager(retention_config config,
                             std::shared_ptr<spdlog::logger> logger);
  ~retention_manager();

  bool enabled() const noexcept {
    return !_config.metrics_dir.empty() || !_config.status_dir.empty();
  }

  /**
   * @brief Initialise the manager: scan dirs for existing buffer files.
   *
   * Must be called once before write_metric / write_status.
   */
  void init();

  /**
   * @brief Append a metric data point to the in-memory retention batch.
   *
   * @return true  A merge should be triggered now.
   * @return false No merge needed yet.
   */
  bool write_metric(uint64_t metric_id,
                    uint64_t time,
                    double value,
                    uint32_t step);

  /**
   * @brief Append a status data point to the in-memory retention batch.
   *
   * @return true  A merge should be triggered now.
   * @return false No merge needed yet.
   */
  bool write_status(uint64_t index_id,
                    uint64_t time,
                    uint32_t status,
                    uint32_t step);

  /**
   * @brief Return all buffered (time, value) pairs for a metric.
   *
   * Includes both on-disk rotated files and in-memory pending batch.
   */
  std::vector<std::pair<uint64_t, double>> get_metric_merge_points(
      uint64_t metric_id);

  /**
   * @brief Return all buffered (time, status) pairs for a status index.
   *
   * Includes both on-disk rotated files and in-memory pending batch.
   */
  std::vector<std::pair<uint64_t, uint32_t>> get_status_merge_points(
      uint64_t index_id);

  /**
   * @brief Clear the retention buffer for a metric after a successful merge.
   */
  void metric_merge_done(uint64_t metric_id);
  void status_merge_done(uint64_t index_id);

  /**
   * @brief Remove all retention files for a metric (on remove_graph event).
   */
  void remove_metric(uint64_t metric_id);
  void remove_status(uint64_t index_id);

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

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

#include "com/centreon/broker/rrd/retention_manager.hh"
#include <absl/synchronization/mutex.h>
#include <re2/re2.h>
#include <cstdio>
#include "com/centreon/broker/rrd/retention_file.hh"

using namespace com::centreon::broker::rrd;
namespace fs = std::filesystem;

/**
 * @brief Constructor.
 *
 * @param config Retention configuration (directories, file size limits, etc.)
 * @param logger Logger for retention-related messages (must not be null).
 */
retention_manager::retention_manager(retention_config config,
                                     std::shared_ptr<spdlog::logger> logger)
    : _config(std::move(config)), _logger(std::move(logger)) {}

/**
 * @brief Scan metrics_dir and status_dir for existing .prot buffer files and
 *        restore the in-memory state.
 *
 * Must be called once after construction, before any write_metric /
 * write_status call.  Both directories are created if they do not yet exist.
 * Files whose names do not match the expected pattern are silently ignored.
 */
void retention_manager::init() {
  auto scan_dir = [&](const fs::path& dir) {
    if (dir.empty())
      return;

    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
      SPDLOG_LOGGER_ERROR(_logger,
                          "retention: cannot create directory '{}': {}",
                          dir.string(), ec.message());
      return;
    }

    static re2::RE2 file_filter("\\d+\\.ret");

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
      if (ec || !entry.is_regular_file())
        continue;
      if (!RE2::FullMatch(entry.path().filename().string(), file_filter))
        continue;

      try {
        retention_file from_disk(entry.path());
        if (from_disk.is_status()) {
          _statuses.emplace(from_disk.id(), std::move(from_disk));
        } else
          _metrics.emplace(from_disk.id(), std::move(from_disk));
      } catch (const std::exception& e) {
      }
    };
  };

  absl::MutexLock l(&_data_m);
  scan_dir(_config.metrics_dir);
  scan_dir(_config.status_dir);
}

/**
 * @brief Append a metric data point to the in-memory retention batch for the
 * given metric ID.  If the batch reaches the configured max_pending_points,
 * it is flushed to a new rotated file and a merge is triggered if the number
 * of rotated files reaches max_files.  The state for the metric ID is created
 * if it does not already exist.  The caller should hold the _metrics_m when
 * calling this function to ensure thread safety.
 *
 * @param metric_id The identifier of the metric for which to write the data
 * point.
 * @param time The timestamp of the data point, typically in seconds since the
 * Unix epoch.
 * @param value The value of the metric data point to write.
 * @param step The step interval (in seconds) for the metric, which may be
 * used for retention policies or merge logic.  The caller is responsible for
 * providing a valid step value according to the retention configuration and
 * expected data frequency.
 *
 * @return true if a merge should be triggered now (e.g., because the number
 * of rotated files has reached the configured limit), or false if no merge is
 * needed yet.  The caller can use this return value to decide when to trigger
 * a merge of the retention data into the main RRD files.
 */
void retention_manager::write_metric(const Metric metric) {
  if (_config.metrics_dir.empty())
    return;

  absl::MutexLock l(&_data_m);
  auto exist = _metrics.find(metric.metric_id());
  try {
    if (exist == _metrics.end()) {
      _metrics.emplace(metric.metric_id(),
                       retention_file(_config.metrics_dir, metric));
    } else {
      exist->second.add_metric(metric);
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to add metric to retention: {}", e);
  }
}

/**
 * @brief Append a status data point to the in-memory retention batch for the
 * given index ID.  If the batch reaches the configured max_pending_points, it
 * is flushed to a new rotated file and a merge is triggered if the number of
 * rotated files reaches max_files.  The state for the index ID is created if
 * it does not already exist.  The caller should hold the _statuses_m when
 * calling this function to ensure thread safety.
 *
 * @param index_id The identifier of the status index for which to write the
 * data point.
 * @param time The timestamp of the data point, typically in seconds since the
 * Unix epoch.
 * @param status The value of the status data point to write, typically an
 * integer representing the status (e.g., 0 for OK, 1 for WARNING, etc.).
 * @param step The step interval (in seconds) for the status, which may be
 * used for retention policies or merge logic.  The caller is responsible for
 * providing a valid step value according to the retention configuration and
 * expected data frequency.
 *
 * @return true if a merge should be triggered now (e.g., because the number
 * of rotated files has reached the configured limit), or false if no merge is
 * needed yet.  The caller can use this return value to decide when to trigger
 * a merge of the retention data into the main RRD files.
 */
void retention_manager::write_status(Status status) {
  if (_config.status_dir.empty())
    return;

  absl::MutexLock l(&_data_m);
  auto exist = _metrics.find(status.index_id());
  try {
    if (exist == _metrics.end()) {
      _metrics.emplace(status.index_id(),
                       retention_file(_config.status_dir, status));
    } else {
      exist->second.add_status(status);
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to add status to retention: {}", e);
  }
}

// ---------------------------------------------------------------------------
// Reading helpers
// ---------------------------------------------------------------------------

/**
 * @brief Get all data points for a given metric ID from the rotated files and
 * current file, and combine them with any pending points in memory.  The
 * caller should hold the _metrics_m when calling this function to ensure
 * thread safety.  The returned vector contains pairs of (time, value) for all
 * points associated with the metric ID, including those from on-disk files
 * and the in-memory batch.  This function is typically called during a merge
 * operation to retrieve all relevant data points for merging into the main
 * RRD files.
 *
 * @param metric_id The identifier of the metric for which to retrieve the
 * data points.  The function will look up the retention state for this metric
 * ID, read all points from the associated rotated files and current file, and
 * combine them with any pending points in memory before returning the
 * complete list of points.
 *
 * @return A vector of pairs, where each pair consists of a timestamp
 * (uint64_t) and a metric value (double).  This vector includes all points
 * for the specified metric ID from both on-disk files and the in-memory
 * pending batch. If the metric ID does not exist in the retention manager, an
 * empty vector is returned.
 */
retention_file::mapped_slot retention_manager::get_metric_merge_points(
    uint64_t metric_id) {
  auto it = _metrics.find(metric_id);
  if (it == _metrics.end())
    return {};

  retention_file& state = it->second;
  return state.sort_and_get_slots();
}

/**
 * @brief Get all data points for a given status index ID from the rotated
 * files and current file, and combine them with any pending points in memory.
 * The caller should hold the _statuses_m when calling this function to ensure
 * thread safety.  The returned vector contains pairs of (time, status) for
 * all points associated with the index ID, including those from on-disk files
 * and the in-memory batch.  This function is typically called during a merge
 * operation to retrieve all relevant data points for merging into the main
 * RRD files.
 *
 * @param index_id The identifier of the status index for which to retrieve
 * the data points.  The function will look up the retention state for this
 * index ID, read all points from the associated rotated files and current
 * file, and combine them with any pending points in memory before returning
 * the complete list of points.
 *
 * @return A vector of pairs, where each pair consists of a timestamp
 * (uint64_t) and a status value (uint32_t).  This vector includes all points
 * for the specified index ID from both on-disk files and the in-memory
 * pending batch. If the index ID does not exist in the retention manager, an
 * empty vector is returned.
 */
retention_file::mapped_slot retention_manager::get_status_merge_points(
    uint64_t metric_id) {
  auto it = _statuses.find(metric_id);
  if (it == _statuses.end())
    return {};

  retention_file& state = it->second;
  return state.sort_and_get_slots();
}

/**
 * @brief Clear all rotated files, the current file, and the pending batch for
 * a given metric ID after a successful merge.  This function is called after
 * the retention data for a metric has been successfully merged into the main
 * RRD files, and it resets the retention state for that metric ID by deleting
 * all associated files and clearing the in-memory batch.  The caller should
 * hold the _metrics_m when calling this function to ensure thread safety, and
 * it should also hold the mutex for the specific metric state while calling
 * this function to avoid concurrent modifications.  If the metric ID does not
 * exist in the retention manager, this function does nothing.
 *
 * @param metric_id The identifier of the metric for which to clear the
 * retention state after a merge.  The function will look up the retention
 * state for this metric ID, delete all associated rotated files and the
 * current file from disk, and clear the in-memory pending batch.  If the
 * metric ID does not exist, the function will simply return without
 * performing any actions.
 */
void retention_manager::metric_merge_done(uint64_t metric_id,
                                          uint64_t last_ts_merged) {
  absl::MutexLock lk(&_data_m);
  auto it = _metrics.find(metric_id);
  if (it == _metrics.end())
    return;
  it->second.set_last_partial_merge_ts(last_ts_merged);
}

/**
 * @brief Clear all rotated files, the current file, and the pending batch for
 * a given status index ID after a successful merge.  This function is called
 * after the retention data for a status index has been successfully merged
 * into the main RRD files, and it resets the retention state for that index
 * ID by deleting all associated files and clearing the in-memory batch.  The
 * caller should hold the _statuses_m when calling this function to ensure
 * thread safety, and it should also hold the mutex for the specific status
 * state while calling this function to avoid concurrent modifications.  If
 * the index ID does not exist in the retention manager, this function does
 * nothing.
 *
 * @param index_id The identifier of the status index for which to clear the
 * retention state after a merge.  The function will look up the retention
 * state for this index ID, delete all associated rotated files and the
 * current file from disk, and clear the in-memory pending batch.  If the
 * index ID does not exist, the function will simply return without performing
 * any actions.
 */
void retention_manager::status_merge_done(uint64_t index_id,
                                          uint64_t last_ts_merged) {
  absl::MutexLock lk(&_data_m);
  auto it = _statuses.find(index_id);
  if (it == _statuses.end())
    return;
  it->second.set_last_partial_merge_ts(last_ts_merged);
}

/**
 * @brief Remove all files and in-memory state for a given metric ID when the
 * metric is removed.  This function is called when a metric is removed from
 * the system, and it cleans up all associated retention data for that metric
 * ID by deleting all rotated files and the current file from disk, and
 * removing the in-memory state from the retention manager.  The caller should
 * hold the _metrics_m when calling this function to ensure thread safety, and
 * it should also hold the mutex for the specific metric state while calling
 * this function to avoid concurrent modifications.  If the metric ID does not
 * exist in the retention manager, this function does nothing.
 *
 * @param metric_id The identifier of the metric for which to remove all
 * retention data.  The function will look up the retention state for this
 * metric ID, delete all associated rotated files and the current file from
 * disk, clear the in-memory pending batch, and remove the state from the
 * retention manager's map.  If the metric ID does not exist, the function
 * will simply return without performing any actions.
 */
void retention_manager::remove_metric(uint64_t metric_id) {
  absl::MutexLock lk(&_data_m);
  auto it = _metrics.find(metric_id);
  if (it == _metrics.end())
    return;
  ::remove(it->second.file_path().c_str());
  _metrics.erase(it);
}

/**
 * @brief Remove all files and in-memory state for a given status index ID
 * when the status is removed.  This function is called when a status index is
 * removed from the system, and it cleans up all associated retention data for
 * that index ID by deleting all rotated files and the current file from disk,
 * and removing the in-memory state from the retention manager.  The caller
 * should hold the _statuses_m when calling this function to ensure thread
 * safety, and it should also hold the mutex for the specific status state
 * while calling this function to avoid concurrent modifications.  If the
 * index ID does not exist in the retention manager, this function does
 * nothing.
 *
 * @param index_id The identifier of the status index for which to remove all
 * retention data.  The function will look up the retention state for this
 * index ID, delete all associated rotated files and the current file from
 * disk, clear the in-memory pending batch, and remove the state from the
 * retention manager's map.  If the index ID does not exist, the function will
 * simply return without performing any actions.
 */
void retention_manager::remove_status(uint64_t index_id) {
  absl::MutexLock lk(&_data_m);
  auto it = _statuses.find(index_id);
  if (it == _statuses.end())
    return;
  ::remove(it->second.file_path().c_str());
  _statuses.erase(it);
}

/**
 * @brief Scan the retention state maps for metrics and statuses to identify
 * any "orphan" entries that have been inactive for longer than the configured
 * orphan_interval.  For each orphan entry found, all associated files
 * (rotated and current) are deleted from disk, the in-memory pending batch is
 * cleared, and the state is removed from the retention manager's map.  This
 * function is typically called periodically (e.g., by a timer) to clean up
 * retention data for metrics and statuses that are no longer active and have
 * not had any recent retention activity.  The now_seconds parameter is used
 * to determine the inactivity duration for each entry by comparing it to the
 * last_activity_time stored in the retention state.  The caller should ensure
 * that this function is called with appropriate synchronization to avoid
 * concurrent modifications to the retention state maps while it is running.
 *
 * @param now_seconds The current time in seconds since the Unix epoch, used
 * to calculate the inactivity duration for each retention state entry.
 * Entries that have a last_activity_time of 0 or have been inactive for less
 * than the configured orphan_interval will not be considered orphans and will
 * not be cleaned up.  Only entries that have been inactive for at least
 * orphan_interval seconds will be removed as orphans.
 */

/**
 * @brief Check whether the junction condition is met for a metric.
 *
 * Acquires a shared lock on the map, then a shared lock on the per-metric
 * state, and evaluates:
 *   last_retention_time + step >= earliest_current_time
 *
 * Both locks are shared so this method has negligible contention with the
 * write path.
 */
bool retention_manager::check_metric_junction(uint64_t metric_id,
                                              uint64_t earliest_current_time) {
  if (earliest_current_time == 0)
    return false;
  absl::ReaderMutexLock map_lk(&_data_m);
  auto it = _metrics.find(metric_id);
  if (it == _metrics.end())
    return false;
  return it->second.last_ts() + it->second.interval() >= earliest_current_time;
}

/**
 * @brief Check whether the junction condition is met for a status index.
 */
bool retention_manager::check_status_junction(uint64_t index_id,
                                              uint64_t earliest_current_time) {
  if (earliest_current_time == 0)
    return false;
  absl::ReaderMutexLock map_lk(&_data_m);
  auto it = _statuses.find(index_id);
  if (it == _statuses.end())
    return false;
  return it->second.last_ts() + it->second.interval() >= earliest_current_time;
}

std::vector<uint64_t> retention_manager::metric_ids_with_data() {
  absl::ReaderMutexLock lk(&_data_m);
  std::vector<uint64_t> result;
  result.reserve(_metrics.size());
  for (const auto& metric : _metrics) {
    result.push_back(metric.first);
  }
  return result;
}

std::vector<uint64_t> retention_manager::status_ids_with_data() {
  absl::ReaderMutexLock lk(&_data_m);
  std::vector<uint64_t> result;
  result.reserve(_statuses.size());
  for (const auto& status : _statuses) {
    result.push_back(status.first);
  }
  return result;
}

void retention_manager::cleanup_orphans(uint64_t now_seconds) {
  auto cleanup_map = [&](auto& map, const char* kind) {
    absl::erase_if(map, [&](auto& kv) {
      auto& [id, ret] = kv;
      if (now_seconds - ret.last_activity_time() < _config.orphan_interval)
        return false;
      SPDLOG_LOGGER_INFO(_logger,
                         "retention: cleaning up orphan {} id={} "
                         "(inactive for {}s)",
                         kind, id, now_seconds - ret.last_activity_time());
      ::remove(ret.file_path().c_str());
      return true;
    });
  };

  absl::MutexLock l(&_data_m);
  cleanup_map(_metrics, "metric");
  cleanup_map(_statuses, "status");
}

// ---------------------------------------------------------------------------
// Step 3.2 / 3.3 helpers
// ---------------------------------------------------------------------------

/**
 * @brief Return the @c last_retention_time for a metric (0 if unknown).
 *
 * Called from the stream write path to read the previous timestamp before
 * appending a new point, enabling gap detection (Step 3.2).
 */
uint64_t retention_manager::last_metric_time(uint64_t metric_id) {
  absl::ReaderMutexLock lk(&_data_m);
  auto it = _metrics.find(metric_id);
  if (it == _metrics.end())
    return 0;
  return it->second.last_ts();
}

/**
 * @brief Return the @c last_retention_time for a status index (0 if unknown).
 */
uint64_t retention_manager::last_status_time(uint64_t index_id) {
  absl::ReaderMutexLock lk(&_data_m);
  auto it = _statuses.find(index_id);
  if (it == _statuses.end())
    return 0;
  return it->second.last_ts();
}

/**
 * @brief Check if a partial merge should be triggered for a metric or a
 * status index (Step 3.3).
 *
 * Shared implementation behind check_metric_partial_merge() and
 * check_status_partial_merge(). Returns true when the buffer has accumulated
 * at least @c partial_merge_interval seconds of data since the previous
 * (partial) merge: last_retention_time - last_partial_merge_ts >=
 * partial_merge_interval
 *
 * Returns false when:
 *   - the id is unknown to @a state,
 *   - @c last_partial_merge_ts or @c last_retention_time is still 0, i.e.
 *     nothing has been merged or buffered yet — this avoids a spurious
 * trigger on the very first write,
 *   - @c last_retention_time is older than @c last_partial_merge_ts, which
 *     guards the unsigned subtraction against wrapping.
 *
 * @tparam StateT Map type holding the retention states (@c _metrics or
 *                @c _statuses).
 *
 * @param metric_id_or_index_id Metric id or status index id to look up.
 * @param state The matching state map. The caller must already hold its mutex
 *              (@c _metrics_m / @c _statuses_m), at least in read mode; the
 *              per-state mutex is taken here.
 *
 * @return true if a partial merge should be triggered now.
 */
bool retention_manager::_check_partial_merge(uint64_t metric_id_or_index_id,
                                             const data_cont& state) const {
  auto it = state.find(metric_id_or_index_id);
  if (it == state.end())
    return false;
  const retention_file& s = it->second;
  return (s.last_ts() > s.last_partial_merge_ts()) &&
         ((s.last_ts() - s.last_partial_merge_ts()) >
          _config.partial_merge_interval +
              _config.partial_merge_before_last_sample_delay);
}

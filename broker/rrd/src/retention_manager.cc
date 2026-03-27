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

using namespace com::centreon::broker::rrd;
namespace fs = std::filesystem;

/**
 * @brief Generate the current file path for a given metric/status ID in the
 * specified directory.
 *
 * @param dir The base directory where the .prot files are stored (e.g.,
 * "/var/lib/rrd/metrics").
 * @param id The metric or status ID for which to generate the file path (e.g.,
 * 42).
 *
 * @return The full file path for the current .prot file (e.g.,
 * "/var/lib/rrd/metrics/42.prot").
 */
fs::path retention_manager::_current_path(const fs::path& dir, uint64_t id) {
  return dir / fmt::format("{}.prot", id);
}

/**
 * @brief Generate the rotated file path for a given current file and rotation
 * timestamp.
 *
 * @param current The current file path (e.g., "42.prot").
 * @param rotation_ts The timestamp to insert into the rotated file name (e.g.,
 * 1735000000).
 *
 * @return The rotated file path (e.g., "42.1735000000.prot").
 */
fs::path retention_manager::_rotated_path(const fs::path& current,
                                          uint64_t rotation_ts) {
  // Insert the timestamp before ".prot": 42.prot → 42.1735000000.prot
  std::string s = current.string();
  const auto pos = s.rfind(".prot");
  if (pos != std::string::npos)
    s.insert(pos, fmt::format(".{}", rotation_ts));
  return fs::path(s);
}

/**
 * @brief Constructor.
 *
 * @param config Retention configuration (directories, file size limits, etc.)
 * @param logger Logger for retention-related messages (must not be null).
 */
retention_manager::retention_manager(retention_config config,
                                     std::shared_ptr<spdlog::logger> logger)
    : _config(std::move(config)), _logger(std::move(logger)) {}

retention_manager::~retention_manager() {
  // Flush in-memory pending batches to disk for graceful shutdown.
  auto flush_map = [this](absl::Mutex& map_mutex, auto& map)
                       ABSL_NO_THREAD_SAFETY_ANALYSIS {
                         absl::MutexLock lk(&map_mutex);
                         for (auto& [id, state] : map) {
                           absl::MutexLock slk(&state->mutex);
                           _flush_pending(*state);
                         }
                       };
  flush_map(_metrics_m, _metrics);
  flush_map(_statuses_m, _statuses);
}

/**
 * @brief Scan metrics_dir and status_dir for existing .prot buffer files and
 *        restore the in-memory state.
 *
 * Must be called once after construction, before any write_metric /
 * write_status call.  Both directories are created if they do not yet exist.
 * Files whose names do not match the expected pattern are silently ignored.
 */
void retention_manager::init() {
  struct file_info {
    bool is_rotated;
    uint64_t ts;
    fs::path path;
  };

  /**
   * @brief Parse a filename to extract the metric/status ID, rotation timestamp
   * (if rotated), and whether it's a rotated file. The expected filename
   * patterns are:
   * - Current file: "{id}.prot" (e.g., "42.prot")
   * - Rotated file: "{id}.{ts}.prot" (e.g., "42.1735000000.prot")
   *
   * @param name The filename to parse (not the full path, just the filename
   * component).
   *
   */
  auto parse_name = [](const std::string& name)
      -> std::optional<std::tuple<uint64_t, uint64_t, bool>> {
    static constexpr std::string_view suffix = ".prot";
    if (!absl::EndsWith(name, suffix))
      return std::nullopt;
    const std::string_view stem(name.data(), name.size() - suffix.size());
    const auto dot = stem.rfind('.');
    uint64_t id = 0, ts = 0;
    if (dot == std::string_view::npos) {
      if (!absl::SimpleAtoi(stem, &id))
        return std::nullopt;
      return std::make_tuple(id, 0ull, false);
    }
    if (!absl::SimpleAtoi(stem.substr(0, dot), &id) ||
        !absl::SimpleAtoi(stem.substr(dot + 1), &ts))
      return std::nullopt;
    return std::make_tuple(id, ts, true);
  };

  // Helper: scan one directory, build a sorted per-ID list of file_info, then
  // populate rotated_files for each recovered state.  Written as a lambda only
  // to avoid duplicating the parse_name/directory-scan boilerplate; the actual
  // map access is done via the typed _get_or_create_metric/_status methods so
  // the thread-safety analyser sees the right mutex.
  auto scan_dir = [&](const fs::path& dir, const char* kind,
                      auto get_or_create_fn) ABSL_NO_THREAD_SAFETY_ANALYSIS {
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

    absl::btree_map<uint64_t, std::vector<file_info>> found;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
      if (ec || !entry.is_regular_file())
        continue;
      auto parsed = parse_name(entry.path().filename().string());
      if (!parsed)
        continue;
      auto [id, ts, is_rotated] = *parsed;
      found[id].push_back({is_rotated, ts, entry.path()});
    }

    for (auto& [id, files] : found) {
      std::sort(files.begin(), files.end(),
                [](const file_info& a, const file_info& b) {
                  if (!a.is_rotated && b.is_rotated)
                    return false;
                  if (a.is_rotated && !b.is_rotated)
                    return true;
                  return a.ts < b.ts;
                });

      auto& state = get_or_create_fn(id);

      absl::MutexLock lk(&state.mutex);
      for (const auto& fi : files) {
        if (fi.is_rotated)
          state.rotated_files.push_back(fi.path);
        // non-rotated: current_path is already set by the constructor
      }
      SPDLOG_LOGGER_DEBUG(_logger,
                          "retention: recovered {} rotated files for {} id={}",
                          state.rotated_files.size(), kind, id);
    }
  };

  scan_dir(_config.metrics_dir, "metric",
           [this](uint64_t id) -> metric_retention_state& {
             return _get_or_create_metric(id);
           });
  scan_dir(_config.status_dir, "status",
           [this](uint64_t id) -> status_retention_state& {
             return _get_or_create_status(id);
           });
}

metric_retention_state& retention_manager::_get_or_create_metric(uint64_t id) {
  absl::MutexLock lk(&_metrics_m);
  auto it = _metrics.find(id);
  if (it == _metrics.end()) {
    it = _metrics
             .emplace(id, std::make_unique<metric_retention_state>(
                              _current_path(_config.metrics_dir, id)))
             .first;
  }
  return *it->second;
}

status_retention_state& retention_manager::_get_or_create_status(uint64_t id) {
  absl::MutexLock lk(&_statuses_m);
  auto it = _statuses.find(id);
  if (it == _statuses.end()) {
    it = _statuses
             .emplace(id, std::make_unique<status_retention_state>(
                              _current_path(_config.status_dir, id)))
             .first;
  }
  return *it->second;
}

/**
 * @brief Serialise the pending batch to a new immutable rotated file.
 *
 * Picks a unique filename by incrementing the timestamp suffix until no
 * collision is found on disk.  Clears the batch on success.
 */
template <typename StateT>
void retention_manager::_flush_to_rotated(StateT& state) {
  if (state.pending.points().empty())
    return;

  uint64_t ts = static_cast<uint64_t>(std::time(nullptr));
  fs::path rotated;
  std::error_code exists_ec;
  do {
    rotated = _rotated_path(state.current_path, ts++);
  } while (fs::exists(rotated, exists_ec) && !exists_ec);

  std::string data;
  state.pending.SerializeToString(&data);

  // "wxe" = O_WRONLY|O_CREAT|O_EXCL + O_CLOEXEC (Linux/glibc)
  std::FILE* f = std::fopen(rotated.c_str(), "wxe");
  if (!f) {
    SPDLOG_LOGGER_ERROR(_logger,
                        "retention: cannot create rotated file '{}': {}",
                        rotated.string(), strerror(errno));
    return;
  }
  std::fwrite(data.data(), 1, data.size(), f);
  std::fclose(f);

  state.rotated_files.push_back(rotated);
  state.pending.Clear();

  SPDLOG_LOGGER_DEBUG(_logger, "retention: flushed batch to '{}' ({} rotated)",
                      rotated.string(), state.rotated_files.size());
}

/**
 * @brief Serialise remaining pending batch to the current (non-rotated) file.
 *
 * Called at graceful shutdown so that data survives a restart.
 */
template <typename StateT>
void retention_manager::_flush_pending(StateT& state) {
  if (state.pending.points().empty())
    return;

  std::string data;
  state.pending.SerializeToString(&data);

  // "ae" = O_WRONLY|O_CREAT|O_APPEND + O_CLOEXEC
  std::FILE* f = std::fopen(state.current_path.c_str(), "ae");
  if (!f) {
    SPDLOG_LOGGER_ERROR(_logger, "retention: cannot flush to '{}': {}",
                        state.current_path.string(), strerror(errno));
    return;
  }
  std::fwrite(data.data(), 1, data.size(), f);
  std::fclose(f);

  state.pending.Clear();
}

/**
 * @brief Check if the pending batch has reached max_pending_points and rotate.
 *
 * The threshold is evaluated against the number of points already in the
 * batch (@c points_size()), which is an O(1) operation.  A rotation is
 * triggered when the batch reaches the configured limit so that each on-disk
 * file represents at most @c max_pending_points data points.
 *
 * @return true  Merge should be triggered (rotated-file limit reached).
 * @return false No merge needed yet.
 */
template <typename StateT>
bool retention_manager::_check_and_rotate(StateT& state) {
  if (static_cast<uint32_t>(state.pending.points_size()) <
      _config.max_pending_points)
    return false;

  _flush_to_rotated(state);
  if (state.rotated_files.size() >= _config.max_files) {
    SPDLOG_LOGGER_INFO(
        _logger, "retention: file count limit ({}) reached — merge triggered",
        _config.max_files);
    return true;
  }
  return false;
}

/**
 * @brief Clear all rotated files and the current file for a given state, and
 * clear the pending batch.  Used when a metric/status is removed or after a
 * merge completes to reset the state for the next retention cycle.
 *
 * @tparam StateT The type of the retention state (metric_retention_state or
 * status_retention_state).
 * @param state The retention state to clear, which will have all its rotated
 * files and current file deleted from disk, and its pending batch cleared.  The
 * caller should hold the state mutex when calling this function.
 */
template <typename StateT>
void retention_manager::_clear_merge(StateT& state) {
  std::error_code ec;
  for (const auto& f : state.rotated_files)
    fs::remove(f, ec);
  state.rotated_files.clear();
  if (!state.current_path.empty())
    fs::remove(state.current_path, ec);
  state.pending.Clear();
}

/**
 * @brief Remove all files and in-memory state for a given metric/status.  Used
 * when a metric/status is removed to clean up all associated data. The caller
 * should hold the state mutex when calling this function.
 *
 * @tparam StateT The type of the retention state (metric_retention_state or
 * status_retention_state).
 * @param state The retention state to remove, which will have all its files
 * deleted from disk and be removed from the in-memory map.  The caller should
 * hold the state mutex when calling this function.
 */
template <typename StateT>
void retention_manager::_remove_state(StateT& state) {
  _clear_merge(state);
}

/**
 * @brief Append a metric data point to the in-memory retention batch for the
 * given metric ID.  If the batch reaches the configured max_pending_points, it
 * is flushed to a new rotated file and a merge is triggered if the number of
 * rotated files reaches max_files.  The state for the metric ID is created if
 * it does not already exist.  The caller should hold the _metrics_m when
 * calling this function to ensure thread safety.
 *
 * @param metric_id The identifier of the metric for which to write the data
 * point.
 * @param time The timestamp of the data point, typically in seconds since the
 * Unix epoch.
 * @param value The value of the metric data point to write.
 * @param step The step interval (in seconds) for the metric, which may be used
 * for retention policies or merge logic.  The caller is responsible for
 * providing a valid step value according to the retention configuration and
 * expected data frequency.
 *
 * @return true if a merge should be triggered now (e.g., because the number of
 * rotated files has reached the configured limit), or false if no merge is
 * needed yet.  The caller can use this return value to decide when to trigger a
 * merge of the retention data into the main RRD files.
 */
bool retention_manager::write_metric(uint64_t metric_id,
                                     uint64_t time,
                                     double value,
                                     uint32_t step) {
  if (_config.metrics_dir.empty())
    return false;

  metric_retention_state& state = _get_or_create_metric(metric_id);

  absl::MutexLock lk(&state.mutex);
  state.step = step;
  state.last_retention_time = time;
  state.last_activity_time = static_cast<uint64_t>(std::time(nullptr));
  // Initialise partial-merge timestamp on the very first write (Step 3.3).
  if (state.last_partial_merge_ts == 0)
    state.last_partial_merge_ts = time;

  auto* pt = state.pending.add_points();
  pt->set_time(time);
  pt->set_value(value);

  return _check_and_rotate(state);
}

/**
 * @brief Append a status data point to the in-memory retention batch for the
 * given index ID.  If the batch reaches the configured max_pending_points, it
 * is flushed to a new rotated file and a merge is triggered if the number of
 * rotated files reaches max_files.  The state for the index ID is created if it
 * does not already exist.  The caller should hold the _statuses_m when
 * calling this function to ensure thread safety.
 *
 * @param index_id The identifier of the status index for which to write the
 * data point.
 * @param time The timestamp of the data point, typically in seconds since the
 * Unix epoch.
 * @param status The value of the status data point to write, typically an
 * integer representing the status (e.g., 0 for OK, 1 for WARNING, etc.).
 * @param step The step interval (in seconds) for the status, which may be used
 * for retention policies or merge logic.  The caller is responsible for
 * providing a valid step value according to the retention configuration and
 * expected data frequency.
 *
 * @return true if a merge should be triggered now (e.g., because the number of
 * rotated files has reached the configured limit), or false if no merge is
 * needed yet.  The caller can use this return value to decide when to trigger a
 * merge of the retention data into the main RRD files.
 */
bool retention_manager::write_status(uint64_t index_id,
                                     uint64_t time,
                                     uint32_t status,
                                     uint32_t step) {
  if (_config.status_dir.empty())
    return false;

  status_retention_state& state = _get_or_create_status(index_id);

  absl::MutexLock lk(&state.mutex);
  state.step = step;
  state.last_retention_time = time;
  state.last_activity_time = static_cast<uint64_t>(std::time(nullptr));
  // Initialise partial-merge timestamp on the very first write (Step 3.3).
  if (state.last_partial_merge_ts == 0)
    state.last_partial_merge_ts = time;

  auto* pt = state.pending.add_points();
  pt->set_time(time);
  pt->set_status(status);

  return _check_and_rotate(state);
}

// ---------------------------------------------------------------------------
// Reading helpers
// ---------------------------------------------------------------------------

namespace {

/**
 * @brief Read the entire content of a file into a string.
 */
std::string read_file_content(const fs::path& p,
                              std::shared_ptr<spdlog::logger> logger) {
  std::FILE* f = std::fopen(p.c_str(), "re");
  if (!f) {
    SPDLOG_LOGGER_WARN(logger, "retention: cannot open '{}': {}", p.string(),
                       strerror(errno));
    return {};
  }
  std::fseek(f, 0, SEEK_END);
  const long size = std::ftell(f);
  std::rewind(f);
  std::string data;
  if (size > 0) {
    data.resize(static_cast<size_t>(size));
    std::fread(data.data(), 1, static_cast<size_t>(size), f);
  }
  std::fclose(f);
  return data;
}

}  // namespace

std::vector<std::pair<uint64_t, double>> retention_manager::_read_metric_points(
    const std::vector<fs::path>& files,
    const fs::path& current,
    std::shared_ptr<spdlog::logger> logger) {
  std::vector<std::pair<uint64_t, double>> result;

  auto read_file = [&](const fs::path& p) {
    const std::string data = read_file_content(p, logger);
    if (data.empty())
      return;
    MetricRetentionBatch batch;
    if (!batch.ParseFromString(data)) {
      SPDLOG_LOGGER_WARN(logger, "retention: cannot parse batch from '{}'",
                         p.string());
      return;
    }
    for (const auto& pt : batch.points())
      result.emplace_back(pt.time(), pt.value());
  };

  for (const auto& f : files)
    read_file(f);
  if (!current.empty() && fs::exists(current))
    read_file(current);
  return result;
}

std::vector<std::pair<uint64_t, uint32_t>>
retention_manager::_read_status_points(const std::vector<fs::path>& files,
                                       const fs::path& current,
                                       std::shared_ptr<spdlog::logger> logger) {
  std::vector<std::pair<uint64_t, uint32_t>> result;

  auto read_file = [&](const fs::path& p) {
    const std::string data = read_file_content(p, logger);
    if (data.empty())
      return;
    StatusRetentionBatch batch;
    if (!batch.ParseFromString(data)) {
      SPDLOG_LOGGER_WARN(logger, "retention: cannot parse batch from '{}'",
                         p.string());
      return;
    }
    for (const auto& pt : batch.points())
      result.emplace_back(pt.time(), pt.status());
  };

  for (const auto& f : files)
    read_file(f);
  if (!current.empty() && fs::exists(current))
    read_file(current);
  return result;
}

/**
 * @brief Get all data points for a given metric ID from the rotated files and
 * current file, and combine them with any pending points in memory.  The caller
 * should hold the _metrics_m when calling this function to ensure thread
 * safety.  The returned vector contains pairs of (time, value) for all points
 * associated with the metric ID, including those from on-disk files and the
 * in-memory batch.  This function is typically called during a merge operation
 * to retrieve all relevant data points for merging into the main RRD files.
 *
 * @param metric_id The identifier of the metric for which to retrieve the data
 * points.  The function will look up the retention state for this metric ID,
 * read all points from the associated rotated files and current file, and
 * combine them with any pending points in memory before returning the complete
 * list of points.
 *
 * @return A vector of pairs, where each pair consists of a timestamp (uint64_t)
 * and a metric value (double).  This vector includes all points for the
 * specified metric ID from both on-disk files and the in-memory pending batch.
 * If the metric ID does not exist in the retention manager, an empty vector is
 * returned.
 */
std::vector<std::pair<uint64_t, double>>
retention_manager::get_metric_merge_points(uint64_t metric_id) {
  absl::MutexLock lk(&_metrics_m);
  auto it = _metrics.find(metric_id);
  if (it == _metrics.end())
    return {};
  metric_retention_state& state = *it->second;
  absl::MutexLock slk(&state.mutex);
  auto result =
      _read_metric_points(state.rotated_files, state.current_path, _logger);
  for (const auto& pt : state.pending.points())
    result.emplace_back(pt.time(), pt.value());
  return result;
}

/**
 * @brief Get all data points for a given status index ID from the rotated files
 * and current file, and combine them with any pending points in memory.  The
 * caller should hold the _statuses_m when calling this function to ensure
 * thread safety.  The returned vector contains pairs of (time, status) for all
 * points associated with the index ID, including those from on-disk files and
 * the in-memory batch.  This function is typically called during a merge
 * operation to retrieve all relevant data points for merging into the main RRD
 * files.
 *
 * @param index_id The identifier of the status index for which to retrieve the
 * data points.  The function will look up the retention state for this index
 * ID, read all points from the associated rotated files and current file, and
 * combine them with any pending points in memory before returning the complete
 * list of points.
 *
 * @return A vector of pairs, where each pair consists of a timestamp (uint64_t)
 * and a status value (uint32_t).  This vector includes all points for the
 * specified index ID from both on-disk files and the in-memory pending batch.
 * If the index ID does not exist in the retention manager, an empty vector is
 * returned.
 */
std::vector<std::pair<uint64_t, uint32_t>>
retention_manager::get_status_merge_points(uint64_t index_id) {
  absl::MutexLock lk(&_statuses_m);
  auto it = _statuses.find(index_id);
  if (it == _statuses.end())
    return {};
  status_retention_state& state = *it->second;
  absl::MutexLock slk(&state.mutex);
  auto result =
      _read_status_points(state.rotated_files, state.current_path, _logger);
  for (const auto& pt : state.pending.points())
    result.emplace_back(pt.time(), pt.status());
  return result;
}

/**
 * @brief Clear all rotated files, the current file, and the pending batch for a
 * given metric ID after a successful merge.  This function is called after the
 * retention data for a metric has been successfully merged into the main RRD
 * files, and it resets the retention state for that metric ID by deleting all
 * associated files and clearing the in-memory batch.  The caller should hold
 * the _metrics_m when calling this function to ensure thread safety, and it
 * should also hold the mutex for the specific metric state while calling this
 * function to avoid concurrent modifications.  If the metric ID does not exist
 * in the retention manager, this function does nothing.
 *
 * @param metric_id The identifier of the metric for which to clear the
 * retention state after a merge.  The function will look up the retention state
 * for this metric ID, delete all associated rotated files and the current file
 * from disk, and clear the in-memory pending batch.  If the metric ID does not
 * exist, the function will simply return without performing any actions.
 */
void retention_manager::metric_merge_done(uint64_t metric_id) {
  absl::MutexLock lk(&_metrics_m);
  auto it = _metrics.find(metric_id);
  if (it == _metrics.end())
    return;
  absl::MutexLock slk(&it->second->mutex);
  // Advance the partial-merge cursor so the next write does not immediately
  // re-trigger.  last_retention_time is not cleared by _clear_merge.
  it->second->last_partial_merge_ts = it->second->last_retention_time;
  _clear_merge(*it->second);
}

/**
 * @brief Clear all rotated files, the current file, and the pending batch for a
 * given status index ID after a successful merge.  This function is called
 * after the retention data for a status index has been successfully merged into
 * the main RRD files, and it resets the retention state for that index ID by
 * deleting all associated files and clearing the in-memory batch.  The caller
 * should hold the _statuses_m when calling this function to ensure thread
 * safety, and it should also hold the mutex for the specific status state while
 * calling this function to avoid concurrent modifications.  If the index ID
 * does not exist in the retention manager, this function does nothing.
 *
 * @param index_id The identifier of the status index for which to clear the
 * retention state after a merge.  The function will look up the retention state
 * for this index ID, delete all associated rotated files and the current file
 * from disk, and clear the in-memory pending batch.  If the index ID does not
 * exist, the function will simply return without performing any actions.
 */
void retention_manager::status_merge_done(uint64_t index_id) {
  absl::MutexLock lk(&_statuses_m);
  auto it = _statuses.find(index_id);
  if (it == _statuses.end())
    return;
  absl::MutexLock slk(&it->second->mutex);
  it->second->last_partial_merge_ts = it->second->last_retention_time;
  _clear_merge(*it->second);
}

/**
 * @brief Remove all files and in-memory state for a given metric ID when the
 * metric is removed.  This function is called when a metric is removed from the
 * system, and it cleans up all associated retention data for that metric ID by
 * deleting all rotated files and the current file from disk, and removing the
 * in-memory state from the retention manager.  The caller should hold the
 * _metrics_m when calling this function to ensure thread safety, and it
 * should also hold the mutex for the specific metric state while calling this
 * function to avoid concurrent modifications.  If the metric ID does not exist
 * in the retention manager, this function does nothing.
 *
 * @param metric_id The identifier of the metric for which to remove all
 * retention data.  The function will look up the retention state for this
 * metric ID, delete all associated rotated files and the current file from
 * disk, clear the in-memory pending batch, and remove the state from the
 * retention manager's map.  If the metric ID does not exist, the function will
 * simply return without performing any actions.
 */
void retention_manager::remove_metric(uint64_t metric_id) {
  absl::MutexLock lk(&_metrics_m);
  auto it = _metrics.find(metric_id);
  if (it == _metrics.end())
    return;
  {
    absl::MutexLock slk(&it->second->mutex);
    _remove_state(*it->second);
  }
  _metrics.erase(it);
}

/**
 * @brief Remove all files and in-memory state for a given status index ID when
 * the status is removed.  This function is called when a status index is
 * removed from the system, and it cleans up all associated retention data for
 * that index ID by deleting all rotated files and the current file from disk,
 * and removing the in-memory state from the retention manager.  The caller
 * should hold the _statuses_m when calling this function to ensure thread
 * safety, and it should also hold the mutex for the specific status state while
 * calling this function to avoid concurrent modifications.  If the index ID
 * does not exist in the retention manager, this function does nothing.
 *
 * @param index_id The identifier of the status index for which to remove all
 * retention data.  The function will look up the retention state for this index
 * ID, delete all associated rotated files and the current file from disk, clear
 * the in-memory pending batch, and remove the state from the retention
 * manager's map.  If the index ID does not exist, the function will simply
 * return without performing any actions.
 */
void retention_manager::remove_status(uint64_t index_id) {
  absl::MutexLock lk(&_statuses_m);
  auto it = _statuses.find(index_id);
  if (it == _statuses.end())
    return;
  {
    absl::MutexLock slk(&it->second->mutex);
    _remove_state(*it->second);
  }
  _statuses.erase(it);
}

/**
 * @brief Scan the retention state maps for metrics and statuses to identify any
 * "orphan" entries that have been inactive for longer than the configured
 * orphan_interval.  For each orphan entry found, all associated files (rotated
 * and current) are deleted from disk, the in-memory pending batch is cleared,
 * and the state is removed from the retention manager's map.  This function is
 * typically called periodically (e.g., by a timer) to clean up retention data
 * for metrics and statuses that are no longer active and have not had any
 * recent retention activity.  The now_seconds parameter is used to determine
 * the inactivity duration for each entry by comparing it to the
 * last_activity_time stored in the retention state.  The caller should ensure
 * that this function is called with appropriate synchronization to avoid
 * concurrent modifications to the retention state maps while it is running.
 *
 * @param now_seconds The current time in seconds since the Unix epoch, used to
 * calculate the inactivity duration for each retention state entry.  Entries
 * that have a last_activity_time of 0 or have been inactive for less than the
 * configured orphan_interval will not be considered orphans and will not be
 * cleaned up.  Only entries that have been inactive for at least
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
  absl::ReaderMutexLock map_lk(&_metrics_m);
  auto it = _metrics.find(metric_id);
  if (it == _metrics.end())
    return false;
  auto& state = *it->second;
  absl::ReaderMutexLock state_lk(&state.mutex);
  return state.last_retention_time != 0 &&
         state.last_retention_time + state.step >= earliest_current_time;
}

/**
 * @brief Check whether the junction condition is met for a status index.
 */
bool retention_manager::check_status_junction(uint64_t index_id,
                                              uint64_t earliest_current_time) {
  if (earliest_current_time == 0)
    return false;
  absl::ReaderMutexLock map_lk(&_statuses_m);
  auto it = _statuses.find(index_id);
  if (it == _statuses.end())
    return false;
  auto& state = *it->second;
  absl::ReaderMutexLock state_lk(&state.mutex);
  return state.last_retention_time != 0 &&
         state.last_retention_time + state.step >= earliest_current_time;
}

std::vector<uint64_t> retention_manager::metric_ids_with_data() {
  absl::ReaderMutexLock lk(&_metrics_m);
  std::vector<uint64_t> result;
  result.reserve(_metrics.size());
  for (const auto& [id, state_ptr] : _metrics) {
    absl::ReaderMutexLock slk(&state_ptr->mutex);
    if (!state_ptr->rotated_files.empty() ||
        !state_ptr->pending.points().empty() ||
        (!state_ptr->current_path.empty() &&
         std::filesystem::exists(state_ptr->current_path)))
      result.push_back(id);
  }
  return result;
}

std::vector<uint64_t> retention_manager::status_ids_with_data() {
  absl::ReaderMutexLock lk(&_statuses_m);
  std::vector<uint64_t> result;
  result.reserve(_statuses.size());
  for (const auto& [id, state_ptr] : _statuses) {
    absl::ReaderMutexLock slk(&state_ptr->mutex);
    if (!state_ptr->rotated_files.empty() ||
        !state_ptr->pending.points().empty() ||
        (!state_ptr->current_path.empty() &&
         std::filesystem::exists(state_ptr->current_path)))
      result.push_back(id);
  }
  return result;
}

void retention_manager::cleanup_orphans(uint64_t now_seconds) {
  auto cleanup_map = [&](absl::Mutex& map_mutex, auto& map,
                         const char* kind) ABSL_NO_THREAD_SAFETY_ANALYSIS {
    absl::MutexLock lk(&map_mutex);
    absl::erase_if(map, [&](auto& kv) {
      auto& [id, state_ptr] = kv;
      absl::MutexLock slk(&state_ptr->mutex);
      if (state_ptr->last_activity_time == 0 ||
          now_seconds - state_ptr->last_activity_time < _config.orphan_interval)
        return false;
      SPDLOG_LOGGER_INFO(_logger,
                         "retention: cleaning up orphan {} id={} "
                         "(inactive for {}s)",
                         kind, id, now_seconds - state_ptr->last_activity_time);
      _remove_state(*state_ptr);
      return true;
    });
  };

  cleanup_map(_metrics_m, _metrics, "metric");
  cleanup_map(_statuses_m, _statuses, "status");
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
  absl::ReaderMutexLock lk(&_metrics_m);
  auto it = _metrics.find(metric_id);
  if (it == _metrics.end())
    return 0;
  metric_retention_state& s = *it->second;
  absl::ReaderMutexLock slk(&s.mutex);
  return s.last_retention_time;
}

/**
 * @brief Return the @c last_retention_time for a status index (0 if unknown).
 */
uint64_t retention_manager::last_status_time(uint64_t index_id) {
  absl::ReaderMutexLock lk(&_statuses_m);
  auto it = _statuses.find(index_id);
  if (it == _statuses.end())
    return 0;
  status_retention_state& s = *it->second;
  absl::ReaderMutexLock slk(&s.mutex);
  return s.last_retention_time;
}

/**
 * @brief Check if a partial merge should be triggered for a metric (Step 3.3).
 *
 * Returns true when
 *   last_retention_time - last_partial_merge_ts >= partial_merge_interval
 * and @c last_partial_merge_ts is already initialised (non-zero).
 */
bool retention_manager::check_metric_partial_merge(uint64_t metric_id) {
  absl::ReaderMutexLock lk(&_metrics_m);
  auto it = _metrics.find(metric_id);
  if (it == _metrics.end())
    return false;
  metric_retention_state& s = *it->second;
  absl::ReaderMutexLock slk(&s.mutex);
  if (s.last_partial_merge_ts == 0 || s.last_retention_time == 0)
    return false;
  return s.last_retention_time >= s.last_partial_merge_ts &&
         s.last_retention_time - s.last_partial_merge_ts >=
             _config.partial_merge_interval;
}

/**
 * @brief Check if a partial merge should be triggered for a status index.
 */
bool retention_manager::check_status_partial_merge(uint64_t index_id) {
  absl::ReaderMutexLock lk(&_statuses_m);
  auto it = _statuses.find(index_id);
  if (it == _statuses.end())
    return false;
  status_retention_state& s = *it->second;
  absl::ReaderMutexLock slk(&s.mutex);
  if (s.last_partial_merge_ts == 0 || s.last_retention_time == 0)
    return false;
  return s.last_retention_time >= s.last_partial_merge_ts &&
         s.last_retention_time - s.last_partial_merge_ts >=
             _config.partial_merge_interval;
}

template void retention_manager::_flush_to_rotated(metric_retention_state&);
template void retention_manager::_flush_to_rotated(status_retention_state&);

template void retention_manager::_flush_pending(metric_retention_state&);
template void retention_manager::_flush_pending(status_retention_state&);

template bool retention_manager::_check_and_rotate(metric_retention_state&);
template bool retention_manager::_check_and_rotate(status_retention_state&);

template void retention_manager::_clear_merge(metric_retention_state&);
template void retention_manager::_clear_merge(status_retention_state&);

template void retention_manager::_remove_state(metric_retention_state&);
template void retention_manager::_remove_state(status_retention_state&);

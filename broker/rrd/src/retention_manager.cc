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
#include <re2/re2.h>
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/rrd/internal.hh"
#include "com/centreon/broker/rrd/retention_file.hh"

using namespace com::centreon::broker::rrd;
namespace fs = std::filesystem;

/**
 * @brief Constructor, use load() to also reload the existing retention files.
 *
 * @param config Retention configuration (directories, merge thresholds).
 * @param io_context io_context on which merges are run.
 * @param logger Logger for retention-related messages (must not be null).
 */
retention_manager::retention_manager(
    const retention_config& config,
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger)
    : _config(config),
      _io_context(io_context),
      _logger(logger),
      _retentions(
          boost::make_tuple(retention_cont::nth_index<0>::type::ctor_args(),
                            retention_cont::nth_index<1>::type::ctor_args(),
                            retention_cont::nth_index<2>::type::ctor_args(),
                            retention_cont::nth_index<3>::type::ctor_args(
                                proportional_activity_extractor(
                                    config.proportional_activity_threshold),
                                std::less<time_t>()))) {}

static absl::Mutex _instance_m;
static std::shared_ptr<retention_manager> _instance
    ABSL_GUARDED_BY(_instance_m);

/**
 * @brief Get the retention_manager shared by all the rrd streams.
 *
 * On the first call, the retention_manager is created and the retention files
 * found in the metrics and status directories are reloaded. It is then kept
 * for the lifetime of the process, so that a stream opened again (reload,
 * reconnection) doesn't reload the files still used or merged by the previous
 * one.
 *
 * On the next calls, only the merge thresholds of @p config are applied (see
 * _update_config()), the directories, @p io_context and @p logger of the first
 * call are kept.
 *
 * @param config Retention configuration (directories, merge thresholds).
 * @param io_context io_context on which merges are run, used on the first
 * call only.
 * @param logger Logger for retention-related messages (must not be null), used
 * on the first call only.
 *
 * @return The shared retention_manager.
 */
std::shared_ptr<retention_manager> retention_manager::load(
    const retention_config& config,
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger) {
  absl::MutexLock l(&_instance_m);
  if (!_instance) {
    _instance = std::make_shared<retention_manager>(config, io_context, logger);
    _instance->_init();
  } else {
    _instance->_update_config(config);
  }
  return _instance;
}

/**
 * @brief Forget the shared retention_manager, the next load() creates a new
 * one and reloads the retention files. Used by the unit tests to start each
 * test with a new configuration or to simulate a restart.
 *
 * The streams and merges still using the previous retention_manager keep it
 * alive until they release it.
 */
void retention_manager::reset() {
  absl::MutexLock l(&_instance_m);
  _instance.reset();
}

/**
 * @brief Apply the merge thresholds of a new configuration, called by load()
 * each time a stream is opened.
 *
 * Only merge_samples_threshold, proportional_activity_threshold and
 * orphan_interval are updated: the directories can't change since the
 * retention files already loaded or created are in them, so they can be read
 * without lock. The thresholds are read by start_merge() under _data_m, that
 * may run on the thread of another stream.
 *
 * @param config The new retention configuration.
 */
void retention_manager::_update_config(const retention_config& config) {
  absl::MutexLock l(&_data_m);
  _config.merge_samples_threshold = config.merge_samples_threshold;
  _config.proportional_activity_threshold =
      config.proportional_activity_threshold;
  _config.orphan_interval = config.orphan_interval;
}

/**
 * @brief Scan metrics_dir and status_dir for existing {id}.ret files and
 * reload them.
 *
 * Called once by load(). Both directories are created if they do not yet
 * exist. Files whose names do not match the expected pattern are ignored,
 * files whose header can't be read are removed.
 */
void retention_manager::_init() {
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
        _retentions.emplace(std::make_shared<retention_file>(entry.path()));
      } catch (const std::exception& e) {
      }
    };
  };
  {
    absl::MutexLock l(&_data_m);
    scan_dir(_config.metrics_dir);
    scan_dir(_config.status_dir);
  }
}

/**
 * @brief Append a metric point to the retention file of the metric, the file
 * is created on the first point. Nothing is done if metrics_dir is empty.
 *
 * Errors are only logged.
 *
 * @param metric The metric point, older than the last update of its rrd file.
 */
void retention_manager::write_metric(const Metric& metric) {
  if (_config.metrics_dir.empty())
    return;
  {
    absl::MutexLock l(&_data_m);
    auto& id_index = _retentions.get<0>();
    auto exist = id_index.find(std::make_pair(metric.metric_id(), false));
    try {
      if (exist == id_index.end()) {
        _retentions.emplace(
            std::make_shared<retention_file>(_config.metrics_dir, metric));
      } else {
        id_index.modify(
            exist, [&metric, this](std::shared_ptr<retention_file>& to_update) {
              try {
                to_update->add_metric(metric);
              } catch (const std::exception& e) {
                SPDLOG_LOGGER_ERROR(_logger,
                                    "fail to add metric {} to retention: {}",
                                    metric.metric_id(), e);
              }
            });
      }
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(_logger, "fail to add metric {} to retention: {}",
                          metric.metric_id(), e);
      return;
    }
  }
}

/**
 * @brief Append a status point to the retention file of the status, the file
 * is created on the first point. Nothing is done if status_dir is empty.
 *
 * Errors are only logged.
 *
 * @param status The status point, older than the last update of its rrd file.
 */
void retention_manager::write_status(const Status& status) {
  if (_config.status_dir.empty())
    return;
  {
    absl::MutexLock l(&_data_m);
    auto& id_index = _retentions.get<0>();
    auto exist = id_index.find(std::make_pair(status.index_id(), true));
    try {
      if (exist == id_index.end()) {
        _retentions.emplace(
            std::make_shared<retention_file>(_config.status_dir, status));
      } else {
        id_index.modify(
            exist, [&status, this](std::shared_ptr<retention_file>& to_update) {
              try {
                to_update->add_status(status);
              } catch (const std::exception& e) {
                SPDLOG_LOGGER_ERROR(_logger,
                                    "fail to add status {} to retention: {}",
                                    status.index_id(), e);
              }
            });
      }
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(_logger, "fail to add status {} to retention: {}",
                          status.index_id(), e);
      return;
    }
  }
}

/**
 * @brief Remove the retention file of a metric (on rebuild or remove_graph).
 *
 * The file is removed from the disk and forgotten. If this metric is being
 * merged, it is no longer in _retentions: the caller must also call
 * abort_metric_merge().
 *
 * @param metric_id The metric identifier.
 */
void retention_manager::remove_metric(uint64_t metric_id) {
  absl::MutexLock l(&_data_m);
  auto& id_index = _retentions.get<0>();
  auto found = id_index.find(std::make_pair(metric_id, false));
  if (found == id_index.end())
    return;
  ::remove((*found)->file_path().c_str());
  id_index.erase(found);
}

/**
 * @brief Remove the retention file of a status (on rebuild or remove_graph).
 *
 * Same as remove_metric(), the caller must also call abort_status_merge().
 *
 * @param index_id The index identifier.
 */
void retention_manager::remove_status(uint64_t index_id) {
  absl::MutexLock l(&_data_m);
  auto& id_index = _retentions.get<0>();
  auto found = id_index.find(std::make_pair(index_id, true));
  if (found == id_index.end())
    return;
  ::remove((*found)->file_path().c_str());
  id_index.erase(found);
}

/**
 * @brief Mark a retention file as being merged and post its merge on
 * _io_context. The caller must remove it from _retentions.
 *
 * @param to_merge The retention file to merge.
 * @param rrd_lib The lib used to create the rrd files, it must outlive the
 * merge.
 */
void retention_manager::_merge(const std::shared_ptr<retention_file>& to_merge,
                               const std::shared_ptr<lib>& rrd_lib,
                               backend* cached_client) {
  if (cached_client) {
    auto final_path =
        to_merge->is_status() ? _config.status_dir : _config.metrics_dir;
    final_path /= fmt::format("{}.rrd", to_merge->id());
    cached_client->pre_merge_flush(final_path);
  }
  _merging = to_merge;
  boost::asio::post(*_io_context,
                    [me = shared_from_this(), to_merge = _merging, rrd_lib]() {
                      me->_do_merge(to_merge, *rrd_lib);
                    });
}

/**
 * @brief Merge a retention file into its rrd file (runs on _io_context).
 *
 * Whatever the result of the merge, the retention file is removed, the
 * manager is ready for another merge and a pb_end_metric_merge or
 * pb_end_status_merge is published so that the stream replays the events it
 * has postponed. The retention file is removed before the publication so that
 * a replayed event can create a new one.
 *
 * @param to_merge The retention file to merge.
 * @param rrd_lib The lib used to create the rrd files.
 */
void retention_manager::_do_merge(
    const std::shared_ptr<retention_file>& to_merge,
    lib& rrd_lib) {
  auto final_path =
      to_merge->is_status() ? _config.status_dir : _config.metrics_dir;
  final_path /= fmt::format("{}.rrd", to_merge->id());
  try {
    to_merge->merge(final_path, rrd_lib);
    ::remove(to_merge->file_path().c_str());
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to merge {} into {}: {}",
                        to_merge->file_path(), final_path, e.what());
    // in order to not log error on next metric received, we mark it
    to_merge->set_last_activity_to_now();
    absl::MutexLock l(&_data_m);
    _retentions.emplace(to_merge);
  }

  {
    absl::MutexLock l(&_data_m);
    _merging.reset();
  }
  multiplexing::publisher publish;
  if (to_merge->is_status()) {
    std::shared_ptr<storage::pb_end_status_merge> to_dispatch =
        std::make_unique<storage::pb_end_status_merge>();
    to_dispatch->mut_obj().set_index_id(to_merge->id());
    publish.write(to_dispatch);
  } else {
    std::shared_ptr<storage::pb_end_metric_merge> to_dispatch =
        std::make_unique<storage::pb_end_metric_merge>();
    to_dispatch->mut_obj().set_metric_id(to_merge->id());
    publish.write(to_dispatch);
  }
}

/**
 * @brief Stop the merge of a metric if it is running, its rrd file is left
 * untouched. The end of the merge is still published.
 *
 * @param metric_id The metric identifier.
 */
void retention_manager::abort_metric_merge(uint64_t metric_id) {
  absl::MutexLock l(&_data_m);
  if (_merging && !_merging->is_status() && _merging->id() == metric_id) {
    _merging->abort_merge();
  }
}

/**
 * @brief Same as abort_metric_merge() for a status.
 *
 * @param index_id The index identifier.
 */
void retention_manager::abort_status_merge(uint64_t index_id) {
  absl::MutexLock l(&_data_m);
  if (_merging && _merging->is_status() && _merging->id() == index_id) {
    _merging->abort_merge();
  }
}

/**
 * @brief Start the merge of a retention file if no merge is running.
 *
 * The file chosen is the least recently fed one if it has not been fed for
 * orphan_interval seconds, else the one with the most samples if it has more
 * than merge_samples_threshold samples. It is removed from _retentions and
 * merged on _io_context, the next points of this metric or status will go to
 * a new retention file.
 *
 * @param rrd_lib The lib used to create the rrd files, it must outlive the
 * merge.
 *
 * @return the id of the metric or status being merged and true if it is a
 * status, nothing if no merge has been started.
 */
std::optional<
    std::pair<uint64_t /* index_id or metric_id*/, bool /*is status*/>>
retention_manager::start_merge(const std::shared_ptr<lib>& rrd_lib,
                               backend* cached_client) {
  absl::MutexLock l(&_data_m);
  if (_merging || _retentions.empty()) {
    return {};
  }

  time_t now = time(nullptr);
  {
    auto& last_interval_index = _retentions.get<1>();
    auto first = last_interval_index.begin();
    auto oldest = *first;
    if (oldest->last_activity() + _config.orphan_interval < now) {
      _merge(oldest, rrd_lib, cached_client);
      last_interval_index.erase(first);
      return std::make_pair(oldest->id(), oldest->is_status());
    }
  }
  {
    auto& nb_samples_index = _retentions.get<2>();
    auto last = std::prev(nb_samples_index.end());
    auto biggest = *last;
    if (biggest->nb_samples() > _config.merge_samples_threshold) {
      _merge(biggest, rrd_lib, cached_client);
      nb_samples_index.erase(last);
      return std::make_pair(biggest->id(), biggest->is_status());
    }
  }
  {
    auto& prop_activity_index = _retentions.get<3>();
    auto first = prop_activity_index.begin();
    auto oldest = *first;
    if (oldest->last_activity() +
            oldest->interval() * _config.proportional_activity_threshold <
        (uint64_t)now) {
      _merge(oldest, rrd_lib, cached_client);
      prop_activity_index.erase(first);
      return std::make_pair(oldest->id(), oldest->is_status());
    }
  }
  return {};
}

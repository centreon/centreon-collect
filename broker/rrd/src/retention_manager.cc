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
using namespace com::centreon::broker;
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
      _retentions(_make_retention_cont(config.proportional_activity_threshold)),
      _merging_ended(0),
      _last_start_merge(0) {}

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
 * Only merge_samples_threshold, proportional_activity_threshold,
 * orphan_interval and merge_retry_delay are updated: the directories can't
 * change since the retention files already loaded or created are in them, so
 * they can be read without lock. The thresholds are read by start_merge() under
 * _data_m, that may run on the thread of another stream.
 *
 * @param config The new retention configuration.
 */
void retention_manager::_update_config(const retention_config& config) {
  absl::MutexLock l(&_data_m);
  _config.merge_samples_threshold = config.merge_samples_threshold;
  _config.orphan_interval = config.orphan_interval;
  _config.merge_retry_delay = config.merge_retry_delay;
  _config.merge_time_out = config.merge_time_out;
  _config.start_merge_interval = config.start_merge_interval;
  if (_config.proportional_activity_threshold !=
      config.proportional_activity_threshold) {
    _config.proportional_activity_threshold =
        config.proportional_activity_threshold;
    // index 3 is sorted with the threshold given to its extractor, it can't be
    // changed in place: the container is rebuilt with a new extractor
    retention_cont rebuilt(
        _make_retention_cont(config.proportional_activity_threshold));
    rebuilt.insert(_retentions.begin(), _retentions.end());
    _retentions.swap(rebuilt);
  }
}

/**
 * @brief Scan metrics_dir and status_dir for existing {id}.ret files and
 * reload them.
 *
 * Called once by load(). Both directories are created if they do not yet
 * exist. Files whose names do not match the expected pattern are ignored,
 * files that don't contain a header and at least one sample are removed (see
 * retention_file).
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
        auto recovered = std::make_shared<retention_file>(entry.path());
        SPDLOG_LOGGER_INFO(_logger,
                           "RRD: recovered retention of {} {} with {} points",
                           recovered->is_status() ? "status" : "metric",
                           recovered->id(), recovered->nb_samples());
        _retentions.emplace(std::move(recovered));
      } catch (const std::exception& e) {
        SPDLOG_LOGGER_ERROR(_logger, "RRD: fail to recover retention {}: {}",
                            entry.path().string(), e.what());
      }
    };
  };
  {
    absl::MutexLock l(&_data_m);
    scan_dir(_config.metrics_dir);
    scan_dir(_config.status_dir);
  }
}

retention_manager::retention_cont retention_manager::_make_retention_cont(
    uint32_t threshold) {
  return retention_cont(boost::make_tuple(
      retention_cont::nth_index<0>::type::ctor_args(),
      retention_cont::nth_index<1>::type::ctor_args(),
      retention_cont::nth_index<2>::type::ctor_args(),
      retention_cont::nth_index<3>::type::ctor_args(
          proportional_activity_extractor(threshold), std::less<time_t>())));
}

/**
 * @brief Append a metric point to the retention file of the metric, the file
 * is created on the first point. Nothing is done if metrics_dir is empty.
 *
 * For a COUNTER or DERIVE metric, rrd computes the rate of a point from the
 * previous raw value: when the file is created, the last point of the
 * previous merged file (see _counter_primers) is also written in it. Its row
 * is already in the rrd file, which has the priority at the merge, so this
 * point only gives its rate to the first new one.
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
        auto created =
            std::make_shared<retention_file>(_config.metrics_dir, metric);
        auto primer = _counter_primers.find(metric.metric_id());
        if (primer != _counter_primers.end()) {
          Metric previous(metric);
          previous.set_time(primer->second.first);
          previous.set_value(primer->second.second);
          _counter_primers.erase(primer);
          try {
            created->add_metric(previous);
          } catch (const std::exception& e) {
            SPDLOG_LOGGER_ERROR(_logger,
                                "fail to add previous point of metric {} to "
                                "retention: {}",
                                metric.metric_id(), e);
          }
        }
        _retentions.emplace(std::move(created));
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
  _counter_primers.erase(metric_id);
  auto& id_index = _retentions.get<0>();
  // a file being merged is no longer in _retentions, its pending events must
  // be dropped all the same, otherwise the end of the merge would replay them
  _pending_metrics.erase(metric_id);
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
  // a file being merged is no longer in _retentions, its pending events must
  // be dropped all the same, otherwise the end of the merge would replay them
  _pending_statuses.erase(index_id);
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
 * With rrdcached, the rrd file is flushed first so that the merge reads all
 * its data.
 *
 * @param to_merge The retention file to merge.
 * @param rrd_lib The lib used to create the rrd files, it must outlive the
 * merge.
 * @param cached_client The rrdcached backend of the stream, nullptr if rrd
 * files are written directly.
 */
void retention_manager::_merge(const std::shared_ptr<retention_file>& to_merge,
                               const std::shared_ptr<lib>& rrd_lib,
                               backend* cached_client) {
  auto final_path =
      to_merge->is_status() ? _config.status_dir : _config.metrics_dir;
  final_path /= fmt::format("{}.rrd", to_merge->id());
  SPDLOG_LOGGER_INFO(
      _logger, "RRD: merging {} retention points for {} {} into {}",
      to_merge->nb_samples(), to_merge->is_status() ? "status" : "metric",
      to_merge->id(), final_path.string());
  if (cached_client) {
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
 * On success, the retention file is removed before the publication of the end
 * of the merge, so that a replayed event can create a new one. For a COUNTER
 * or DERIVE metric, its last point is kept in _counter_primers (see
 * write_metric()). On failure, it is kept in _retentions and can't be merged
 * again before merge_retry_delay.
 *
 * In both cases a pb_end_metric_merge or pb_end_status_merge is published:
 * the stream then calls clear_merging(), so that another merge can start, and
 * replays the events kept during the merge.
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
    // keep the last raw value of a counter to compute the rate of the first
    // point of the next retention file of this metric
    if (!to_merge->is_status() && !to_merge->is_merge_aborted() &&
        (to_merge->value_type() == Metric_ValueType_COUNTER ||
         to_merge->value_type() == Metric_ValueType_DERIVE)) {
      absl::MutexLock l(&_data_m);
      _counter_primers[to_merge->id()] =
          std::make_pair(to_merge->last_ts(), to_merge->last_value());
    }
    ::remove(to_merge->file_path().c_str());
    SPDLOG_LOGGER_INFO(_logger, "RRD: merge of {} {} into {} done",
                       to_merge->is_status() ? "status" : "metric",
                       to_merge->id(), final_path.string());
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to merge {} into {}: {}",
                        to_merge->file_path(), final_path, e.what());
    // in order to not log error on next metric received, we mark it
    if (to_merge->is_merge_aborted()) {
      ::remove(to_merge->file_path().c_str());
    } else {
      absl::MutexLock l(&_data_m);
      to_merge->set_no_merge_before(time(nullptr) + _config.merge_retry_delay);
      _retentions.emplace(to_merge);
    }
  }

  // Even on failure, the end of the merge must be published: the stream
  // replays the events kept during the merge only when it receives it.
  {
    absl::MutexLock l(&_data_m);
    _merging_ended = time(nullptr);
  }
  _post_end_merge(to_merge->id(), to_merge->is_status());
}

void retention_manager::_post_end_merge(uint64_t id, bool is_status) {
  multiplexing::publisher publish;
  if (is_status) {
    std::shared_ptr<storage::pb_end_status_merge> to_dispatch =
        std::make_unique<storage::pb_end_status_merge>();
    to_dispatch->mut_obj().set_index_id(id);
    publish.write(to_dispatch);
  } else {
    std::shared_ptr<storage::pb_end_metric_merge> to_dispatch =
        std::make_unique<storage::pb_end_metric_merge>();
    to_dispatch->mut_obj().set_metric_id(id);
    publish.write(to_dispatch);
  }
}

/**
 * @brief Mark the merge of a metric or a status as finished, so that
 * start_merge() can start another one. Called by the stream when it receives
 * the pb_end_metric_merge or pb_end_status_merge event. Nothing is done if
 * this metric or status is not the one being merged.
 *
 * @param id The metric_id or the index_id.
 * @param is_status true if id is an index_id.
 */
void retention_manager::clear_merging(uint64_t id, bool is_status) {
  absl::MutexLock l(&_data_m);
  if (_merging && _merging->id() == id && _merging->is_status() == is_status) {
    _merging.reset();
    _merging_ended = 0;
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
 * The file chosen is, in this order:
 *   - the least recently fed one if it has not been fed for orphan_interval
 *     seconds,
 *   - else the one with the most samples if it has more than
 *     merge_samples_threshold samples,
 *   - else the one not fed for the longest time compared to its interval, if
 *     it has not been fed for interval * proportional_activity_threshold.
 * For each criterion, files whose merge failed less than merge_retry_delay ago
 * are skipped.
 *
 * The file is removed from _retentions and merged on _io_context, the next
 * points of this metric or status will go to a new retention file. The merge
 * stays running until clear_merging() is called.
 *
 * @param rrd_lib The lib used to create the rrd files, it must outlive the
 * merge.
 * @param cached_client The rrdcached backend of the stream, nullptr if rrd
 * files are written directly.
 *
 * @return the id of the metric or status being merged and true if it is a
 * status, nothing if no merge has been started.
 */
std::optional<
    std::pair<uint64_t /* index_id or metric_id*/, bool /*is status*/>>
retention_manager::start_merge(const std::shared_ptr<lib>& rrd_lib,
                               backend* cached_client) {
  absl::MutexLock l(&_data_m);
  if (_merging &&
      (!_merging_ended ||
       _merging_ended + static_cast<time_t>(_config.merge_time_out) >
           time(nullptr))) {
    return {};
  }

  if (_merging) {
    boost::asio::post(
        *_io_context,
        [id = _merging->id(), is_status = _merging->is_status(),
         me = shared_from_this()]() { me->_post_end_merge(id, is_status); });
    _merging.reset();
  }
  _merging_ended = 0;

  time_t now = time(nullptr);
  if (_retentions.empty() ||
      now < _last_start_merge +
                static_cast<time_t>(_config.start_merge_interval)) {
    return {};
  }
  _last_start_merge = now;
  {
    auto& last_interval_index = _retentions.get<1>();
    time_t threshold = now - _config.orphan_interval;
    for (auto to_merge_it = last_interval_index.begin();
         to_merge_it != last_interval_index.end(); ++to_merge_it) {
      auto to_merge = *to_merge_it;
      if (to_merge->last_activity() > threshold) {
        break;
      }
      if (to_merge->no_merge_before() < time(nullptr)) {
        _merge(to_merge, rrd_lib, cached_client);
        last_interval_index.erase(to_merge_it);
        return std::make_pair(to_merge->id(), to_merge->is_status());
      }
    }
  }
  {
    auto& nb_samples_index = _retentions.get<2>();
    for (auto to_merge_it = nb_samples_index.begin();
         to_merge_it != nb_samples_index.end(); ++to_merge_it) {
      auto to_merge = *to_merge_it;
      if (to_merge->nb_samples() <= _config.merge_samples_threshold) {
        break;
      }
      if (to_merge->no_merge_before() < time(nullptr)) {
        _merge(to_merge, rrd_lib, cached_client);
        nb_samples_index.erase(to_merge_it);
        return std::make_pair(to_merge->id(), to_merge->is_status());
      }
    }
  }
  {
    auto& prop_activity_index = _retentions.get<3>();
    for (auto to_merge_it = prop_activity_index.begin();
         to_merge_it != prop_activity_index.end(); ++to_merge_it) {
      auto to_merge = *to_merge_it;
      if (to_merge->last_activity() +
              to_merge->interval() * _config.proportional_activity_threshold >=
          (uint64_t)now) {
        break;
      }
      if (to_merge->no_merge_before() < time(nullptr)) {
        _merge(to_merge, rrd_lib, cached_client);
        prop_activity_index.erase(to_merge_it);
        return std::make_pair(to_merge->id(), to_merge->is_status());
      }
    }
  }
  return {};
}

/**
 * @brief Keep a metric event if its metric is being merged, it will be
 * replayed by the stream at the end of the merge (see get_pending_metric()).
 *
 * @param metric A pb_metric event.
 *
 * @return true if the event has been kept, false if this metric is not being
 * merged and the event must be written now.
 */
bool retention_manager::write_pending_metric(
    const std::shared_ptr<io::data>& metric) {
  absl::MutexLock l(&_data_m);
  uint64_t metric_id =
      std::static_pointer_cast<storage::pb_metric>(metric)->obj().metric_id();
  if (_merging && !_merging->is_status() && _merging->id() == metric_id) {
    _pending_metrics[metric_id].emplace_back(metric);
    return true;
  }
  return false;
}

/**
 * @brief Same as write_pending_metric() for a status.
 *
 * @param status A pb_status event.
 *
 * @return true if the event has been kept, false if this status is not being
 * merged and the event must be written now.
 */
bool retention_manager::write_pending_status(
    const std::shared_ptr<io::data>& status) {
  absl::MutexLock l(&_data_m);
  auto index_id =
      std::static_pointer_cast<storage::pb_status>(status)->obj().index_id();

  if (_merging && _merging->is_status() && _merging->id() == index_id) {
    _pending_statuses[index_id].emplace_back(status);
    return true;
  }
  return false;
}

/**
 * @brief Get and forget the events kept by write_pending_metric() during the
 * merge of a metric, in their arrival order.
 *
 * @param metric_id The metric identifier.
 *
 * @return The events to replay, empty if there is none.
 */
std::list<std::shared_ptr<io::data>> retention_manager::get_pending_metric(
    uint64_t metric_id) {
  absl::MutexLock l(&_data_m);
  auto exist = _pending_metrics.find(metric_id);
  if (exist == _pending_metrics.end()) {
    return {};
  }
  return std::move(_pending_metrics.extract(exist).mapped());
}

/**
 * @brief Same as get_pending_metric() for a status.
 *
 * @param index_id The index identifier.
 *
 * @return The events to replay, empty if there is none.
 */
std::list<std::shared_ptr<io::data>> retention_manager::get_pending_status(
    uint64_t index_id) {
  absl::MutexLock l(&_data_m);
  auto exist = _pending_statuses.find(index_id);
  if (exist == _pending_statuses.end()) {
    return {};
  }
  return std::move(_pending_statuses.extract(exist).mapped());
}

/**
 * Copyright 2011-2015,2017, 2020-2026 Centreon
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

#include <absl/strings/str_join.h>
#include <fmt/format.h>

#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/neb/bbdo2_to_bbdo3.hh"
#include "com/centreon/broker/rrd/internal.hh"
#include "com/centreon/broker/rrd/stream.hh"

#include "bbdo/storage/metric.hh"
#include "bbdo/storage/remove_graph.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/rrd/exceptions/open.hh"
#include "com/centreon/broker/rrd/exceptions/update.hh"
#include "com/centreon/common/perfdata.hh"
#include "com/centreon/common/pool.hh"

namespace asio = boost::asio;

using namespace com::centreon::broker;
using namespace com::centreon::broker::rrd;

namespace com::centreon::broker::rrd {

template <class map_type>
std::vector<typename map_type::key_type> keys_of_map(const map_type& data) {
  std::vector<typename map_type::key_type> ret;
  ret.reserve(data.size());
  for (const auto& key_val : data) {
    ret.push_back(key_val.first);
  }
  return ret;
}

template <class map_type>
absl::flat_hash_set<typename map_type::mapped_type> values_of_map(
    const map_type& data) {
  absl::flat_hash_set<typename map_type::mapped_type> ret;
  for (const auto& key_val : data) {
    ret.insert(key_val.second);
  }
  return ret;
}

/**
 *  Standard constructor.
 *
 *  @param[in] metrics_path         Path in which metrics RRD files
 *                                  should be written.
 *  @param[in] status_path          Path in which status RRD files
 *                                  should be written.
 *  @param[in] cache_size           The maximum number of cache element.
 *  @param[in] ignore_update_errors Set to true to ignore update errors.
 *  @param[in] write_metrics        Set to true if metrics graph must be
 *                                  written.
 *  @param[in] write_status         Set to true if status graph must be
 *                                  written.
 */
template <>
stream<lib>::stream(std::filesystem::path metrics_path,
                    std::filesystem::path status_path,
                    uint32_t cache_size,
                    bool ignore_update_errors,
                    const retention_config& retention_cfg,
                    bool write_metrics,
                    bool write_status)
    : io::stream("RRD"),
      _ignore_update_errors(ignore_update_errors),
      _metrics_path(metrics_path),
      _status_path(status_path),
      _write_metrics(write_metrics),
      _write_status(write_status),
      _backend(!metrics_path.empty() ? std::move(metrics_path)
                                     : std::move(status_path),
               cache_size),
      _retention(retention_manager::load(retention_cfg,
                                         common::pool::io_context_ptr(),
                                         log_v2::instance().get(log_v2::RRD))),
      _merge_lib("", 0),
      _logger{log_v2::instance().get(log_v2::RRD)} {}

/**
 *  Local socket constructor.
 *
 *  @param[in] metrics_path         See standard constructor.
 *  @param[in] status_path          See standard constructor.
 *  @param[in] cache_size           The maximum number of cache element.
 *  @param[in] ignore_update_errors Set to true to ignore update errors.
 *  @param[in] local                Local socket connection parameters.
 *  @param[in] write_metrics        Set to true if metrics graph must be
 *                                  written.
 *  @param[in] write_status         Set to true if status graph must be
 *                                  written.
 */
template <>
stream<cached<asio::local::stream_protocol::socket>>::stream(
    std::filesystem::path metrics_path,
    std::filesystem::path status_path,
    uint32_t cache_size,
    bool ignore_update_errors,
    std::string const& local,
    const retention_config& retention_cfg,
    bool write_metrics,
    bool write_status)
    : io::stream("RRD"),
      _ignore_update_errors(ignore_update_errors),
      _metrics_path(metrics_path),
      _status_path(status_path),
      _write_metrics(write_metrics),
      _write_status(write_status),
      _backend(std::move(metrics_path), cache_size),
      _retention(retention_manager::load(retention_cfg,
                                         common::pool::io_context_ptr(),
                                         log_v2::instance().get(log_v2::RRD))),
      _merge_lib("", 0),
      _logger{log_v2::instance().get(log_v2::RRD)} {}

/**
 *  Network socket constructor.
 *
 *  @param[in] metrics_path         See standard constructor.
 *  @param[in] status_path          See standard constructor.
 *  @param[in] cache_size           The maximum number of cache element.
 *  @param[in] ignore_update_errors Set to true to ignore update errors.
 *  @param[in] port                 rrdcached listening port.
 *  @param[in] write_metrics        Set to true if metrics graph must be
 *                                  written.
 *  @param[in] write_status         Set to true if status graph must be
 *                                  written.
 */
template <>
stream<cached<asio::ip::tcp::socket>>::stream(
    std::filesystem::path metrics_path,
    std::filesystem::path status_path,
    uint32_t cache_size,
    bool ignore_update_errors,
    unsigned short port,
    const retention_config& retention_cfg,
    bool write_metrics,
    bool write_status)
    : io::stream("RRD"),
      _ignore_update_errors(ignore_update_errors),
      _metrics_path(metrics_path),
      _status_path(status_path),
      _write_metrics(write_metrics),
      _write_status(write_status),
      _backend(std::move(metrics_path), cache_size),
      _retention(retention_manager::load(retention_cfg,
                                         common::pool::io_context_ptr(),
                                         log_v2::instance().get(log_v2::RRD))),
      _merge_lib("", 0),
      _logger{log_v2::instance().get(log_v2::RRD)} {}
}  // namespace com::centreon::broker::rrd

/**
 * @brief Destructor: wait for background merge thread to finish.
 */
template <typename T>
stream<T>::~stream() noexcept {
  _retention->stop();
}

/**
 * @brief Schedule a metric merge on the background thread.
 *
 * Nothing is done if a merge of this metric is already queued or running (an
 * entry exists in _pending_metric_merges). Otherwise an entry is inserted, it
 * will collect the events of this metric received during the merge. Once
 * _do_metric_merge is done, the merge thread publishes a pb_end_metric_merge
 * event; when write() receives it, postponed events are replayed and the entry
 * is erased so that a new merge can be scheduled.
 *
 * Must be called from the stream thread.
 *
 * @param metric_id  The metric identifier.
 * @param rrd_path   Absolute path to the metric .rrd file.
 */
// template <typename T>
// void stream<T>::_schedule_metric_merge(uint64_t metric_id,
//                                        std::filesystem::path rrd_path) {
//   if (!_pending_metric_merges.emplace(metric_id, rebuild_entry()).second)
//     return;  // already queued
//   asio::post(
//       _merge_ctx, [this, metric_id, path = std::move(rrd_path)]() mutable {
//         int64_t last_metric_time = _do_metric_merge(metric_id, path);
//         auto end_event = std::make_shared<storage::pb_end_metric_merge>();
//         end_event->mut_obj().set_metric_id(metric_id);
//         end_event->mut_obj().set_last_metric_time(last_metric_time);
//         multiplexing::publisher().write(end_event);
//       });
// }

/**
 * @brief Schedule a status merge on the background thread.
 *
 * Same as _schedule_metric_merge, the end of the merge is signaled by a
 * pb_end_status_merge event.
 *
 * @param index_id   The status index identifier.
 * @param rrd_path   Absolute path to the status .rrd file.
 */
// template <typename T>
// void stream<T>::_schedule_status_merge(uint64_t index_id,
//                                        std::filesystem::path rrd_path) {
//   if (!_pending_status_merges.emplace(index_id, rebuild_entry()).second)
//     return;  // already queued
//   asio::post(
//       _merge_ctx, [this, index_id, path = std::move(rrd_path)]() mutable {
//         int64_t last_status_time = _do_status_merge(index_id, path);
//         auto end_event = std::make_shared<storage::pb_end_status_merge>();
//         end_event->mut_obj().set_index_id(index_id);
//         end_event->mut_obj().set_last_status_time(last_status_time);
//         multiplexing::publisher().write(end_event);
//       });
// }

/**
 *  Read data.
 *
 *  @param[out] d         Cleared.
 *  @param[in]  deadline  Timeout.
 *
 *  @return This method throws.
 */
template <typename T>
bool stream<T>::read(std::shared_ptr<io::data>& d, time_t deadline) {
  (void)deadline;
  d.reset();
  throw com::centreon::broker::exceptions::shutdown(
      "cannot read from RRD stream");
  return true;
}

/**
 *  Update backend after a sigup.
 */
template <typename T>
void stream<T>::update() {
  _backend.clean();
  // if (!_retention.enabled())
  //   return;

  // const uint64_t now = static_cast<uint64_t>(std::time(nullptr));
  // _retention.cleanup_orphans(now);

  // // Deferred-merge sweep: for metrics/statuses with buffered data (typically
  // // from a previous session, recovered by init()) where the .rrd file was
  // not
  // // yet present at startup (so _startup_merge() deferred), but has since
  // been
  // // created by incoming current data.  This is also the implementation of
  // the
  // // "junction via now" condition from the design doc: once the backfill
  // buffer
  // // catches up, current data creates the .rrd, and update() re-attempts the
  // // merge for any metric that has no known earliest_current_time (ect == 0).
  // for (uint64_t metric_id : _retention.metric_ids_with_data()) {
  //   uint64_t ect = 0;
  //   {
  //     absl::ReaderMutexLock lk(&_ect_m);
  //     auto it = _metric_earliest_current.find(metric_id);
  //     if (it != _metric_earliest_current.end())
  //       ect = it->second;
  //   }
  //   if (ect != 0)
  //     continue;  // junction detection in write() handles this case.
  //   auto rrd_path = _metrics_path / fmt::format("{}.rrd", metric_id);
  //   if (std::filesystem::exists(rrd_path)) {
  //     SPDLOG_LOGGER_DEBUG(_logger,
  //                         "RRD: metric {} has buffered data and .rrd exists;
  //                         " "scheduling deferred merge from update()",
  //                         metric_id);
  //     _schedule_metric_merge(metric_id, std::move(rrd_path));
  //   }
  // }
  // for (uint64_t index_id : _retention.status_ids_with_data()) {
  //   uint64_t ect = 0;
  //   {
  //     absl::ReaderMutexLock lk(&_ect_m);
  //     auto it = _status_earliest_current.find(index_id);
  //     if (it != _status_earliest_current.end())
  //       ect = it->second;
  //   }
  //   if (ect != 0)
  //     continue;
  //   auto rrd_path = _status_path / fmt::format("{}.rrd", index_id);
  //   if (std::filesystem::exists(rrd_path)) {
  //     SPDLOG_LOGGER_DEBUG(_logger,
  //                         "RRD: status {} has buffered data and .rrd exists;
  //                         " "scheduling deferred merge from update()",
  //                         index_id);
  //     _schedule_status_merge(index_id, std::move(rrd_path));
  //   }
  // }
}

template <typename T>
void stream<T>::append_metric_to_rrd(const Metric& m) {
  // Metric path.
  auto metric_path = _metrics_path / fmt::format("{}.rrd", m.metric_id());
  try {
    _backend.open(metric_path);
  } catch (exceptions::open const& b) {
    time_t interval(m.interval() ? m.interval() : 60);
    assert(m.rrd_len());
    _backend.open(metric_path, m.rrd_len(), m.time() - 1, interval,
                  m.value_type());
  }
  std::string v;
  switch (m.value_type()) {
    case Metric_ValueType_GAUGE:
      v = fmt::format("{:f}", m.value());
      SPDLOG_LOGGER_TRACE(_logger,
                          "RRD: update metric {} of type GAUGE with {}",
                          m.metric_id(), v);
      break;
    case Metric_ValueType_COUNTER:
      v = fmt::format("{}", static_cast<uint64_t>(m.value()));
      SPDLOG_LOGGER_TRACE(_logger,
                          "RRD: update metric {} of type COUNTER with {}",
                          m.metric_id(), v);
      break;
    case Metric_ValueType_DERIVE:
      v = fmt::format("{}", static_cast<int64_t>(m.value()));
      SPDLOG_LOGGER_TRACE(_logger,
                          "RRD: update metric {} of type DERIVE with {}",
                          m.metric_id(), v);
      break;
    case Metric_ValueType_ABSOLUTE:
      v = fmt::format("{}", static_cast<uint64_t>(m.value()));
      SPDLOG_LOGGER_TRACE(_logger,
                          "RRD: update metric {} of type ABSOLUTE with {}",
                          m.metric_id(), v);
      break;
    default:
      v = fmt::format("{:f}", m.value());
      SPDLOG_LOGGER_TRACE(_logger, "RRD: update metric {} of type {} with {}",
                          m.metric_id(), m.value_type(), v);
      break;
  }

  _backend.update(m.time(), v);
}

template <typename T>
void stream<T>::append_status_to_rrd(const Status& s) {
  auto status_path = _status_path / fmt::format("{}.rrd", s.index_id());
  const std::string* value;
  using namespace std::string_literals;
  static const std::string _100 = "100";
  static const std::string _75 = "75";
  static const std::string _0 = "0";
  static const std::string _undefined = "U";
  switch (s.state()) {
    case 0:
      value = &_100;
      break;
    case 1:
      value = &_75;
      break;
    case 2:
      value = &_0;
      break;
    default:
      value = &_undefined;
      break;
  }
  try {
    _backend.open(status_path);
  } catch (exceptions::open const& b) {
    time_t interval(s.interval() ? s.interval() : 60);
    assert(s.rrd_len());
    _backend.open(status_path, s.rrd_len(), s.time() - 1, interval);
  }
  _backend.update(s.time(), *value);
}

/**
 *  Write an event.
 *
 *  @param[in] d Data to write.
 *
 *  @return Number of events acknowledged.
 */
template <typename T>
uint32_t stream<T>::write(std::shared_ptr<io::data> const& d) {
  SPDLOG_LOGGER_TRACE(_logger, "RRD: stream::write.");
  // Check that data exists.
  if (!validate(d, "RRD"))
    return 1;

  switch (d->type()) {
    case storage::metric::static_type():
      return write(neb::bbdo2_to_bbdo3(d));
    case storage::pb_metric::static_type():
      if (!_write_metrics) {
        // Debug message.
        std::shared_ptr<storage::pb_metric> e(
            std::static_pointer_cast<storage::pb_metric>(d));
        auto& m = e->obj();
        SPDLOG_LOGGER_DEBUG(_logger, "RRD: new pb data for metric {} (time {})",
                            m.metric_id(), m.time());

        // Metric path.
        auto metric_path = _metrics_path / fmt::format("{}.rrd", m.metric_id());

        // Check that metric is not being rebuilt.
        rebuild_cache::iterator it = _metrics_rebuild.find(m.metric_id());
        if (it != _metrics_rebuild.end()) {
          // rebuild running => will be replay later
          it->second.postponed.push_back(d);
          break;
        }
        uint64_t last_update = get_last_metric_latest_current(m.metric_id());
        if (last_update < m.time()) {
          append_metric_to_rrd(m);
        } else {  // we receive an old metric => retention if enabled
          if (_retention->enabled()) {
            _retention->write_metric(m);
          }
        }
      }
      break;
    case storage::pb_status::static_type():
      if (_write_status) {
        // Debug message.
        std::shared_ptr<storage::pb_status> e(
            std::static_pointer_cast<storage::pb_status>(d));
        const auto& s = e->obj();
        SPDLOG_LOGGER_DEBUG(_logger,
                            "RRD: new pb status data for index {} (state {})",
                            s.index_id(), s.state());

        // Status path.
        auto status_path = _status_path / fmt::format("{}.rrd", s.index_id());

        // Check that status is not begin rebuild.
        rebuild_cache::iterator it = _status_rebuild.find(s.index_id());
        if (it != _status_rebuild.end()) {
          // rebuild running => will be replay later
          it->second.postponed.push_back(d);
          break;
        }

        uint64_t last_update = get_last_metric_latest_current(s.index_id());
        if (last_update < s.time()) {
          append_status_to_rrd(s);
        } else {  // we receive an old metric => retention if enabled
          if (_retention->enabled()) {
            _retention->write_status(s);
          }
        }
      }
      break;
    case storage::status::static_type():
      return write(neb::bbdo2_to_bbdo3(d));
    case storage::pb_rebuild_message::static_type(): {
      SPDLOG_LOGGER_DEBUG(_logger, "RRD: RebuildMessage received");
      std::shared_ptr<storage::pb_rebuild_message> e{
          std::static_pointer_cast<storage::pb_rebuild_message>(d)};
      switch (e->obj().state()) {
        case RebuildMessage_State_START:
          if (e->obj().metric_to_index_id().empty()) {
            SPDLOG_LOGGER_ERROR(_logger, "RRD: rebuild empty metric list");
            return 1;
          }
          SPDLOG_LOGGER_INFO(
              _logger, "RRD: Starting to rebuild metrics ({}) status ({})",
              fmt::join(keys_of_map(e->obj().metric_to_index_id()), ","),
              fmt::join(values_of_map(e->obj().metric_to_index_id()), ","));
          // Rebuild is starting.
          _metrics_to_index_rebuild.reserve(
              e->obj().metric_to_index_id().size());
          for (auto& m : e->obj().metric_to_index_id()) {
            /* Creation of metric caches */
            _metrics_rebuild[m.first];
            /* File removed */
            _backend.remove(_metrics_path / fmt::format("{}.rrd", m.first));
            /* Clear stale retention data so the rebuild starts fresh. */
            if (_retention->enabled()) {
              _retention->remove_metric(m.first);
            }
            _metric_latest_current.erase(m.first);

            // creation of status caches
            if (_status_rebuild.find(m.second) == _status_rebuild.end()) {
              _status_rebuild[m.second];
              _metrics_to_index_rebuild[m.first] = m.second;
              /* File removed */
              _backend.remove(_status_path / fmt::format("{}.rrd", m.second));
              if (_retention->enabled()) {
                _retention->remove_status(m.second);
              }
              _status_latest_current.erase(m.second);
            }
          }
          break;
        case RebuildMessage_State_DATA:
          if (_metrics_rebuild.empty()) {
            SPDLOG_LOGGER_ERROR(_logger, "RRD: rebuild empty metric list");
            return 1;
          }
          SPDLOG_LOGGER_DEBUG(_logger, "RRD: Data to rebuild metrics");
          _rebuild_data(e->obj());
          break;
        case RebuildMessage_State_END:
          if (e->obj().metric_to_index_id().empty()) {
            SPDLOG_LOGGER_ERROR(_logger, "RRD: rebuild empty metric list");
            return 1;
          }
          SPDLOG_LOGGER_INFO(
              _logger, "RRD: Finishing to rebuild metrics ({}) status ({})",
              fmt::join(keys_of_map(e->obj().metric_to_index_id()), ","),
              fmt::join(values_of_map(e->obj().metric_to_index_id()), ","));
          // Rebuild is ending.
          for (auto& m : e->obj().metric_to_index_id()) {
            auto it = _metrics_rebuild.find(m.first);
            if (it != _metrics_rebuild.end()) {
              rebuild_entry entry = std::move(it->second);
              _metrics_rebuild.erase(it);
              _write_postponed(entry, m.first, false);
            }
            it = _status_rebuild.find(m.second);
            if (it != _status_rebuild.end()) {
              rebuild_entry entry = std::move(it->second);
              _status_rebuild.erase(it);
              _write_postponed(entry, m.second, true);
            }
            _metrics_to_index_rebuild.erase(m.first);
          }
          break;
        default:
          _logger->error(
              "RRD: Bad 'state' value in rebuild message: it can only contain "
              "START, DATA or END");
          break;
      }
    } break;
    case storage::pb_remove_graph_message::static_type(): {
      SPDLOG_LOGGER_DEBUG(_logger, "RRD: RemoveGraphsMessage received");
      std::shared_ptr<storage::pb_remove_graph_message> e{
          std::static_pointer_cast<storage::pb_remove_graph_message>(d)};
      for (auto& m : e->obj().metric_ids()) {
        auto path = _metrics_path / fmt::format("{}.rrd", m);
        /* File removed */
        SPDLOG_LOGGER_INFO(_logger, "RRD: removing {} file", path);
        _backend.remove(path);
        if (_retention->enabled())
          _retention->remove_metric(m);
        _metric_latest_current.erase(m);
      }
      for (auto& i : e->obj().index_ids()) {
        auto path = _status_path / fmt::format("{}.rrd", i);
        /* File removed */
        SPDLOG_LOGGER_INFO(_logger, "RRD: removing {} file", path);
        _backend.remove(path);
        if (_retention->enabled())
          _retention->remove_status(i);
        _status_latest_current.erase(i);
      }
    } break;
    case storage::remove_graph::static_type(): {
      SPDLOG_LOGGER_INFO(_logger, "storage::remove_graph");
      // Debug message.
      std::shared_ptr<storage::remove_graph> e(
          std::static_pointer_cast<storage::remove_graph>(d));
      SPDLOG_LOGGER_DEBUG(_logger, "RRD: remove graph request for {} {}",
                          e->is_index ? "index" : "metric", e->id);

      // Generate path.
      auto path = (e->is_index ? _status_path : _metrics_path) /
                  fmt::format("{}.rrd", e->id);

      // Remove data from cache.
      rebuild_cache& cache(e->is_index ? _status_rebuild : _metrics_rebuild);
      rebuild_cache::iterator it(cache.find(e->id));
      if (it != cache.end())
        cache.erase(it);

      // Remove file and retention buffer.
      _backend.remove(path);
      if (e->is_index) {
        if (_retention->enabled())
          _retention->remove_status(e->id);
        _status_latest_current.erase(e->id);
      } else {
        if (_retention->enabled())
          _retention->remove_metric(e->id);
        _metric_latest_current.erase(e->id);
      }
    } break;
    case storage::pb_need_metric_merge::static_type(): {
      TODO
    } break;
    case storage::pb_need_status_merge::static_type(): {
      TODO
    } break;
    default:
      _logger->warn("RRD: unknown BBDO message received of type {}", d->type());
  }

  return 1;
}

/**
 * @brief Merge the metric retention buffer into the RRD file.
 *
 * Reads all buffered (time, value) pairs and replays them into the backend.
 * Points that are already in the RRD are silently rejected by librrd/rrdcached.
 * On success, clears the buffer.
 *
 * @param metric_id  The metric identifier.
 * @param rrd_path   Absolute path to the metric's .rrd file.
 */
/**
 * @brief Trigger merges for all metrics and statuses whose retention data was
 *        recovered from disk by retention_manager::init().
 *
 * Called once from each stream constructor after _retention.init() so that
 * buffered data from a previous broker run is replayed into the RRD files
 * immediately, without waiting for new data to arrive.  If an RRD file does
 * not yet exist the merge is silently deferred (the buffer is preserved on
 * disk until the file is created by an incoming current-data write).
 */
// template <typename T>
// void stream<T>::_startup_merge() {
//   if (!_retention.enabled())
//     return;

//   if (_write_metrics) {
//     for (uint64_t id : _retention.metric_ids_with_data()) {
//       auto path = _metrics_path / fmt::format("{}.rrd", id);
//       SPDLOG_LOGGER_INFO(_logger,
//                          "RRD: startup merge for recovered metric {} ('{}')",
//                          id, path.string());
//       _schedule_metric_merge(id, std::move(path));
//     }
//   }

//   if (_write_status) {
//     for (uint64_t id : _retention.status_ids_with_data()) {
//       auto path = _status_path / fmt::format("{}.rrd", id);
//       SPDLOG_LOGGER_INFO(_logger,
//                          "RRD: startup merge for recovered status {} ('{}')",
//                          id, path.string());
//       _schedule_status_merge(id, std::move(path));
//     }
//   }
// }

/**
 * @brief Merge the metric retention buffer into the RRD file.
 *
 * Runs on the merge thread. Buffered (backfill) points are merged with the
 * points already in the RRD file and written to a temporary file which then
 * replaces the RRD file.
 *
 * @param metric_id  The metric identifier.
 * @param rrd_path   Absolute path to the metric .rrd file.
 *
 * @return The most recent timestamp written in the RRD file, 0 if nothing was
 * merged (empty buffer, missing file, error) or if the metric is not a GAUGE
 * (buffer points are then just appended with an rrd update).
 */
// template <typename T>
// int64_t stream<T>::_do_metric_merge(uint64_t metric_id,
//                                     const std::filesystem::path& rrd_path) {
//   // 1. Collect buffered points.
//   auto buf_pts = _retention.get_metric_merge_points(metric_id);
//   if (buf_pts.empty()) {
//     _retention.metric_merge_done(metric_id);
//     return 0;
//   }

//   SPDLOG_LOGGER_INFO(_logger,
//                      "RRD: merging {} buffered points for metric {} into
//                      '{}'", buf_pts.size(), metric_id, rrd_path);

//   // 2. File must exist (created by a current-data write).
//   if (!std::filesystem::exists(rrd_path)) {
//     SPDLOG_LOGGER_DEBUG(
//         _logger,
//         "RRD: metric {} file '{}' not found — deferring merge until RRD
//         exists", metric_id, rrd_path);
//     return 0;  // Keep buffer; will retry when current data creates the file.
//   }

//   // 3. For rrdcached: flush pending writes to disk before reading.
//   //    _backend.pre_merge_flush is thread-safe (socket mutex inside
//   cached<T>). _backend.pre_merge_flush(rrd_path);

//   // 4. Read file metadata and existing data-points using the dedicated merge
//   //    lib (avoids contending on _backend's internal _filename state).
//   const uint64_t now = static_cast<uint64_t>(std::time(nullptr));
//   auto existing =
//       _merge_lib.fetch_existing(rrd_path, buf_pts.front().first, now);

//   if (existing.step == 0) {
//     SPDLOG_LOGGER_WARN(
//         _logger,
//         "RRD: could not read metadata for metric {} from '{}' — skipping",
//         metric_id, rrd_path);
//     _retention.metric_merge_done(metric_id);
//     return 0;
//   }

//   // 5. For non-GAUGE types, rrd_fetch returns rates (not raw values) so a
//   //    full reconstruction would produce wrong data.  Fall back to a direct
//   //    librrd update; some points may be silently dropped but the file is
//   not
//   //    corrupted.  We also call post_merge_forget to keep rrdcached
//   consistent. if (existing.value_type != 0) {
//     SPDLOG_LOGGER_DEBUG(
//         _logger,
//         "RRD: metric {} is non-GAUGE (value_type={}), using direct update "
//         "(points older than last_update may be discarded by librrd)",
//         metric_id, existing.value_type);  // value_type is int, no change
//     try {
//       _merge_lib.open(rrd_path);
//     } catch (const exceptions::open&) {
//       _retention.metric_merge_done(metric_id);
//       return 0;
//     }
//     std::deque<std::string> batch;
//     for (const auto& [t, v] : buf_pts)
//       batch.emplace_back(fmt::format("{}:{:f}", t, v));
//     _merge_lib.update(batch);
//     _backend.post_merge_forget(rrd_path);
//     _retention.metric_merge_done(metric_id);
//     {
//       absl::MutexLock lk(&_ect_m);
//       _metric_earliest_current.erase(metric_id);
//     }
//     return 0;
//   }

//   int64_t last_metric_merged_time = 0;

//   // 6. GAUGE: merge-sort buffer + existing data (buffer wins on collision).
//   absl::btree_map<uint64_t, double> merged;
//   for (const auto& [t, v] : existing.points)
//     merged[t] = v;
//   for (const auto& [t, v] : buf_pts)
//     merged[t] = v;  // buffer overwrites existing for the same timestamp

//   if (!merged.empty()) {
//     last_metric_merged_time = merged.rbegin()->first;
//   }

//   // 7. Build the sorted update batch.
//   std::deque<std::string> batch;
//   for (const auto& [t, v] : merged)
//     batch.emplace_back(fmt::format("{}:{:f}", t, v));

//   // 8. Create temp file and write all merged points via _merge_lib (librrd
//   //    directly, bypasses rrdcached socket — safe from the merge thread).
//   auto tmp_path = rrd_path;
//   tmp_path += ".tmp";
//   const time_t from =
//       static_cast<time_t>(merged.begin()->first) - existing.step;
//   _merge_lib.merge_create_temp(tmp_path, existing.rrd_len, from,
//   existing.step,
//                                existing.value_type, batch);

//   // 9. Atomic rename: replace the live file with the merged one.
//   std::error_code ec;
//   std::filesystem::rename(tmp_path, rrd_path, ec);
//   if (ec) {
//     SPDLOG_LOGGER_ERROR(
//         _logger, "RRD: rename '{}' → '{}' failed: {} — removing tmp file",
//         tmp_path, rrd_path, ec.message());
//     std::filesystem::remove(tmp_path, ec);
//     _retention.metric_merge_done(metric_id);
//     return 0;
//   }

//   // 10. For rrdcached: invalidate its stale queue for this path.
//   _backend.post_merge_forget(rrd_path);

//   // 11. Cleanup.
//   _retention.metric_merge_done(metric_id);
//   {
//     absl::MutexLock lk(&_ect_m);
//     _metric_earliest_current.erase(metric_id);
//   }
//   SPDLOG_LOGGER_INFO(_logger,
//                      "RRD: retention merge for metric {} done ({} points)",
//                      metric_id, merged.size());
//   return last_metric_merged_time;
// }

/**
 * @brief Merge the status retention buffer into the RRD file.
 *
 * Runs on the merge thread, same algorithm as _do_metric_merge.
 *
 * @param index_id   The status index identifier.
 * @param rrd_path   Absolute path to the status .rrd file.
 *
 * @return The most recent timestamp written in the RRD file, 0 if nothing was
 * merged.
 */
// template <typename T>
// int64_t stream<T>::_do_status_merge(uint64_t index_id,
//                                     const std::filesystem::path& rrd_path) {
//   // RRD status strings (static storage, safe for string_view).
//   static constexpr std::string_view kOk{"100"};
//   static constexpr std::string_view kWarn{"75"};
//   static constexpr std::string_view kCrit{"0"};
//   static constexpr std::string_view kUnknown{"U"};

//   // State (0=OK/UP, 1=WARN/DOWN, 2=CRIT, else UNKNOWN/PENDING) → RRD string.
//   static constexpr auto state_to_str = [](uint32_t state) -> std::string_view
//   {
//     switch (state) {
//       case 0:
//         return kOk;
//       case 1:
//         return kWarn;
//       case 2:
//         return kCrit;
//       default:
//         return kUnknown;
//     }
//   };

//   // double percentage from rrd_fetch → RRD string ("U" if out of range).
//   static constexpr auto pct_to_str = [](double v) -> std::string_view {
//     int iv = static_cast<int>(std::round(v));
//     if (iv == 100)
//       return kOk;
//     if (iv == 75)
//       return kWarn;
//     if (iv == 0)
//       return kCrit;
//     return kUnknown;
//   };

//   // 1. Collect buffered points.
//   auto buf_pts = _retention.get_status_merge_points(index_id);
//   if (buf_pts.empty()) {
//     _retention.status_merge_done(index_id);
//     return 0;
//   }

//   SPDLOG_LOGGER_INFO(_logger,
//                      "RRD: merging {} buffered points for status {} into
//                      '{}'", buf_pts.size(), index_id, rrd_path);

//   // 2. File must exist.
//   if (!std::filesystem::exists(rrd_path)) {
//     SPDLOG_LOGGER_DEBUG(
//         _logger,
//         "RRD: status {} file '{}' not found — deferring merge until RRD
//         exists", index_id, rrd_path);
//     return 0;  // Keep buffer; will retry when current data creates the file.
//   }

//   // 3. For rrdcached: flush before read (thread-safe via socket mutex in
//   // cached<T>).
//   _backend.pre_merge_flush(rrd_path);

//   // 4. Read metadata and existing data-points via _merge_lib (no _backend
//   state
//   // used).
//   const uint64_t now = static_cast<uint64_t>(std::time(nullptr));
//   auto existing =
//       _merge_lib.fetch_existing(rrd_path, buf_pts.front().first, now);

//   if (existing.step == 0) {
//     SPDLOG_LOGGER_WARN(
//         _logger,
//         "RRD: could not read metadata for status {} from '{}' — skipping",
//         index_id, rrd_path);
//     _retention.status_merge_done(index_id);
//     return 0;
//   }

//   int64_t last_status_merged_time = 0;

//   // 5. Status RRDs are always GAUGE (percentage "0"/"75"/"100").
//   //    Merge-sort as string_view on static constants; empty = unknown →
//   skip. absl::btree_map<uint64_t, std::string_view> merged; for (const auto&
//   [t, v] : existing.points) {
//     auto sv = pct_to_str(v);
//     if (!sv.empty())
//       merged[t] = sv;
//   }
//   for (const auto& [t, state] : buf_pts) {
//     auto sv = state_to_str(state);
//     if (!sv.empty())
//       merged[t] = sv;  // buffer overwrites existing
//   }

//   if (!merged.empty()) {
//     last_status_merged_time = merged.rbegin()->first;
//   }
//   // 6. Build sorted batch.
//   std::deque<std::string> batch;
//   for (const auto& [t, sv] : merged)
//     batch.emplace_back(fmt::format("{}:{}", t, sv));

//   if (batch.empty()) {
//     _retention.status_merge_done(index_id);
//     return 0;
//   }

//   // 7. Create temp file and write merged data via _merge_lib (librrd
//   directly). auto tmp_path = rrd_path; tmp_path += ".tmp"; const time_t from
//   =
//       static_cast<time_t>(merged.begin()->first) - existing.step;
//   _merge_lib.merge_create_temp(tmp_path, existing.rrd_len, from,
//   existing.step,
//                                /*value_type=*/0, batch);

//   // 8. Atomic rename.
//   std::error_code ec;
//   std::filesystem::rename(tmp_path, rrd_path, ec);
//   if (ec) {
//     SPDLOG_LOGGER_ERROR(
//         _logger, "RRD: rename '{}' → '{}' failed: {} — removing tmp file",
//         tmp_path, rrd_path, ec.message());
//     std::filesystem::remove(tmp_path, ec);
//     _retention.status_merge_done(index_id);
//     return 0;
//   }

//   // 9. For rrdcached: invalidate stale queue.
//   _backend.post_merge_forget(rrd_path);

//   // 10. Cleanup.
//   _retention.status_merge_done(index_id);
//   {
//     absl::MutexLock lk(&_ect_m);
//     _status_earliest_current.erase(index_id);
//   }
//   SPDLOG_LOGGER_INFO(_logger,
//                      "RRD: retention merge for status {} done ({} points)",
//                      index_id, merged.size());
//   return last_status_merged_time;
// }

/**
 * @brief Internal function called to read the protobuf RebuildMessage
 * when timeseries are received. It is here that RRD files are rebuilt.
 *
 * @tparam T The backend RRD.
 * @param rm The message to handle.
 */
template <typename T>
void stream<T>::_rebuild_data(const RebuildMessage& rm) {
  // Always use the direct-write path for rebuild data, even when the retention
  // buffer is enabled.  Rebuild data is written to a freshly-created RRD (the
  // file was removed at START) in strict chronological order, so there is no
  // risk of an "illegal attempt to update" error.  Routing through the
  // retention buffer would make the write asynchronous (merge scheduled at
  // END) which breaks the expectation that data is available immediately after
  // "RRD: Finishing to rebuild metrics" is logged.

  // we can receive the same status indexed by index_id in several metrics, so
  // whe have to reorder that in this container
  struct status_data {
    uint32_t check_interval = 60;
    uint32_t rrd_retention = 0;
    absl::btree_map<uint64_t /*time*/, const char* /* "{}:[100,75,0]" */>
        time_to_value;
  };
  using index_id_to_status_values =
      absl::flat_hash_map<uint64_t /*index_id*/, status_data>;

  index_id_to_status_values status_values;

  auto fill_status_request = [&](uint64_t index_id, uint32_t check_interval,
                                 uint32_t rrd_retention,
                                 const com::centreon::broker::Point& pt) {
    if (!index_id || pt.status() > 2)
      return;
    status_data& to_update = status_values[index_id];
    if (to_update.check_interval < check_interval)
      to_update.check_interval = check_interval;
    if (to_update.rrd_retention < rrd_retention)
      to_update.rrd_retention = rrd_retention;
    switch (pt.status()) {
      case 0:
        to_update.time_to_value[pt.ctime()] = "{}:100";
        break;
      case 1:
        to_update.time_to_value[pt.ctime()] = "{}:75";
        break;
      case 2:
        to_update.time_to_value[pt.ctime()] = "{}:0";
        break;
      default:
        break;
    }
  };

  for (auto& p : rm.timeserie()) {
    std::deque<std::string> query;
    SPDLOG_LOGGER_DEBUG(_logger, "RRD: Rebuilding metric {}", p.first);
    auto path = _metrics_path / fmt::format("{}.rrd", p.first);
    auto index_id_search = _metrics_to_index_rebuild.find(p.first);
    uint64_t index_id = 0;
    if (index_id_search != _metrics_to_index_rebuild.end()) {
      index_id = index_id_search->second;
    }

    int32_t data_source_type = p.second.data_source_type();
    switch (data_source_type) {
      case common::perfdata::gauge:
        for (auto& pt : p.second.pts()) {
          query.emplace_back(fmt::format("{}:{:f}", pt.ctime(), pt.value()));
          fill_status_request(index_id, p.second.check_interval(),
                              p.second.rrd_retention(), pt);
        }
        break;
      case common::perfdata::counter:
      case common::perfdata::absolute:
        for (auto& pt : p.second.pts()) {
          query.emplace_back(fmt::format("{}:{}", pt.ctime(),
                                         static_cast<uint64_t>(pt.value())));
          fill_status_request(index_id, p.second.check_interval(),
                              p.second.rrd_retention(), pt);
        }
        break;
      case common::perfdata::derive:
        for (auto& pt : p.second.pts()) {
          query.emplace_back(fmt::format("{}:{}", pt.ctime(),
                                         static_cast<int64_t>(pt.value())));
          fill_status_request(index_id, p.second.check_interval(),
                              p.second.rrd_retention(), pt);
        }
        break;
      default:
        SPDLOG_LOGGER_DEBUG(_logger, "data_source_type = {} is not managed",
                            data_source_type);
    }

    uint32_t interval{p.second.check_interval() ? p.second.check_interval()
                                                : 60};
    if (!query.empty()) {
      time_t start_time;
      // we substract interval to ensure that first value will be accepted by
      // rrd
      if (!p.second.pts().empty())
        start_time = p.second.pts()[0].ctime() - interval;
      else
        start_time = std::time(nullptr) - interval;
      SPDLOG_LOGGER_TRACE(_logger, "'{}' start date set to {}", path,
                          start_time);
      try {
        /* Here, the file is opened only if it exists. */
        _backend.open(path);
      } catch (const exceptions::open& b) {
        /* Here, the file is created. */
        _backend.open(path, p.second.rrd_retention(), start_time, interval,
                      p.second.data_source_type(), true);
      }
      SPDLOG_LOGGER_TRACE(_logger, "{} points added to file '{}'", query.size(),
                          path);
      _backend.update(query);

      auto rebuild_it = _metrics_rebuild.find(p.first);
      if (rebuild_it != _metrics_rebuild.end()) {
        for (auto& pt : p.second.pts()) {
          if (static_cast<uint64_t>(pt.ctime()) >
              rebuild_it->second.last_rebuilt_time)
            rebuild_it->second.last_rebuilt_time = pt.ctime();
        }
      }

    } else
      SPDLOG_LOGGER_TRACE(_logger, "Nothing to rebuild in '{}'", path);
  }

  for (const auto& by_index_status_values : status_values) {
    SPDLOG_LOGGER_DEBUG(_logger, "RRD: Rebuilding status {}",
                        by_index_status_values.first);
    auto status_path =
        _status_path / fmt::format("{}.rrd", by_index_status_values.first);

    time_t start_time =
        by_index_status_values.second.time_to_value.begin()->first -
        by_index_status_values.second.check_interval;
    try {
      /* Here, the file is opened only if it exists. */
      _backend.open(status_path);
      SPDLOG_LOGGER_TRACE(_logger, "open '{}' start date set to {}",
                          status_path, start_time);
    } catch (const exceptions::open& b) {
      /* Here, the file is created. */
      _backend.open(status_path, by_index_status_values.second.rrd_retention,
                    start_time, by_index_status_values.second.check_interval, 0,
                    true);
      SPDLOG_LOGGER_TRACE(_logger,
                          "create '{}' start date set to {} retention set to "
                          "{}, check interval set to {}",
                          status_path, start_time,
                          by_index_status_values.second.rrd_retention,
                          by_index_status_values.second.check_interval);
    }
    SPDLOG_LOGGER_TRACE(_logger, "{} points added to file '{}'",
                        by_index_status_values.second.time_to_value.size(),
                        status_path);

    std::deque<std::string> status_query;
    for (const auto& time_val : by_index_status_values.second.time_to_value) {
      status_query.emplace_back(fmt::format(time_val.second, time_val.first));
    }

    _backend.update(status_query);

    auto rebuild_it = _status_rebuild.find(by_index_status_values.first);
    if (rebuild_it != _status_rebuild.end()) {
      uint64_t last_time =
          by_index_status_values.second.time_to_value.rbegin()->first;
      if (last_time > rebuild_it->second.last_rebuilt_time)
        rebuild_it->second.last_rebuilt_time = last_time;
    }
  }
}

/**
 * @brief Write events postponed during a rebuild. Events whose time is not
 * after the last timestamp written by the rebuild are dropped: they are
 * already in the rebuilt RRD file (rebuild reads data_bin, which also contains
 * real-time data inserted while the rebuild was running) and rrd would reject
 * them with an "illegal attempt to update" error.
 *
 * @tparam T The backend RRD.
 * @param entry The rebuild cache entry (no more in the cache).
 * @param id metric_id or index_id
 * @param is_status true if id is an index_id
 */
template <typename T>
void stream<T>::_write_postponed(rebuild_entry& entry,
                                 uint64_t id,
                                 bool is_status) {
  while (!entry.postponed.empty()) {
    std::shared_ptr<io::data> d = std::move(entry.postponed.front());
    entry.postponed.pop_front();
    uint64_t t = 0;
    if (d->type() == storage::pb_metric::static_type())
      t = std::static_pointer_cast<storage::pb_metric>(d)->obj().time();
    else if (d->type() == storage::pb_status::static_type())
      t = std::static_pointer_cast<storage::pb_status>(d)->obj().time();
    if (t && t <= entry.last_rebuilt_time) {
      SPDLOG_LOGGER_DEBUG(_logger,
                          "RRD: postponed {} {} data at {} already written by "
                          "rebuild (last rebuilt time {}), ignored",
                          is_status ? "status" : "metric", id, t,
                          entry.last_rebuilt_time);
      continue;
    }
    write(d);
  }
}

template <typename T>
uint64_t stream<T>::get_last_metric_latest_current(uint64_t id) {
  auto last_recorded = _metric_latest_current.find(id);
  if (last_recorded != _metric_latest_current.end()) {
    return last_recorded->second;
  }

  std::filesystem::path rrd_path = _metrics_path / fmt::format("{}.rrd", id);
  // For rrdcached: flush pending writes to disk before reading.
  //    _backend.pre_merge_flush is thread-safe (socket mutex inside cached<T>).
  _backend.pre_merge_flush(rrd_path);

  time_t last_update = lib::last_update(rrd_path);
  if (last_update > 0) {
    _metric_latest_current.emplace(id, last_update);
  }
  return last_update;
}

template <typename T>
uint64_t stream<T>::get_last_status_latest_current(uint64_t id) {
  auto last_recorded = _status_latest_current.find(id);
  if (last_recorded != _status_latest_current.end()) {
    return last_recorded->second;
  }

  std::filesystem::path rrd_path = _status_path / fmt::format("{}.rrd", id);
  // For rrdcached: flush pending writes to disk before reading.
  //    _backend.pre_merge_flush is thread-safe (socket mutex inside cached<T>).
  _backend.pre_merge_flush(rrd_path);

  time_t last_update = lib::last_update(rrd_path);
  if (last_update > 0) {
    _status_latest_current.emplace(id, last_update);
  }
  return last_update;
}

// Explicit instantiations — required for the destructor which is defined in
// this translation unit (not inlined in the header) and must be available to
// the test binary that links against the shared library.
template stream<lib>::~stream() noexcept;
template stream<
    cached<asio::local::stream_protocol::socket>>::~stream() noexcept;
template stream<cached<asio::ip::tcp::socket>>::~stream() noexcept;

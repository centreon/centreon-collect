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
#include <exception>
#include <memory>

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
#include "storage.pb.h"

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
 *  @param[in] retention_cfg        Configuration of the retention of the
 *                                  points older than their rrd file.
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
      _merge_lib(std::make_shared<lib>("", 0)),
      _logger{log_v2::instance().get(log_v2::RRD)} {}

/**
 *  Local socket constructor.
 *
 *  @param[in] metrics_path         See standard constructor.
 *  @param[in] status_path          See standard constructor.
 *  @param[in] cache_size           The maximum number of cache element.
 *  @param[in] ignore_update_errors Set to true to ignore update errors.
 *  @param[in] local                Local socket connection parameters.
 *  @param[in] retention_cfg        See standard constructor.
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
      _merge_lib(std::make_shared<lib>("", 0)),
      _logger{log_v2::instance().get(log_v2::RRD)} {
  _backend.connect_local(local);
}

/**
 *  Network socket constructor.
 *
 *  @param[in] metrics_path         See standard constructor.
 *  @param[in] status_path          See standard constructor.
 *  @param[in] cache_size           The maximum number of cache element.
 *  @param[in] ignore_update_errors Set to true to ignore update errors.
 *  @param[in] port                 rrdcached listening port.
 *  @param[in] retention_cfg        See standard constructor.
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
      _merge_lib(std::make_shared<lib>("", 0)),
      _logger{log_v2::instance().get(log_v2::RRD)} {
  _backend.connect_remote("localhost", port);
}
}  // namespace com::centreon::broker::rrd

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
}

/**
 * @brief Write a metric point in its rrd file, the file is created if it
 * doesn't exist.
 *
 * @param m The metric point.
 *
 * @return false if the point is not written because its time is not after
 * the last update of the rrd file, true otherwise.
 */
template <typename T>
bool stream<T>::append_metric_to_rrd(const Metric& m) {
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

  return _backend.try_update(m.time(), v);
}

/**
 * @brief Write a status point in its rrd file (OK=100, WARNING=75,
 * CRITICAL=0, other=unknown), the file is created if it doesn't exist.
 *
 * @param s The status point.
 *
 * @return false if the point is not written because its time is not after
 * the last update of the rrd file, true otherwise.
 */
template <typename T>
bool stream<T>::append_status_to_rrd(const Status& s) {
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
  return _backend.try_update(s.time(), *value);
}

/**
 *  Write an event.
 *
 *  A metric or status newer than the last update of its rrd file is written in
 *  it, an older one is written in the retention (if enabled) and will be
 *  merged later into the rrd file. After each metric or status, a merge is
 *  started if one is due (see retention_manager::start_merge()).
 *
 *  Events of a metric or status being rebuilt are postponed until the end of
 *  the rebuild. Events of a metric or status being merged are kept by the
 *  retention_manager, they are replayed when the end of the merge
 *  (pb_end_metric_merge / pb_end_status_merge) is received.
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
      if (_write_metrics) {
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
        if (_retention->write_pending_metric(d)) {
          // merge running => will be replay later
          SPDLOG_LOGGER_DEBUG(_logger,
                              "RRD: merge of metric {} running, data at {} "
                              "postponed",
                              m.metric_id(), m.time());
          break;
        }
        // When the last update of the rrd file is not known yet, the point is
        // written and rrd rejects it if it is too old: this avoids reading
        // the file (and an rrdcached flush) for each new metric.
        uint64_t last_update =
            get_last_metric_latest_current(m.metric_id(), false);
        bool written = false;
        if (!last_update || last_update < m.time()) {
          try {
            written = append_metric_to_rrd(m);
          } catch (const std::exception&) {  // in case of update issue (disk
                                             // full, bad value type... we pass
                                             // this event)
            break;
          }
        }
        if (written) {
          _metric_latest_current[m.metric_id()] = m.time();
        } else {             // we receive an old metric => retention if enabled
          if (!last_update)  // now we need the last update of the rrd file
            last_update = get_last_metric_latest_current(m.metric_id(), true);
          if (last_update == m.time()) {  // no duplicate
            break;
          }
          if (_retention->enabled()) {
            _retention->write_metric(m);
          }
        }
        if (_retention->enabled()) {
          auto merging = _retention->start_merge(
              _merge_lib, std::is_same_v<T, lib> ? nullptr : &_backend);
          if (merging) {
            SPDLOG_LOGGER_DEBUG(_logger, "start merge {} for id {}",
                                merging->second ? "status" : "metric",
                                merging->first);
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
        if (_retention->write_pending_status(d)) {
          // merge running => will be replay later
          SPDLOG_LOGGER_DEBUG(_logger,
                              "RRD: merge of status {} running, data at {} "
                              "postponed",
                              s.index_id(), s.time());
          break;
        }

        // Same as metrics: the point is written when the last update of the
        // rrd file is not known yet.
        uint64_t last_update =
            get_last_status_latest_current(s.index_id(), false);
        bool written = false;
        if (!last_update || last_update < s.time()) {
          try {
            written = append_status_to_rrd(s);
          } catch (const std::exception&) {  // in case of update issue (disk
                                             // full, bad value type... we pass
                                             // this event)
            break;
          }
        }
        if (written) {
          _status_latest_current[s.index_id()] = s.time();
        } else {             // we receive an old status => retention if enabled
          if (!last_update)  // now we need the last update of the rrd file
            last_update = get_last_status_latest_current(s.index_id(), true);
          if (last_update == s.time()) {  // ignore duplicate
            break;
          }
          if (_retention->enabled()) {
            _retention->write_status(s);
          }
        }
        if (_retention->enabled()) {
          auto merging = _retention->start_merge(
              _merge_lib, std::is_same_v<T, lib> ? nullptr : &_backend);
          if (merging) {
            SPDLOG_LOGGER_DEBUG(_logger, "start merge {} for id {}",
                                merging->second ? "status" : "metric",
                                merging->first);
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
            /* Clear stale retention data so the rebuild starts fresh. */
            if (_retention->enabled()) {
              _retention->remove_metric(m.first);
              _retention->abort_metric_merge(m.first);
            }
            /* File removed */
            _backend.remove(_metrics_path / fmt::format("{}.rrd", m.first));
            _metric_latest_current.erase(m.first);

            // creation of status caches
            if (_status_rebuild.find(m.second) == _status_rebuild.end()) {
              _status_rebuild[m.second];
              _metrics_to_index_rebuild[m.first] = m.second;
              if (_retention->enabled()) {
                _retention->remove_status(m.second);
                _retention->abort_status_merge(m.second);
              }
              /* File removed */
              _backend.remove(_status_path / fmt::format("{}.rrd", m.second));
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
            _retention->remove_metric(m.first);
            _retention->abort_metric_merge(m.first);
            _retention->remove_status(m.second);
            _retention->abort_status_merge(m.second);
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
        if (_retention->enabled()) {
          _retention->abort_metric_merge(m);
          _retention->remove_metric(m);
        }
        _backend.remove(path);
        _metric_latest_current.erase(m);
      }
      for (auto& i : e->obj().index_ids()) {
        auto path = _status_path / fmt::format("{}.rrd", i);
        /* File removed */
        SPDLOG_LOGGER_INFO(_logger, "RRD: removing {} file", path);
        if (_retention->enabled()) {
          _retention->abort_status_merge(i);
          _retention->remove_status(i);
        }
        _backend.remove(path);
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
      if (e->is_index) {
        if (_retention->enabled()) {
          _retention->abort_status_merge(e->id);
          _retention->remove_status(e->id);
        }
        _status_latest_current.erase(e->id);
      } else {
        if (_retention->enabled()) {
          _retention->abort_metric_merge(e->id);
          _retention->remove_metric(e->id);
        }
        _metric_latest_current.erase(e->id);
      }
      _backend.remove(path);
    } break;
    case storage::pb_end_metric_merge::static_type(): {
      const EndMetricMerge& end_merge =
          std::static_pointer_cast<storage::pb_end_metric_merge>(d)->obj();
      _retention->clear_merging(end_merge.metric_id(), false);
      if constexpr (!std::is_same_v<T, lib>) {
        auto rrd_path =
            _metrics_path / fmt::format("{}.rrd", end_merge.metric_id());
        _backend.post_merge_forget(rrd_path);
      }
      auto events_to_play =
          _retention->get_pending_metric(end_merge.metric_id());
      if (!events_to_play.empty()) {
        // the rrd file has been replaced by the merge
        _metric_latest_current.erase(end_merge.metric_id());
        for (auto evt : events_to_play) {
          write(evt);
        }
      }
    } break;
    case storage::pb_end_status_merge::static_type(): {
      const EndStatusMerge& end_merge =
          std::static_pointer_cast<storage::pb_end_status_merge>(d)->obj();
      _retention->clear_merging(end_merge.index_id(), true);
      if constexpr (!std::is_same_v<T, lib>) {
        auto rrd_path =
            _status_path / fmt::format("{}.rrd", end_merge.index_id());
        _backend.post_merge_forget(rrd_path);
      }
      auto events_to_play =
          _retention->get_pending_status(end_merge.index_id());
      if (!events_to_play.empty()) {
        // the rrd file has been replaced by the merge
        _status_latest_current.erase(end_merge.index_id());
        for (auto evt : events_to_play) {
          write(evt);
        }
      }
    } break;
    default:
      _logger->warn("RRD: unknown BBDO message received of type {}", d->type());
  }

  return 1;
}

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

/**
 * @brief Get the time of the last real-time data written in the rrd file of a
 * metric. Data at this time or older can't be written by rrd_update, they go
 * to the retention.
 *
 * The value is read once from the rrd file and then kept in
 * _metric_latest_current, which is updated on each write in the rrd file and
 * cleared when the file is replaced (merge, rebuild, removal).
 *
 * @param id The metric identifier.
 * @param search_in_rrd_file If the value is not known, read it from the rrd
 * file (after an rrdcached flush), else return 0.
 *
 * @return The last update time, 0 if it is not known or the rrd file doesn't
 * exist.
 */
template <typename T>
uint64_t stream<T>::get_last_metric_latest_current(uint64_t id,
                                                   bool search_in_rrd_file) {
  auto last_recorded = _metric_latest_current.find(id);
  if (last_recorded != _metric_latest_current.end()) {
    return last_recorded->second;
  }

  if (!search_in_rrd_file) {
    return 0;
  }
  std::filesystem::path rrd_path = _metrics_path / fmt::format("{}.rrd", id);
  // For rrdcached: flush pending writes to disk before reading.
  //    _backend.pre_merge_flush is thread-safe (socket mutex inside cached<T>).
  _backend.pre_merge_flush(rrd_path);

  time_t last_update = lib::last_update(rrd_path);
  if (last_update <= 0) {
    // no rrd file yet
    return 0;
  }
  _metric_latest_current.emplace(id, last_update);
  return last_update;
}

/**
 * @brief Same as get_last_metric_latest_current() for the rrd file of a
 * status, the value is kept in _status_latest_current.
 *
 * @param id The index identifier.
 * @param search_in_rrd_file If the value is not known, read it from the rrd
 * file (after an rrdcached flush), else return 0.
 *
 * @return The last update time, 0 if it is not known or the rrd file doesn't
 * exist.
 */
template <typename T>
uint64_t stream<T>::get_last_status_latest_current(uint64_t id,
                                                   bool search_in_rrd_file) {
  auto last_recorded = _status_latest_current.find(id);
  if (last_recorded != _status_latest_current.end()) {
    return last_recorded->second;
  }

  if (!search_in_rrd_file) {
    return 0;
  }

  std::filesystem::path rrd_path = _status_path / fmt::format("{}.rrd", id);
  // For rrdcached: flush pending writes to disk before reading.
  //    _backend.pre_merge_flush is thread-safe (socket mutex inside cached<T>).
  _backend.pre_merge_flush(rrd_path);

  time_t last_update = lib::last_update(rrd_path);
  if (last_update <= 0) {
    // no rrd file yet
    return 0;
  }
  _status_latest_current.emplace(id, last_update);
  return last_update;
}

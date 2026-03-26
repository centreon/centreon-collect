/**
 * Copyright 2011-2015,2017, 2020-2022 Centreon
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

#include "com/centreon/broker/rrd/stream.hh"
#include "com/centreon/broker/rrd/internal.hh"

#include <absl/strings/str_join.h>
#include <fmt/format.h>

#include <cassert>
#include <cstdlib>
#include <iomanip>

#include "bbdo/storage/metric.hh"
#include "bbdo/storage/remove_graph.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/rrd/exceptions/open.hh"
#include "com/centreon/broker/rrd/exceptions/update.hh"
#include "com/centreon/common/perfdata.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::rrd;

namespace com::centreon::broker::rrd {

template <class map_type>
std::vector<typename map_type::key_type> keys_of_map(const map_type& data) {
  std::vector<typename map_type::key_type> ret;
  for (const auto& key_val : data) {
    ret.push_back(key_val.first);
  }
  return ret;
}

template <class map_type>
std::set<typename map_type::mapped_type> values_of_map(const map_type& data) {
  std::set<typename map_type::mapped_type> ret;
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
                    retention_config retention_cfg,
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
      _retention(std::move(retention_cfg), log_v2::instance().get(log_v2::RRD)),
      _logger{log_v2::instance().get(log_v2::RRD)} {
  _retention.init();
}

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
    retention_config retention_cfg,
    bool write_metrics,
    bool write_status)
    : io::stream("RRD"),
      _ignore_update_errors(ignore_update_errors),
      _metrics_path(metrics_path),
      _status_path(status_path),
      _write_metrics(write_metrics),
      _write_status(write_status),
      _backend(std::move(metrics_path), cache_size),
      _retention(std::move(retention_cfg), log_v2::instance().get(log_v2::RRD)),
      _logger{log_v2::instance().get(log_v2::RRD)} {
  _backend.connect_local(local);
  _retention.init();
}

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
stream<cached<asio::ip::tcp::socket>>::stream(std::filesystem::path metrics_path,
                                              std::filesystem::path status_path,
                                              uint32_t cache_size,
                                              bool ignore_update_errors,
                                              unsigned short port,
                                              retention_config retention_cfg,
                                              bool write_metrics,
                                              bool write_status)
    : io::stream("RRD"),
      _ignore_update_errors(ignore_update_errors),
      _metrics_path(metrics_path),
      _status_path(status_path),
      _write_metrics(write_metrics),
      _write_status(write_status),
      _backend(std::move(metrics_path), cache_size),
      _retention(std::move(retention_cfg), log_v2::instance().get(log_v2::RRD)),
      _logger{log_v2::instance().get(log_v2::RRD)} {
  _backend.connect_remote("localhost", port);
  _retention.init();
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
  if (_retention.enabled())
    _retention.cleanup_orphans(static_cast<uint64_t>(std::time(nullptr)));
}

/**
 *  Write an event.
 *
 *  @param[in] d Data to write.
 *
 *  @return Number of events acknowledged.
 */
template <typename T>
int stream<T>::write(std::shared_ptr<io::data> const& d) {
  SPDLOG_LOGGER_TRACE(_logger, "RRD: stream::write.");
  // Check that data exists.
  if (!validate(d, "RRD"))
    return 1;

  switch (d->type()) {
    case storage::pb_metric::static_type():
      if (_write_metrics) {
        // Debug message.
        std::shared_ptr<storage::pb_metric> e(
            std::static_pointer_cast<storage::pb_metric>(d));
        auto& m = e->obj();
        SPDLOG_LOGGER_DEBUG(_logger, "RRD: new pb data for metric {} (time {})",
                            m.metric_id(), m.time());

        // Metric path.
        std::string metric_path(
            (_metrics_path / fmt::format("{}.rrd", m.metric_id())).string());

        // Check that metric is not being rebuilt.
        rebuild_cache::iterator it = _metrics_rebuild.find(metric_path);
        if (it == _metrics_rebuild.end()) {
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
              SPDLOG_LOGGER_TRACE(
                  _logger, "RRD: update metric {} of type COUNTER with {}",
                  m.metric_id(), v);
              break;
            case Metric_ValueType_DERIVE:
              v = fmt::format("{}", static_cast<int64_t>(m.value()));
              SPDLOG_LOGGER_TRACE(
                  _logger, "RRD: update metric {} of type DERIVE with {}",
                  m.metric_id(), v);
              break;
            case Metric_ValueType_ABSOLUTE:
              v = fmt::format("{}", static_cast<uint64_t>(m.value()));
              SPDLOG_LOGGER_TRACE(
                  _logger, "RRD: update metric {} of type ABSOLUTE with {}",
                  m.metric_id(), v);
              break;
            default:
              v = fmt::format("{:f}", m.value());
              SPDLOG_LOGGER_TRACE(_logger,
                                  "RRD: update metric {} of type {} with {}",
                                  m.metric_id(), m.value_type(), v);
              break;
          }
          const uint32_t step = m.interval() ? m.interval() : 60;
          if (!_retention.enabled() ||
              static_cast<time_t>(m.time()) >=
                  std::time(nullptr) - static_cast<time_t>(step)) {
            // Current data → open/create RRD file and write directly.
            try {
              _backend.open(metric_path);
            } catch (exceptions::open const& b) {
              time_t interval(m.interval() ? m.interval() : 60);
              assert(m.rrd_len());
              _backend.open(metric_path, m.rrd_len(), m.time() - 1, interval,
                            m.value_type());
            }
            _backend.update(m.time(), v);
            if (_retention.enabled()) {
              auto [it, inserted] =
                  _metric_earliest_current.try_emplace(m.metric_id(), m.time());
              if (!inserted && m.time() < it->second)
                it->second = m.time();
              // Junction: buffer may have already caught up.
              if (_retention.check_metric_junction(m.metric_id(), it->second)) {
                SPDLOG_LOGGER_DEBUG(
                    _logger,
                    "RRD: metric {} junction reached via current data (ect={})",
                    m.metric_id(), it->second);
                _do_metric_merge(m.metric_id(), metric_path);
              }
            }
          } else {
            // Old (backfill) data → retention buffer only; bypass RRD backend.
            SPDLOG_LOGGER_DEBUG(
                _logger,
                "RRD: metric {} t={} is old (step={}s) → retention buffer",
                m.metric_id(), m.time(), step);
            if (_retention.write_metric(m.metric_id(), m.time(), m.value(),
                                        step)) {
              _do_metric_merge(m.metric_id(), metric_path);
            } else {
              auto ect_it = _metric_earliest_current.find(m.metric_id());
              if (ect_it != _metric_earliest_current.end() &&
                  m.time() + static_cast<uint64_t>(step) >= ect_it->second) {
                SPDLOG_LOGGER_DEBUG(
                    _logger,
                    "RRD: metric {} junction reached "
                    "(t={}+step={}s >= ect={})",
                    m.metric_id(), m.time(), step, ect_it->second);
                _do_metric_merge(m.metric_id(), metric_path);
              }
            }
          }
        } else
          // Cache value.
          it->second.push_back(d);
      }
      break;
    case storage::metric::static_type():
      if (_write_metrics) {
        // Debug message.
        std::shared_ptr<storage::metric> e(
            std::static_pointer_cast<storage::metric>(d));
        SPDLOG_LOGGER_DEBUG(_logger, "RRD: new data for metric {} (time {}) {}",
                            e->metric_id, e->time,
                            e->is_for_rebuild ? "for rebuild" : "");

        // Metric path.
        std::string metric_path(
            (_metrics_path / fmt::format("{}.rrd", e->metric_id)).string());

        // Check that metric is not being rebuilt.
        rebuild_cache::iterator it = _metrics_rebuild.find(metric_path);
        if (e->is_for_rebuild || it == _metrics_rebuild.end()) {
          std::string v;
          switch (e->value_type) {
            case common::perfdata::gauge:
              v = fmt::format("{:f}", e->value);
              SPDLOG_LOGGER_TRACE(_logger,
                                  "RRD: update metric {} of type GAUGE with {}",
                                  e->metric_id, v);
              break;
            case common::perfdata::counter:
              v = fmt::format("{}", static_cast<uint64_t>(e->value));
              SPDLOG_LOGGER_TRACE(
                  _logger, "RRD: update metric {} of type COUNTER with {}",
                  e->metric_id, v);
              break;
            case common::perfdata::derive:
              v = fmt::format("{}", static_cast<int64_t>(e->value));
              SPDLOG_LOGGER_TRACE(
                  _logger, "RRD: update metric {} of type DERIVE with {}",
                  e->metric_id, v);
              break;
            case common::perfdata::absolute:
              v = fmt::format("{}", static_cast<uint64_t>(e->value));
              SPDLOG_LOGGER_TRACE(
                  _logger, "RRD: update metric {} of type ABSOLUTE with {}",
                  e->metric_id, v);
              break;
            default:
              v = fmt::format("{:f}", e->value);
              SPDLOG_LOGGER_TRACE(_logger,
                                  "RRD: update metric {} of type {} with {}",
                                  e->metric_id, e->value_type, v);
              break;
          }
          const uint32_t step = e->interval ? e->interval : 60;
          if (!_retention.enabled() || e->is_for_rebuild ||
              static_cast<time_t>(e->time) >=
                  std::time(nullptr) - static_cast<time_t>(step)) {
            // Current (or rebuild) data → open/create RRD file and write.
            try {
              _backend.open(metric_path);
            } catch (exceptions::open const& b) {
              time_t interval(e->interval ? e->interval : 60);
              assert(e->rrd_len);
              _backend.open(metric_path, e->rrd_len, e->time - 1, interval,
                            e->value_type);
            }
            _backend.update(e->time, v);
            if (_retention.enabled() && !e->is_for_rebuild) {
              auto [it, inserted] = _metric_earliest_current.try_emplace(
                  e->metric_id, static_cast<uint64_t>(e->time));
              if (!inserted && static_cast<uint64_t>(e->time) < it->second)
                it->second = static_cast<uint64_t>(e->time);
              if (_retention.check_metric_junction(e->metric_id, it->second)) {
                SPDLOG_LOGGER_DEBUG(
                    _logger,
                    "RRD: metric {} junction reached via current data (ect={})",
                    e->metric_id, it->second);
                _do_metric_merge(
                    e->metric_id,
                    (_metrics_path / fmt::format("{}.rrd", e->metric_id))
                        .string());
              }
            }
          } else {
            SPDLOG_LOGGER_DEBUG(
                _logger,
                "RRD: metric {} t={} is old (step={}s) → retention buffer",
                e->metric_id, e->time, step);
            if (_retention.write_metric(e->metric_id, e->time, e->value,
                                        step)) {
              _do_metric_merge(
                  e->metric_id,
                  (_metrics_path / fmt::format("{}.rrd", e->metric_id))
                      .string());
            } else {
              auto ect_it = _metric_earliest_current.find(e->metric_id);
              if (ect_it != _metric_earliest_current.end() &&
                  static_cast<uint64_t>(e->time) + step >= ect_it->second) {
                SPDLOG_LOGGER_DEBUG(
                    _logger,
                    "RRD: metric {} junction reached "
                    "(t={}+step={}s >= ect={})",
                    e->metric_id, e->time, step, ect_it->second);
                _do_metric_merge(
                    e->metric_id,
                    (_metrics_path / fmt::format("{}.rrd", e->metric_id))
                        .string());
              }
            }
          }
        } else
          // Cache value.
          it->second.push_back(d);
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
        std::string status_path(
            (_status_path / fmt::format("{}.rrd", s.index_id())).string());

        // Check that status is not begin rebuild.
        rebuild_cache::iterator it(_status_rebuild.find(status_path));
        if (it == _status_rebuild.end()) {
          std::string value;
          switch (s.state()) {
            case 0:
              value = "100";
              break;
            case 1:
              value = "75";
              break;
            case 2:
              value = "0";
              break;
            default:
              value = "U";
              break;
          }
          const uint32_t step = s.interval() ? s.interval() : 60;
          if (!_retention.enabled() ||
              static_cast<time_t>(s.time()) >=
                  std::time(nullptr) - static_cast<time_t>(step)) {
            // Current data → open/create RRD file and write directly.
            try {
              _backend.open(status_path);
            } catch (exceptions::open const& b) {
              time_t interval(s.interval() ? s.interval() : 60);
              assert(s.rrd_len());
              _backend.open(status_path, s.rrd_len(), s.time() - 1, interval);
            }
            _backend.update(s.time(), value);
            if (_retention.enabled()) {
              auto [it, inserted] =
                  _status_earliest_current.try_emplace(s.index_id(), s.time());
              if (!inserted && s.time() < it->second)
                it->second = s.time();
              if (_retention.check_status_junction(s.index_id(), it->second)) {
                SPDLOG_LOGGER_DEBUG(
                    _logger,
                    "RRD: status {} junction reached via current data (ect={})",
                    s.index_id(), it->second);
                _do_status_merge(s.index_id(), status_path);
              }
            }
          } else {
            SPDLOG_LOGGER_DEBUG(
                _logger,
                "RRD: status {} t={} is old (step={}s) → retention buffer",
                s.index_id(), s.time(), step);
            if (_retention.write_status(s.index_id(), s.time(), s.state(),
                                        step)) {
              _do_status_merge(s.index_id(), status_path);
            } else {
              auto ect_it = _status_earliest_current.find(s.index_id());
              if (ect_it != _status_earliest_current.end() &&
                  s.time() + static_cast<uint64_t>(step) >= ect_it->second) {
                SPDLOG_LOGGER_DEBUG(
                    _logger,
                    "RRD: status {} junction reached "
                    "(t={}+step={}s >= ect={})",
                    s.index_id(), s.time(), step, ect_it->second);
                _do_status_merge(s.index_id(), status_path);
              }
            }
          }
        } else
          // Cache value.
          it->second.push_back(d);
      }
      break;
    case storage::status::static_type():
      if (_write_status) {
        // Debug message.
        std::shared_ptr<storage::status> e(
            std::static_pointer_cast<storage::status>(d));
        SPDLOG_LOGGER_DEBUG(
            _logger, "RRD: new status data for index {} (state {}) {}",
            e->index_id, e->state, e->is_for_rebuild ? "for rebuild" : "");

        // Status path.
        std::string status_path(
            (_status_path / fmt::format("{}.rrd", e->index_id)).string());

        // Check that status is not begin rebuild.
        rebuild_cache::iterator it(_status_rebuild.find(status_path));
        if (e->is_for_rebuild || it == _status_rebuild.end()) {
          std::string value;
          switch (e->state) {
            case 0:
              value = "100";
              break;
            case 1:
              value = "75";
              break;
            case 2:
              value = "0";
              break;
            default:
              value = "U";
              break;
          }
          const uint32_t step = e->interval ? e->interval : 60;
          if (!_retention.enabled() || e->is_for_rebuild ||
              static_cast<time_t>(e->time) >=
                  std::time(nullptr) - static_cast<time_t>(step)) {
            // Current (or rebuild) data → open/create RRD file and write.
            try {
              _backend.open(status_path);
            } catch (exceptions::open const& b) {
              time_t interval(e->interval ? e->interval : 60);
              assert(e->rrd_len);
              _backend.open(status_path, e->rrd_len, e->time - 1, interval);
            }
            _backend.update(e->time, value);
            if (_retention.enabled() && !e->is_for_rebuild) {
              auto [it, inserted] = _status_earliest_current.try_emplace(
                  e->index_id, static_cast<uint64_t>(e->time));
              if (!inserted && static_cast<uint64_t>(e->time) < it->second)
                it->second = static_cast<uint64_t>(e->time);
              if (_retention.check_status_junction(e->index_id, it->second)) {
                SPDLOG_LOGGER_DEBUG(
                    _logger,
                    "RRD: status {} junction reached via current data (ect={})",
                    e->index_id, it->second);
                _do_status_merge(
                    e->index_id,
                    (_status_path / fmt::format("{}.rrd", e->index_id))
                        .string());
              }
            }
          } else {
            SPDLOG_LOGGER_DEBUG(
                _logger,
                "RRD: status {} t={} is old (step={}s) → retention buffer",
                e->index_id, e->time, step);
            if (_retention.write_status(e->index_id, e->time, e->state,
                                        step)) {
              _do_status_merge(
                  e->index_id,
                  (_status_path / fmt::format("{}.rrd", e->index_id)).string());
            } else {
              auto ect_it = _status_earliest_current.find(e->index_id);
              if (ect_it != _status_earliest_current.end() &&
                  static_cast<uint64_t>(e->time) + step >= ect_it->second) {
                SPDLOG_LOGGER_DEBUG(
                    _logger,
                    "RRD: status {} junction reached "
                    "(t={}+step={}s >= ect={})",
                    e->index_id, e->time, step, ect_it->second);
                _do_status_merge(
                    e->index_id,
                    (_status_path / fmt::format("{}.rrd", e->index_id))
                        .string());
              }
            }
          }
        } else
          // Cache value.
          it->second.push_back(d);
      }
      break;
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
            std::string path{
                (_metrics_path / fmt::format("{}.rrd", m.first)).string()};
            /* Creation of metric caches */
            _metrics_rebuild[path];
            /* File removed */
            _backend.remove(path);
            // creation of status caches
            path = (_status_path / fmt::format("{}.rrd", m.second)).string();
            if (_status_rebuild.find(path) == _status_rebuild.end()) {
              _status_rebuild[path];
              _metrics_to_index_rebuild[m.first] = m.second;
              /* File removed */
              _backend.remove(path);
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
            std::string path{
                (_metrics_path / fmt::format("{}.rrd", m.first)).string()};
            auto it = _metrics_rebuild.find(path);
            std::list<std::shared_ptr<io::data>> l;
            if (it != _metrics_rebuild.end()) {
              l = std::move(it->second);
              _metrics_rebuild.erase(it);
              while (!l.empty()) {
                write(l.front());
                l.pop_front();
              }
            }
            path = (_status_path / fmt::format("{}.rrd", m.second)).string();
            it = _status_rebuild.find(path);
            if (it != _status_rebuild.end()) {
              l = std::move(it->second);
              _status_rebuild.erase(it);
              while (!l.empty()) {
                write(l.front());
                l.pop_front();
              }
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
        std::string path{
            (_metrics_path / fmt::format("{}.rrd", m)).string()};
        /* File removed */
        SPDLOG_LOGGER_INFO(_logger, "RRD: removing {} file", path);
        _backend.remove(path);
        if (_retention.enabled())
          _retention.remove_metric(m);
        _metric_earliest_current.erase(m);
      }
      for (auto& i : e->obj().index_ids()) {
        std::string path{
            (_status_path / fmt::format("{}.rrd", i)).string()};
        /* File removed */
        SPDLOG_LOGGER_INFO(_logger, "RRD: removing {} file", path);
        _backend.remove(path);
        if (_retention.enabled())
          _retention.remove_status(i);
        _status_earliest_current.erase(i);
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
      std::string path(
          ((e->is_index ? _status_path : _metrics_path) /
           fmt::format("{}.rrd", e->id))
              .string());

      // Remove data from cache.
      rebuild_cache& cache(e->is_index ? _status_rebuild : _metrics_rebuild);
      rebuild_cache::iterator it(cache.find(path));
      if (it != cache.end())
        cache.erase(it);

      // Remove file and retention buffer.
      _backend.remove(path);
      if (e->is_index) {
        if (_retention.enabled())
          _retention.remove_status(e->id);
        _status_earliest_current.erase(e->id);
      } else {
        if (_retention.enabled())
          _retention.remove_metric(e->id);
        _metric_earliest_current.erase(e->id);
      }
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
template <typename T>
void stream<T>::_do_metric_merge(uint64_t metric_id,
                                 const std::string& rrd_path) {
  SPDLOG_LOGGER_INFO(_logger,
                     "RRD: merging retention buffer for metric {} into '{}'",
                     metric_id, rrd_path);

  auto pts = _retention.get_metric_merge_points(metric_id);
  if (pts.empty()) {
    _retention.metric_merge_done(metric_id);
    return;
  }

  try {
    _backend.open(rrd_path);
  } catch (const exceptions::open&) {
    SPDLOG_LOGGER_WARN(
        _logger,
        "RRD: metric {} RRD file '{}' not found during merge — skipping",
        metric_id, rrd_path);
    _retention.metric_merge_done(metric_id);
    return;
  }

  // Build batch of "time:value" strings and replay into the backend.
  std::deque<std::string> batch;
  for (const auto& [t, v] : pts)
    batch.emplace_back(fmt::format("{}:{:f}", t, v));

  _backend.update(batch);
  _retention.metric_merge_done(metric_id);
  _metric_earliest_current.erase(metric_id);
  SPDLOG_LOGGER_INFO(_logger,
                     "RRD: retention merge for metric {} done ({} points)",
                     metric_id, pts.size());
}

/**
 * @brief Merge the status retention buffer into the RRD file.
 *
 * @param index_id   The status index identifier.
 * @param rrd_path   Absolute path to the status .rrd file.
 */
template <typename T>
void stream<T>::_do_status_merge(uint64_t index_id,
                                 const std::string& rrd_path) {
  SPDLOG_LOGGER_INFO(_logger,
                     "RRD: merging retention buffer for status {} into '{}'",
                     index_id, rrd_path);

  auto pts = _retention.get_status_merge_points(index_id);
  if (pts.empty()) {
    _retention.status_merge_done(index_id);
    return;
  }

  try {
    _backend.open(rrd_path);
  } catch (const exceptions::open&) {
    SPDLOG_LOGGER_WARN(
        _logger,
        "RRD: status {} RRD file '{}' not found during merge — skipping",
        index_id, rrd_path);
    _retention.status_merge_done(index_id);
    return;
  }

  // Status → string conversion (same as normal write path).
  std::deque<std::string> batch;
  for (const auto& [t, state] : pts) {
    const char* v;
    switch (state) {
      case 0:
        v = "100";
        break;
      case 1:
        v = "75";
        break;
      case 2:
        v = "0";
        break;
      default:
        v = "U";
        break;
    }
    batch.emplace_back(fmt::format("{}:{}", t, v));
  }

  _backend.update(batch);
  _retention.status_merge_done(index_id);
  _status_earliest_current.erase(index_id);
  SPDLOG_LOGGER_INFO(_logger,
                     "RRD: retention merge for status {} done ({} points)",
                     index_id, pts.size());
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
  // we can receive the same status indexed by index_id in several metrics, so
  // whe have to reorder that in this container
  struct status_data {
    uint32_t check_interval = 60;
    uint32_t rrd_retention = 0;
    std::map<uint64_t /*time*/, const char* /* "{}:[100,75,0]" */>
        time_to_value;
  };
  using index_id_to_status_values =
      std::map<uint64_t /*index_id*/, status_data>;

  index_id_to_status_values status_values;

  auto fill_status_request = [&](uint64_t index_id, uint32_t check_interval,
                                 uint32_t rrd_retention,
                                 const com::centreon::broker::Point& pt) {
    if (!index_id)
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
    std::string path{
        (_metrics_path / fmt::format("{}.rrd", p.first)).string()};
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

    } else
      SPDLOG_LOGGER_TRACE(_logger, "Nothing to rebuild in '{}'", path);
  }

  for (const auto& by_index_status_values : status_values) {
    SPDLOG_LOGGER_DEBUG(_logger, "RRD: Rebuilding status {}",
                        by_index_status_values.first);
    std::string status_path{
        (_status_path / fmt::format("{}.rrd", by_index_status_values.first))
            .string()};

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
  }
}

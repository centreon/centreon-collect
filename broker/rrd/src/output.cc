/*
** Copyright 2011-2015,2017, 2020-2022 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include "com/centreon/broker/rrd/output.hh"
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
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/misc/perfdata.hh"
#include "com/centreon/broker/rrd/exceptions/open.hh"
#include "com/centreon/broker/rrd/exceptions/update.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::rrd;

namespace com {
namespace centreon {
namespace broker {
namespace rrd {

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
output<lib>::output(std::string const& metrics_path,
                    std::string const& status_path,
                    uint32_t cache_size,
                    bool ignore_update_errors,
                    bool write_metrics,
                    bool write_status)
    : io::stream("RRD"),
      _ignore_update_errors(ignore_update_errors),
      _metrics_path(metrics_path),
      _status_path(status_path),
      _write_metrics(write_metrics),
      _write_status(write_status),
      _backend(!metrics_path.empty() ? metrics_path : status_path, cache_size) {
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
output<cached<asio::local::stream_protocol::socket>>::output(
    std::string const& metrics_path,
    std::string const& status_path,
    uint32_t cache_size,
    bool ignore_update_errors,
    std::string const& local,
    bool write_metrics,
    bool write_status)
    : io::stream("RRD"),
      _ignore_update_errors(ignore_update_errors),
      _metrics_path(metrics_path),
      _status_path(status_path),
      _write_metrics(write_metrics),
      _write_status(write_status),
      _backend(metrics_path, cache_size) {
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
 *  @param[in] write_metrics        Set to true if metrics graph must be
 *                                  written.
 *  @param[in] write_status         Set to true if status graph must be
 *                                  written.
 */
template <>
output<cached<asio::ip::tcp::socket>>::output(std::string const& metrics_path,
                                              std::string const& status_path,
                                              uint32_t cache_size,
                                              bool ignore_update_errors,
                                              unsigned short port,
                                              bool write_metrics,
                                              bool write_status)
    : io::stream("RRD"),
      _ignore_update_errors(ignore_update_errors),
      _metrics_path(metrics_path),
      _status_path(status_path),
      _write_metrics(write_metrics),
      _write_status(write_status),
      _backend(metrics_path, cache_size) {
  _backend.connect_remote("localhost", port);
}
}  // namespace rrd
}  // namespace broker
}  // namespace centreon
}  // namespace com

/**
 *  Read data.
 *
 *  @param[out] d         Cleared.
 *  @param[in]  deadline  Timeout.
 *
 *  @return This method throws.
 */
template <typename T>
bool output<T>::read(std::shared_ptr<io::data>& d, time_t deadline) {
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
void output<T>::update() {
  _backend.clean();
}

/**
 *  Write an event.
 *
 *  @param[in] d Data to write.
 *
 *  @return Number of events acknowledged.
 */
template <typename T>
int output<T>::write(std::shared_ptr<io::data> const& d) {
  SPDLOG_LOGGER_TRACE(log_v2::rrd(), "RRD: output::write.");
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
        log_v2::rrd()->debug("RRD: new pb data for metric {} (time {})",
                             m.metric_id(), m.time());

        // Metric path.
        std::string metric_path(
            fmt::format("{}{}.rrd", _metrics_path, m.metric_id()));

        // Check that metric is not being rebuilt.
        rebuild_cache::iterator it = _metrics_rebuild.find(metric_path);
        if (it == _metrics_rebuild.end()) {
          // Write metrics RRD.
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
            case misc::perfdata::gauge:
              v = fmt::format("{:f}", m.value());
              SPDLOG_LOGGER_TRACE(log_v2::rrd(),
                                  "RRD: update metric {} of type GAUGE with {}",
                                  m.metric_id(), v);
              break;
            case misc::perfdata::counter:
              v = fmt::format("{}", static_cast<uint64_t>(m.value()));
              SPDLOG_LOGGER_TRACE(
                  log_v2::rrd(),
                  "RRD: update metric {} of type COUNTER with {}",
                  m.metric_id(), v);
              break;
            case misc::perfdata::derive:
              v = fmt::format("{}", static_cast<int64_t>(m.value()));
              SPDLOG_LOGGER_TRACE(
                  log_v2::rrd(), "RRD: update metric {} of type DERIVE with {}",
                  m.metric_id(), v);
              break;
            case misc::perfdata::absolute:
              v = fmt::format("{}", static_cast<uint64_t>(m.value()));
              SPDLOG_LOGGER_TRACE(
                  log_v2::rrd(),
                  "RRD: update metric {} of type ABSOLUTE with {}",
                  m.metric_id(), v);
              break;
            default:
              v = fmt::format("{:f}", m.value());
              SPDLOG_LOGGER_TRACE(log_v2::rrd(),
                                  "RRD: update metric {} of type {} with {}",
                                  m.metric_id(), m.value_type(), v);
              break;
          }
          _backend.update(m.time(), v);
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
        log_v2::rrd()->debug("RRD: new data for metric {} (time {}) {}",
                             e->metric_id, e->time,
                             e->is_for_rebuild ? "for rebuild" : "");

        // Metric path.
        std::string metric_path(
            fmt::format("{}{}.rrd", _metrics_path, e->metric_id));

        // Check that metric is not being rebuilt.
        rebuild_cache::iterator it = _metrics_rebuild.find(metric_path);
        if (e->is_for_rebuild || it == _metrics_rebuild.end()) {
          // Write metrics RRD.
          try {
            _backend.open(metric_path);
          } catch (exceptions::open const& b) {
            time_t interval(e->interval ? e->interval : 60);
            assert(e->rrd_len);
            _backend.open(metric_path, e->rrd_len, e->time - 1, interval,
                          e->value_type);
          }
          std::string v;
          switch (e->value_type) {
            case misc::perfdata::gauge:
              v = fmt::format("{:f}", e->value);
              SPDLOG_LOGGER_TRACE(log_v2::rrd(),
                                  "RRD: update metric {} of type GAUGE with {}",
                                  e->metric_id, v);
              break;
            case misc::perfdata::counter:
              v = fmt::format("{}", static_cast<uint64_t>(e->value));
              SPDLOG_LOGGER_TRACE(
                  log_v2::rrd(),
                  "RRD: update metric {} of type COUNTER with {}", e->metric_id,
                  v);
              break;
            case misc::perfdata::derive:
              v = fmt::format("{}", static_cast<int64_t>(e->value));
              SPDLOG_LOGGER_TRACE(
                  log_v2::rrd(), "RRD: update metric {} of type DERIVE with {}",
                  e->metric_id, v);
              break;
            case misc::perfdata::absolute:
              v = fmt::format("{}", static_cast<uint64_t>(e->value));
              SPDLOG_LOGGER_TRACE(
                  log_v2::rrd(),
                  "RRD: update metric {} of type ABSOLUTE with {}",
                  e->metric_id, v);
              break;
            default:
              v = fmt::format("{:f}", e->value);
              SPDLOG_LOGGER_TRACE(log_v2::rrd(),
                                  "RRD: update metric {} of type {} with {}",
                                  e->metric_id, e->value_type, v);
              break;
          }
          _backend.update(e->time, v);
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
        log_v2::rrd()->debug("RRD: new pb status data for index {} (state {})",
                             s.index_id(), s.state());

        // Status path.
        std::string status_path(
            fmt::format("{}{}.rrd", _status_path, s.index_id()));

        // Check that status is not begin rebuild.
        rebuild_cache::iterator it(_status_rebuild.find(status_path));
        if (it == _status_rebuild.end()) {
          // Write status RRD.
          try {
            _backend.open(status_path);
          } catch (exceptions::open const& b) {
            time_t interval(s.interval() ? s.interval() : 60);
            assert(s.rrd_len());
            _backend.open(status_path, s.rrd_len(), s.time() - 1, interval);
          }
          std::string value;
          if (s.state() == 0)
            value = "100";
          else if (s.state() == 1)
            value = "75";
          else if (s.state() == 2)
            value = "0";
          else
            value = "";
          _backend.update(s.time(), value);
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
        log_v2::rrd()->debug("RRD: new status data for index {} (state {}) {}",
                             e->index_id, e->state,
                             e->is_for_rebuild ? "for rebuild" : "");

        // Status path.
        std::string status_path(
            fmt::format("{}{}.rrd", _status_path, e->index_id));

        // Check that status is not begin rebuild.
        rebuild_cache::iterator it(_status_rebuild.find(status_path));
        if (e->is_for_rebuild || it == _status_rebuild.end()) {
          // Write status RRD.
          try {
            _backend.open(status_path);
          } catch (exceptions::open const& b) {
            time_t interval(e->interval ? e->interval : 60);
            assert(e->rrd_len);
            _backend.open(status_path, e->rrd_len, e->time - 1, interval);
          }
          std::string value;
          if (e->state == 0)
            value = "100";
          else if (e->state == 1)
            value = "75";
          else if (e->state == 2)
            value = "0";
          else
            value = "";
          _backend.update(e->time, value);
        } else
          // Cache value.
          it->second.push_back(d);
      }
      break;
    case storage::pb_rebuild_message::static_type(): {
      log_v2::rrd()->debug("RRD: RebuildMessage received");
      std::shared_ptr<storage::pb_rebuild_message> e{
          std::static_pointer_cast<storage::pb_rebuild_message>(d)};
      switch (e->obj().state()) {
        case RebuildMessage_State_START:
          log_v2::rrd()->info(
              "RRD: Starting to rebuild metrics ({}) status ({})",
              fmt::join(keys_of_map(e->obj().metric_to_index_id()), ","),
              fmt::join(values_of_map(e->obj().metric_to_index_id()), ","));
          // Rebuild is starting.
          _metrics_to_index_rebuild.reserve(
              e->obj().metric_to_index_id().size());
          for (auto& m : e->obj().metric_to_index_id()) {
            std::string path{fmt::format("{}{}.rrd", _metrics_path, m.first)};
            /* Creation of metric caches */
            _metrics_rebuild[path];
            /* File removed */
            _backend.remove(path);
            // creation of status caches
            path = fmt::format("{}{}.rrd", _status_path, m.second);
            if (_status_rebuild.find(path) == _status_rebuild.end()) {
              _status_rebuild[path];
              _metrics_to_index_rebuild[m.first] = m.second;
              /* File removed */
              _backend.remove(path);
            }
          }
          break;
        case RebuildMessage_State_DATA:
          log_v2::rrd()->debug("RRD: Data to rebuild metrics");
          if (!e->obj().metric_id().empty())
            _rebuild_data_v1(e->obj());
          else
            _rebuild_data(e->obj());
          break;
        case RebuildMessage_State_END:
          log_v2::rrd()->info(
              "RRD: Finishing to rebuild metrics ({}) status ({})",
              fmt::join(keys_of_map(e->obj().metric_to_index_id()), ","),
              fmt::join(values_of_map(e->obj().metric_to_index_id()), ","));
          // Rebuild is ending.
          for (auto& m : e->obj().metric_to_index_id()) {
            std::string path{fmt::format("{}{}.rrd", _metrics_path, m.first)};
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
            path = fmt::format("{}{}.rrd", _status_path, m.second);
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
          log_v2::rrd()->error(
              "RRD: Bad 'state' value in rebuild message: it can only contain "
              "START, DATA or END");
          break;
      }
    } break;
    case storage::pb_remove_graph_message::static_type(): {
      log_v2::rrd()->debug("RRD: RemoveGraphsMessage received");
      std::shared_ptr<storage::pb_remove_graph_message> e{
          std::static_pointer_cast<storage::pb_remove_graph_message>(d)};
      for (auto& m : e->obj().metric_ids()) {
        std::string path{fmt::format("{}{}.rrd", _metrics_path, m)};
        /* File removed */
        log_v2::rrd()->info("RRD: removing {} file", path);
        _backend.remove(path);
      }
      for (auto& i : e->obj().index_ids()) {
        std::string path{fmt::format("{}{}.rrd", _status_path, i)};
        /* File removed */
        log_v2::rrd()->info("RRD: removing {} file", path);
        _backend.remove(path);
      }
    } break;
    case storage::remove_graph::static_type(): {
      log_v2::rrd()->info("storage::remove_graph");
      // Debug message.
      std::shared_ptr<storage::remove_graph> e(
          std::static_pointer_cast<storage::remove_graph>(d));
      log_v2::rrd()->debug("RRD: remove graph request for {} {}",
                           e->is_index ? "index" : "metric", e->id);

      // Generate path.
      std::string path(fmt::format(
          "{}{}.rrd", e->is_index ? _status_path : _metrics_path, e->id));

      // Remove data from cache.
      rebuild_cache& cache(e->is_index ? _status_rebuild : _metrics_rebuild);
      rebuild_cache::iterator it(cache.find(path));
      if (it != cache.end())
        cache.erase(it);

      // Remove file.
      _backend.remove(path);
    } break;
    default:
      log_v2::rrd()->warn("RRD: unknown BBDO message received of type {}",
                          d->type());
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
void output<T>::_rebuild_data_v1(const RebuildMessage& rm) {
  for (auto& p : rm.timeserie()) {
    std::deque<std::string> query;
    log_v2::rrd()->debug("RRD: Rebuilding metric {}", p.first);
    std::string path{fmt::format("{}{}.rrd", _metrics_path, p.first)};
    int32_t data_source_type = p.second.data_source_type();
    switch (data_source_type) {
      case misc::perfdata::gauge:
        for (auto& pt : p.second.pts())
          query.emplace_back(fmt::format("{}:{:f}", pt.ctime(), pt.value()));
        break;
      case misc::perfdata::counter:
      case misc::perfdata::absolute:
        for (auto& pt : p.second.pts())
          query.emplace_back(fmt::format("{}:{}", pt.ctime(),
                                         static_cast<uint64_t>(pt.value())));
        break;
      case misc::perfdata::derive:
        for (auto& pt : p.second.pts())
          query.emplace_back(fmt::format("{}:{}", pt.ctime(),
                                         static_cast<int64_t>(pt.value())));
        break;
      default:
        log_v2::rrd()->debug("data_source_type = {} is not managed",
                             data_source_type);
    }
    if (!query.empty()) {
      time_t start_time;
      if (!p.second.pts().empty())
        start_time = p.second.pts()[0].ctime() - 1;
      else
        start_time = std::time(nullptr);
      log_v2::rrd()->trace("'{}' start date set to {}", path, start_time);
      uint32_t interval{p.second.check_interval() ? p.second.check_interval()
                                                  : 60};
      try {
        /* Here, the file is opened only if it exists. */
        _backend.open(path);
      } catch (const exceptions::open& b) {
        /* Here, the file is created. */
        _backend.open(path, p.second.rrd_retention(), start_time, interval,
                      p.second.data_source_type(), true);
      }
      log_v2::rrd()->trace("{} points added to file '{}'", query.size(), path);
      _backend.update(query);
    } else
      log_v2::rrd()->trace("Nothing to rebuild in '{}'", path);
  }
}

/**
 * @brief Internal function called to read the protobuf RebuildMessage
 * when timeseries are received. It is here that RRD files are rebuilt.
 *
 * @tparam T The backend RRD.
 * @param rm The message to handle.
 */
template <typename T>
void output<T>::_rebuild_data(const RebuildMessage& rm) {
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
    log_v2::rrd()->debug("RRD: Rebuilding metric {}", p.first);
    std::string path{fmt::format("{}{}.rrd", _metrics_path, p.first)};
    auto index_id_search = _metrics_to_index_rebuild.find(p.first);
    uint64_t index_id = 0;
    if (index_id_search != _metrics_to_index_rebuild.end()) {
      index_id = index_id_search->second;
    }

    int32_t data_source_type = p.second.data_source_type();
    switch (data_source_type) {
      case misc::perfdata::gauge:
        for (auto& pt : p.second.pts()) {
          query.emplace_back(fmt::format("{}:{:f}", pt.ctime(), pt.value()));
          fill_status_request(index_id, p.second.check_interval(),
                              p.second.rrd_retention(), pt);
        }
        break;
      case misc::perfdata::counter:
      case misc::perfdata::absolute:
        for (auto& pt : p.second.pts()) {
          query.emplace_back(fmt::format("{}:{}", pt.ctime(),
                                         static_cast<uint64_t>(pt.value())));
          fill_status_request(index_id, p.second.check_interval(),
                              p.second.rrd_retention(), pt);
        }
        break;
      case misc::perfdata::derive:
        for (auto& pt : p.second.pts()) {
          query.emplace_back(fmt::format("{}:{}", pt.ctime(),
                                         static_cast<int64_t>(pt.value())));
          fill_status_request(index_id, p.second.check_interval(),
                              p.second.rrd_retention(), pt);
        }
        break;
      default:
        log_v2::rrd()->debug("data_source_type = {} is not managed",
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
      SPDLOG_LOGGER_TRACE(log_v2::rrd(), "'{}' start date set to {}", path,
                          start_time);
      try {
        /* Here, the file is opened only if it exists. */
        _backend.open(path);
      } catch (const exceptions::open& b) {
        /* Here, the file is created. */
        _backend.open(path, p.second.rrd_retention(), start_time, interval,
                      p.second.data_source_type(), true);
      }
      SPDLOG_LOGGER_TRACE(log_v2::rrd(), "{} points added to file '{}'",
                          query.size(), path);
      _backend.update(query);

    } else
      SPDLOG_LOGGER_TRACE(log_v2::rrd(), "Nothing to rebuild in '{}'", path);
  }

  for (const auto& by_index_status_values : status_values) {
    std::string status_path{
        fmt::format("{}{}.rrd", _status_path, by_index_status_values.first)};

    time_t start_time =
        by_index_status_values.second.time_to_value.begin()->first -
        by_index_status_values.second.check_interval;
    SPDLOG_LOGGER_TRACE(log_v2::rrd(), "'{}' start date set to {}", status_path,
                        start_time);
    try {
      /* Here, the file is opened only if it exists. */
      _backend.open(status_path);
    } catch (const exceptions::open& b) {
      /* Here, the file is created. */
      _backend.open(status_path, by_index_status_values.second.rrd_retention,
                    start_time, by_index_status_values.second.check_interval);
    }
    SPDLOG_LOGGER_TRACE(log_v2::rrd(), "{} points added to file '{}'",
                        by_index_status_values.second.time_to_value.size(),
                        status_path);

    std::deque<std::string> status_query;
    for (const auto& time_val : by_index_status_values.second.time_to_value) {
      status_query.emplace_back(fmt::format(time_val.second, time_val.first));
    }

    _backend.update(status_query);
  }
}

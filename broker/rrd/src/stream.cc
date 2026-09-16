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

#include "com/centreon/broker/rrd/stream.hh"
#include "com/centreon/broker/neb/bbdo2_to_bbdo3.hh"
#include "com/centreon/broker/rrd/internal.hh"

#include <absl/strings/str_join.h>
#include <fmt/format.h>

#include <absl/container/btree_map.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iomanip>

#include "bbdo/storage/metric.hh"
#include "bbdo/storage/remove_graph.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/rrd/exceptions/open.hh"
#include "com/centreon/broker/rrd/exceptions/update.hh"
#include "com/centreon/common/perfdata.hh"

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
      _merge_lib("", 0),
      _logger{log_v2::instance().get(log_v2::RRD)} {
  _merge_work.emplace(_merge_ctx.get_executor());
  _merge_thread = std::thread([this] { _merge_ctx.run(); });
  _retention.init();
  _startup_merge();
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
      _merge_lib("", 0),
      _logger{log_v2::instance().get(log_v2::RRD)} {
  _merge_work.emplace(_merge_ctx.get_executor());
  _merge_thread = std::thread([this] { _merge_ctx.run(); });
  _backend.connect_local(local);
  _retention.init();
  _startup_merge();
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
stream<cached<asio::ip::tcp::socket>>::stream(
    std::filesystem::path metrics_path,
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
      _merge_lib("", 0),
      _logger{log_v2::instance().get(log_v2::RRD)} {
  _merge_work.emplace(_merge_ctx.get_executor());
  _merge_thread = std::thread([this] { _merge_ctx.run(); });
  _backend.connect_remote("localhost", port);
  _retention.init();
  _startup_merge();
}
}  // namespace com::centreon::broker::rrd

/**
 * @brief Destructor: wait for background merge thread to finish.
 */
template <typename T>
stream<T>::~stream() noexcept {
  // Drop the work guard so that the io_context stops after pending tasks
  // finish.
  _merge_work.reset();
  if (_merge_thread.joinable())
    _merge_thread.join();
}

/**
 * @brief Schedule a metric merge on the background thread.
 *
 * If a merge for this metric is already queued or running a new one will still
 * be posted; the pending-set prevents only exact duplicates from queuing up
 * simultaneously.  The pending flag is cleared at the start of @c
 * _do_metric_merge so that a subsequent trigger during the merge is accepted.
 */
template <typename T>
void stream<T>::_schedule_metric_merge(uint64_t metric_id,
                                       std::filesystem::path rrd_path)
    ABSL_NO_THREAD_SAFETY_ANALYSIS {
  {
    absl::MutexLock lk(&_merge_pending_m);
    if (!_pending_metric_merges.insert(metric_id).second)
      return;  // already queued
  }
  asio::post(_merge_ctx,
             [this, metric_id, path = std::move(rrd_path)]() mutable {
               _do_metric_merge(metric_id, path);
             });
}

/**
 * @brief Schedule a status merge on the background thread.
 */
template <typename T>
void stream<T>::_schedule_status_merge(uint64_t index_id,
                                       std::filesystem::path rrd_path)
    ABSL_NO_THREAD_SAFETY_ANALYSIS {
  {
    absl::MutexLock lk(&_merge_pending_m);
    if (!_pending_status_merges.insert(index_id).second)
      return;  // already queued
  }
  asio::post(_merge_ctx,
             [this, index_id, path = std::move(rrd_path)]() mutable {
               _do_status_merge(index_id, path);
             });
}

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
  if (!_retention.enabled())
    return;

  const uint64_t now = static_cast<uint64_t>(std::time(nullptr));
  _retention.cleanup_orphans(now);

  // Deferred-merge sweep: for metrics/statuses with buffered data (typically
  // from a previous session, recovered by init()) where the .rrd file was not
  // yet present at startup (so _startup_merge() deferred), but has since been
  // created by incoming current data.  This is also the implementation of the
  // "junction via now" condition from the design doc: once the backfill buffer
  // catches up, current data creates the .rrd, and update() re-attempts the
  // merge for any metric that has no known earliest_current_time (ect == 0).
  for (uint64_t metric_id : _retention.metric_ids_with_data()) {
    uint64_t ect = 0;
    {
      absl::ReaderMutexLock lk(&_ect_m);
      auto it = _metric_earliest_current.find(metric_id);
      if (it != _metric_earliest_current.end())
        ect = it->second;
    }
    if (ect != 0)
      continue;  // junction detection in write() handles this case.
    auto rrd_path = _metrics_path / fmt::format("{}.rrd", metric_id);
    if (std::filesystem::exists(rrd_path)) {
      SPDLOG_LOGGER_DEBUG(_logger,
                          "RRD: metric {} has buffered data and .rrd exists; "
                          "scheduling deferred merge from update()",
                          metric_id);
      _schedule_metric_merge(metric_id, std::move(rrd_path));
    }
  }
  for (uint64_t index_id : _retention.status_ids_with_data()) {
    uint64_t ect = 0;
    {
      absl::ReaderMutexLock lk(&_ect_m);
      auto it = _status_earliest_current.find(index_id);
      if (it != _status_earliest_current.end())
        ect = it->second;
    }
    if (ect != 0)
      continue;
    auto rrd_path = _status_path / fmt::format("{}.rrd", index_id);
    if (std::filesystem::exists(rrd_path)) {
      SPDLOG_LOGGER_DEBUG(_logger,
                          "RRD: status {} has buffered data and .rrd exists; "
                          "scheduling deferred merge from update()",
                          index_id);
      _schedule_status_merge(index_id, std::move(rrd_path));
    }
  }
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
              uint64_t ect = 0;
              {
                absl::MutexLock lk(&_ect_m);
                auto [it, inserted] = _metric_earliest_current.try_emplace(
                    m.metric_id(), m.time());
                if (!inserted && m.time() < it->second)
                  it->second = m.time();
                ect = it->second;
              }
              // Junction: buffer may have already caught up.
              if (_retention.check_metric_junction(m.metric_id(), ect)) {
                SPDLOG_LOGGER_DEBUG(
                    _logger,
                    "RRD: metric {} junction reached via current data (ect={})",
                    m.metric_id(), ect);
                _schedule_metric_merge(m.metric_id(), metric_path);
              }
            }
          } else {
            // Old (backfill) data → retention buffer only; bypass RRD backend.
            SPDLOG_LOGGER_DEBUG(
                _logger,
                "RRD: metric {} t={} is old (step={}s) → retention buffer",
                m.metric_id(), m.time(), step);
            const uint64_t prev_t = _retention.last_metric_time(m.metric_id());
            if (_retention.write_metric(m.metric_id(), m.time(), m.value(),
                                        step)) {
              _schedule_metric_merge(m.metric_id(), metric_path);
            } else {
              bool should_merge = false;
              uint64_t ect = 0;
              {
                absl::ReaderMutexLock lk(&_ect_m);
                auto ect_it = _metric_earliest_current.find(m.metric_id());
                if (ect_it != _metric_earliest_current.end()) {
                  ect = ect_it->second;
                  if (m.time() + static_cast<uint64_t>(step) >= ect) {
                    should_merge = true;
                    SPDLOG_LOGGER_DEBUG(_logger,
                                        "RRD: metric {} junction reached "
                                        "(t={}+step={}s >= ect={})",
                                        m.metric_id(), m.time(), step, ect);
                  }
                }
              }
              // Step 3.2: Gap > 2×step — check junction for completed batch.
              if (!should_merge && prev_t != 0 && ect != 0 &&
                  m.time() > prev_t + 2 * static_cast<uint64_t>(step) &&
                  prev_t + static_cast<uint64_t>(step) >= ect) {
                should_merge = true;
                SPDLOG_LOGGER_DEBUG(
                    _logger,
                    "RRD: metric {} gap detected "
                    "(t={} − prev={}={}s > 2×step={}s), "
                    "prev junction (prev+step={} >= ect={}) → merge",
                    m.metric_id(), m.time(), prev_t, m.time() - prev_t, step,
                    prev_t + step, ect);
              }
              // Step 3.3: Partial merge — interval of data accumulated.
              if (!should_merge &&
                  _retention.check_metric_partial_merge(m.metric_id())) {
                should_merge = true;
                SPDLOG_LOGGER_DEBUG(
                    _logger,
                    "RRD: metric {} partial-merge interval reached → merge",
                    m.metric_id());
              }
              if (should_merge)
                _schedule_metric_merge(m.metric_id(), metric_path);
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
        auto status_path = _status_path / fmt::format("{}.rrd", s.index_id());

        // Check that status is not begin rebuild.
        rebuild_cache::iterator it(_status_rebuild.find(s.index_id()));
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
              uint64_t ect = 0;
              {
                absl::MutexLock lk(&_ect_m);
                auto [it, inserted] = _status_earliest_current.try_emplace(
                    s.index_id(), s.time());
                if (!inserted && s.time() < it->second)
                  it->second = s.time();
                ect = it->second;
              }
              if (_retention.check_status_junction(s.index_id(), ect)) {
                SPDLOG_LOGGER_DEBUG(
                    _logger,
                    "RRD: status {} junction reached via current data (ect={})",
                    s.index_id(), ect);
                _schedule_status_merge(s.index_id(), status_path);
              }
            }
          } else {
            SPDLOG_LOGGER_DEBUG(
                _logger,
                "RRD: status {} t={} is old (step={}s) → retention buffer",
                s.index_id(), s.time(), step);
            const uint64_t prev_t = _retention.last_status_time(s.index_id());
            if (_retention.write_status(s.index_id(), s.time(), s.state(),
                                        step)) {
              _schedule_status_merge(s.index_id(), status_path);
            } else {
              bool should_merge = false;
              uint64_t ect = 0;
              {
                absl::ReaderMutexLock lk(&_ect_m);
                auto ect_it = _status_earliest_current.find(s.index_id());
                if (ect_it != _status_earliest_current.end()) {
                  ect = ect_it->second;
                  if (s.time() + static_cast<uint64_t>(step) >= ect) {
                    should_merge = true;
                    SPDLOG_LOGGER_DEBUG(_logger,
                                        "RRD: status {} junction reached "
                                        "(t={}+step={}s >= ect={})",
                                        s.index_id(), s.time(), step, ect);
                  }
                }
              }
              if (!should_merge && prev_t != 0 && ect != 0 &&
                  s.time() > prev_t + 2 * static_cast<uint64_t>(step) &&
                  prev_t + static_cast<uint64_t>(step) >= ect) {
                should_merge = true;
                SPDLOG_LOGGER_DEBUG(_logger,
                                    "RRD: status {} gap detected "
                                    "(t={} − prev={}={}s > 2×step={}s), "
                                    "prev junction → merge",
                                    s.index_id(), s.time(), prev_t,
                                    s.time() - prev_t, step);
              }
              if (!should_merge &&
                  _retention.check_status_partial_merge(s.index_id())) {
                should_merge = true;
                SPDLOG_LOGGER_DEBUG(
                    _logger,
                    "RRD: status {} partial-merge interval reached → merge",
                    s.index_id());
              }
              if (should_merge)
                _schedule_status_merge(s.index_id(), status_path);
            }
          }
        } else
          // Cache value.
          it->second.push_back(d);
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
            if (_retention.enabled())
              _retention.remove_metric(m.first);
            // creation of status caches
            if (_status_rebuild.find(m.second) == _status_rebuild.end()) {
              _status_rebuild[m.second];
              _metrics_to_index_rebuild[m.first] = m.second;
              /* File removed */
              _backend.remove(_status_path / fmt::format("{}.rrd", m.second));
              if (_retention.enabled())
                _retention.remove_status(m.second);
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
            std::list<std::shared_ptr<io::data>> l;
            if (it != _metrics_rebuild.end()) {
              l = std::move(it->second);
              _metrics_rebuild.erase(it);
              while (!l.empty()) {
                write(l.front());
                l.pop_front();
              }
            }
            it = _status_rebuild.find(m.second);
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
        auto path = _metrics_path / fmt::format("{}.rrd", m);
        /* File removed */
        SPDLOG_LOGGER_INFO(_logger, "RRD: removing {} file", path);
        _backend.remove(path);
        if (_retention.enabled())
          _retention.remove_metric(m);
        {
          absl::MutexLock lk(&_ect_m);
          _metric_earliest_current.erase(m);
        }
      }
      for (auto& i : e->obj().index_ids()) {
        auto path = _status_path / fmt::format("{}.rrd", i);
        /* File removed */
        SPDLOG_LOGGER_INFO(_logger, "RRD: removing {} file", path);
        _backend.remove(path);
        if (_retention.enabled())
          _retention.remove_status(i);
        {
          absl::MutexLock lk(&_ect_m);
          _status_earliest_current.erase(i);
        }
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
        if (_retention.enabled())
          _retention.remove_status(e->id);
        absl::MutexLock lk(&_ect_m);
        _status_earliest_current.erase(e->id);
      } else {
        if (_retention.enabled())
          _retention.remove_metric(e->id);
        absl::MutexLock lk(&_ect_m);
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
template <typename T>
void stream<T>::_startup_merge() {
  if (!_retention.enabled())
    return;

  if (_write_metrics) {
    for (uint64_t id : _retention.metric_ids_with_data()) {
      auto path = _metrics_path / fmt::format("{}.rrd", id);
      SPDLOG_LOGGER_INFO(_logger,
                         "RRD: startup merge for recovered metric {} ('{}')",
                         id, path.string());
      _schedule_metric_merge(id, std::move(path));
    }
  }

  if (_write_status) {
    for (uint64_t id : _retention.status_ids_with_data()) {
      auto path = _status_path / fmt::format("{}.rrd", id);
      SPDLOG_LOGGER_INFO(_logger,
                         "RRD: startup merge for recovered status {} ('{}')",
                         id, path.string());
      _schedule_status_merge(id, std::move(path));
    }
  }
}

template <typename T>
void stream<T>::_do_metric_merge(uint64_t metric_id,
                                 const std::filesystem::path& rrd_path) {
  // 0. Clear the pending flag so that a new merge can be scheduled while this
  //    one is running (preventing unbounded accumulation).
  {
    absl::MutexLock lk(&_merge_pending_m);
    _pending_metric_merges.erase(metric_id);
  }

  // 1. Collect buffered points.
  auto buf_pts = _retention.get_metric_merge_points(metric_id);
  if (buf_pts.empty()) {
    _retention.metric_merge_done(metric_id);
    return;
  }

  SPDLOG_LOGGER_INFO(_logger,
                     "RRD: merging {} buffered points for metric {} into '{}'",
                     buf_pts.size(), metric_id, rrd_path);

  // 2. File must exist (created by a current-data write).
  if (!std::filesystem::exists(rrd_path)) {
    SPDLOG_LOGGER_DEBUG(
        _logger,
        "RRD: metric {} file '{}' not found — deferring merge until RRD exists",
        metric_id, rrd_path);
    return;  // Keep buffer; will retry when current data creates the file.
  }

  // 3. For rrdcached: flush pending writes to disk before reading.
  //    _backend.pre_merge_flush is thread-safe (socket mutex inside cached<T>).
  _backend.pre_merge_flush(rrd_path);

  // 4. Read file metadata and existing data-points using the dedicated merge
  //    lib (avoids contending on _backend's internal _filename state).
  const uint64_t now = static_cast<uint64_t>(std::time(nullptr));
  auto existing =
      _merge_lib.fetch_existing(rrd_path, buf_pts.front().first, now);

  if (existing.step == 0) {
    SPDLOG_LOGGER_WARN(
        _logger,
        "RRD: could not read metadata for metric {} from '{}' — skipping",
        metric_id, rrd_path);
    _retention.metric_merge_done(metric_id);
    return;
  }

  // 5. For non-GAUGE types, rrd_fetch returns rates (not raw values) so a
  //    full reconstruction would produce wrong data.  Fall back to a direct
  //    librrd update; some points may be silently dropped but the file is not
  //    corrupted.  We also call post_merge_forget to keep rrdcached consistent.
  if (existing.value_type != 0) {
    SPDLOG_LOGGER_DEBUG(
        _logger,
        "RRD: metric {} is non-GAUGE (value_type={}), using direct update "
        "(points older than last_update may be discarded by librrd)",
        metric_id, existing.value_type);  // value_type is int, no change
    try {
      _merge_lib.open(rrd_path);
    } catch (const exceptions::open&) {
      _retention.metric_merge_done(metric_id);
      return;
    }
    std::deque<std::string> batch;
    for (const auto& [t, v] : buf_pts)
      batch.emplace_back(fmt::format("{}:{:f}", t, v));
    _merge_lib.update(batch);
    _backend.post_merge_forget(rrd_path);
    _retention.metric_merge_done(metric_id);
    {
      absl::MutexLock lk(&_ect_m);
      _metric_earliest_current.erase(metric_id);
    }
    return;
  }

  // 6. GAUGE: merge-sort buffer + existing data (buffer wins on collision).
  absl::btree_map<uint64_t, double> merged;
  for (const auto& [t, v] : existing.points)
    merged[t] = v;
  for (const auto& [t, v] : buf_pts)
    merged[t] = v;  // buffer overwrites existing for the same timestamp

  // 7. Build the sorted update batch.
  std::deque<std::string> batch;
  for (const auto& [t, v] : merged)
    batch.emplace_back(fmt::format("{}:{:f}", t, v));

  // 8. Create temp file and write all merged points via _merge_lib (librrd
  //    directly, bypasses rrdcached socket — safe from the merge thread).
  auto tmp_path = rrd_path;
  tmp_path += ".tmp";
  const time_t from =
      static_cast<time_t>(merged.begin()->first) - existing.step;
  _merge_lib.merge_create_temp(tmp_path, existing.rrd_len, from, existing.step,
                               existing.value_type, batch);

  // 9. Atomic rename: replace the live file with the merged one.
  std::error_code ec;
  std::filesystem::rename(tmp_path, rrd_path, ec);
  if (ec) {
    SPDLOG_LOGGER_ERROR(
        _logger, "RRD: rename '{}' → '{}' failed: {} — removing tmp file",
        tmp_path, rrd_path, ec.message());
    std::filesystem::remove(tmp_path, ec);
    _retention.metric_merge_done(metric_id);
    return;
  }

  // 10. For rrdcached: invalidate its stale queue for this path.
  _backend.post_merge_forget(rrd_path);

  // 11. Cleanup.
  _retention.metric_merge_done(metric_id);
  {
    absl::MutexLock lk(&_ect_m);
    _metric_earliest_current.erase(metric_id);
  }
  SPDLOG_LOGGER_INFO(_logger,
                     "RRD: retention merge for metric {} done ({} points)",
                     metric_id, merged.size());
}

/**
 * @brief Merge the status retention buffer into the RRD file.
 *
 * @param index_id   The status index identifier.
 * @param rrd_path   Absolute path to the status .rrd file.
 */
template <typename T>
void stream<T>::_do_status_merge(uint64_t index_id,
                                 const std::filesystem::path& rrd_path) {
  // 0. Clear pending flag.
  {
    absl::MutexLock lk(&_merge_pending_m);
    _pending_status_merges.erase(index_id);
  }
  // RRD status strings (static storage, safe for string_view).
  static constexpr std::string_view kOk{"100"};
  static constexpr std::string_view kWarn{"75"};
  static constexpr std::string_view kCrit{"0"};
  static constexpr std::string_view kUnknown{"U"};

  // State (0=OK/UP, 1=WARN/DOWN, 2=CRIT, else UNKNOWN/PENDING) → RRD string.
  static constexpr auto state_to_str = [](uint32_t state) -> std::string_view {
    switch (state) {
      case 0:
        return kOk;
      case 1:
        return kWarn;
      case 2:
        return kCrit;
      default:
        return kUnknown;
    }
  };

  // double percentage from rrd_fetch → RRD string ("U" if out of range).
  static constexpr auto pct_to_str = [](double v) -> std::string_view {
    int iv = static_cast<int>(std::round(v));
    if (iv == 100)
      return kOk;
    if (iv == 75)
      return kWarn;
    if (iv == 0)
      return kCrit;
    return kUnknown;
  };

  // 1. Collect buffered points.
  auto buf_pts = _retention.get_status_merge_points(index_id);
  if (buf_pts.empty()) {
    _retention.status_merge_done(index_id);
    return;
  }

  SPDLOG_LOGGER_INFO(_logger,
                     "RRD: merging {} buffered points for status {} into '{}'",
                     buf_pts.size(), index_id, rrd_path);

  // 2. File must exist.
  if (!std::filesystem::exists(rrd_path)) {
    SPDLOG_LOGGER_DEBUG(
        _logger,
        "RRD: status {} file '{}' not found — deferring merge until RRD exists",
        index_id, rrd_path);
    return;  // Keep buffer; will retry when current data creates the file.
  }

  // 3. For rrdcached: flush before read (thread-safe via socket mutex in
  // cached<T>).
  _backend.pre_merge_flush(rrd_path);

  // 4. Read metadata and existing data-points via _merge_lib (no _backend state
  // used).
  const uint64_t now = static_cast<uint64_t>(std::time(nullptr));
  auto existing =
      _merge_lib.fetch_existing(rrd_path, buf_pts.front().first, now);

  if (existing.step == 0) {
    SPDLOG_LOGGER_WARN(
        _logger,
        "RRD: could not read metadata for status {} from '{}' — skipping",
        index_id, rrd_path);
    _retention.status_merge_done(index_id);
    return;
  }

  // 5. Status RRDs are always GAUGE (percentage "0"/"75"/"100").
  //    Merge-sort as string_view on static constants; empty = unknown → skip.
  absl::btree_map<uint64_t, std::string_view> merged;
  for (const auto& [t, v] : existing.points) {
    auto sv = pct_to_str(v);
    if (!sv.empty())
      merged[t] = sv;
  }
  for (const auto& [t, state] : buf_pts) {
    auto sv = state_to_str(state);
    if (!sv.empty())
      merged[t] = sv;  // buffer overwrites existing
  }

  // 6. Build sorted batch.
  std::deque<std::string> batch;
  for (const auto& [t, sv] : merged)
    batch.emplace_back(fmt::format("{}:{}", t, sv));

  if (batch.empty()) {
    _retention.status_merge_done(index_id);
    return;
  }

  // 7. Create temp file and write merged data via _merge_lib (librrd directly).
  auto tmp_path = rrd_path;
  tmp_path += ".tmp";
  const time_t from =
      static_cast<time_t>(merged.begin()->first) - existing.step;
  _merge_lib.merge_create_temp(tmp_path, existing.rrd_len, from, existing.step,
                               /*value_type=*/0, batch);

  // 8. Atomic rename.
  std::error_code ec;
  std::filesystem::rename(tmp_path, rrd_path, ec);
  if (ec) {
    SPDLOG_LOGGER_ERROR(
        _logger, "RRD: rename '{}' → '{}' failed: {} — removing tmp file",
        tmp_path, rrd_path, ec.message());
    std::filesystem::remove(tmp_path, ec);
    _retention.status_merge_done(index_id);
    return;
  }

  // 9. For rrdcached: invalidate stale queue.
  _backend.post_merge_forget(rrd_path);

  // 10. Cleanup.
  _retention.status_merge_done(index_id);
  {
    absl::MutexLock lk(&_ect_m);
    _status_earliest_current.erase(index_id);
  }
  SPDLOG_LOGGER_INFO(_logger,
                     "RRD: retention merge for status {} done ({} points)",
                     index_id, merged.size());
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
  }
}

// Explicit instantiations — required for the destructor which is defined in
// this translation unit (not inlined in the header) and must be available to
// the test binary that links against the shared library.
template stream<lib>::~stream() noexcept;
template stream<
    cached<asio::local::stream_protocol::socket>>::~stream() noexcept;
template stream<cached<asio::ip::tcp::socket>>::~stream() noexcept;

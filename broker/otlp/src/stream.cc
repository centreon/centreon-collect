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

#include "com/centreon/broker/otlp/stream.hh"

#include "bbdo/neb.pb.h"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::otlp;

stream::stream(const otlp_config::pointer& conf,
               const std::shared_ptr<resource_enricher>& enricher,
               const std::shared_ptr<exporter_base>& exporter,
               const std::shared_ptr<spdlog::logger>& logger)
    : io::stream("otlp"),
      _conf(conf),
      _logger(logger),
      _enricher(enricher),
      _exporter(exporter),
      _builder(std::make_unique<request_builder>(conf, enricher, logger)),
      _last_send(std::time(nullptr)) {}

bool stream::read(std::shared_ptr<io::data>& d, time_t deadline [[maybe_unused]]) {
  d.reset();
  throw exceptions::shutdown("cannot read from OTLP stream");
}

int stream::_take_acknowledged_locked() {
  const int acknowledged = static_cast<int>(_acknowledged);
  _acknowledged = 0;
  return acknowledged;
}

std::optional<stream::pending_export> stream::_prepare_send_locked() {
  if (_builder->empty())
    return std::nullopt;

  /* Saturated: stop acknowledging so the muxer holds the backlog and, past
   * event_queue_max_size, spills it to the retention file. Building our own
   * queue here would duplicate that and lose the retention file's durability. */
  if (_inflight >= _conf->max_inflight_requests) {
    SPDLOG_LOGGER_DEBUG(_logger,
                        "otlp: {} exports in flight, deferring batch",
                        _inflight);
    return std::nullopt;
  }

  /* Read the count before take(), which resets the builder. */
  const uint64_t nb_data = _builder->nb_data();
  pending_export batch{_builder->take(), nb_data};
  ++_inflight;
  _last_send = std::time(nullptr);
  return batch;
}

void stream::_dispatch(pending_export&& batch) {
  const uint64_t nb_data = batch.nb_data;
  _exporter->export_async(
      std::move(batch.request), nb_data,
      [this](const ::grpc::Status& status,
             const exporter_base::ExportResponse&, uint64_t sent) {
        /* Safe to take the lock: the caller released it before dispatching,
         * precisely so a synchronous completion cannot deadlock. */
        std::lock_guard<std::mutex> l(_protect);
        --_inflight;
        if (status.ok()) {
          ++_stat_batches_sent;
          _stat_datapoints_sent += sent;
        } else {
          ++_stat_export_errors;
        }
      });
}

int stream::write(std::shared_ptr<io::data> const& d) {
  std::optional<pending_export> to_send;
  int acknowledged;

  {
    std::lock_guard<std::mutex> l(_protect);

    if (!validate(d, get_name())) {
      ++_acknowledged;
      return _take_acknowledged_locked();
    }

    switch (d->type()) {
      case neb::pb_service_status::static_type(): {
        const auto& status =
            std::static_pointer_cast<neb::pb_service_status>(d)->obj();
        if (!_builder->add_service_status(status))
          ++_stat_dropped_no_host_name;
        /* Acknowledged either way: a host that is never resolvable would
         * otherwise block the pipeline permanently. */
        ++_acknowledged;
        break;
      }
      case neb::pb_host_status::static_type(): {
        const auto& status =
            std::static_pointer_cast<neb::pb_host_status>(d)->obj();
        if (!_builder->add_host_status(status))
          ++_stat_dropped_no_host_name;
        ++_acknowledged;
        break;
      }
      default:
        /* Not ours; acknowledge immediately so it does not stall the muxer. */
        ++_acknowledged;
        break;
    }

    if (_builder->nb_data() >= _conf->max_datapoints_per_batch)
      to_send = _prepare_send_locked();
    acknowledged = _take_acknowledged_locked();
  }

  if (to_send)
    _dispatch(std::move(*to_send));
  return acknowledged;
}

int stream::flush() {
  std::optional<pending_export> to_send;
  int acknowledged;

  {
    std::lock_guard<std::mutex> l(_protect);
    const std::time_t now = std::time(nullptr);
    if (now >= _last_send + static_cast<std::time_t>(_conf->max_send_interval))
      to_send = _prepare_send_locked();
    acknowledged = _take_acknowledged_locked();
  }

  if (to_send)
    _dispatch(std::move(*to_send));
  return acknowledged;
}

int32_t stream::stop() {
  std::optional<pending_export> to_send;
  int acknowledged;

  {
    std::lock_guard<std::mutex> l(_protect);
    to_send = _prepare_send_locked();
    acknowledged = _take_acknowledged_locked();
  }

  if (to_send)
    _dispatch(std::move(*to_send));
  return acknowledged;
}

void stream::statistics(nlohmann::json& tree) const {
  std::lock_guard<std::mutex> l(_protect);
  tree["batches_sent"] = _stat_batches_sent;
  tree["datapoints_sent"] = _stat_datapoints_sent;
  tree["export_errors"] = _stat_export_errors;
  tree["dropped_no_host_name"] = _stat_dropped_no_host_name;
  tree["inflight_requests"] = _inflight;
  tree["pending_datapoints"] = _builder->nb_data();
}

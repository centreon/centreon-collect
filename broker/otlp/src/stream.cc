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

#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/neb/internal.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::otlp;

/**
 * @brief Construct the stream and its request builder.
 *
 * @param conf endpoint configuration
 * @param enricher resolves host names, service descriptions and OTel identity
 * @param mapping semantic convention mapping, shared with the endpoint
 * @param exporter sends the batches
 * @param logger
 * @param host_metadata CMA host information, shared with the endpoint, may be
 * null
 */
stream::stream(const otlp_config::pointer& conf,
               const std::shared_ptr<resource_enricher>& enricher,
               const mapping_provider::pointer& mapping,
               const std::shared_ptr<exporter_base>& exporter,
               const std::shared_ptr<spdlog::logger>& logger,
               const host_metadata_store::pointer& host_metadata)
    : io::stream("otlp"),
      _conf(conf),
      _logger(logger),
      _enricher(enricher),
      _mapping(mapping),
      _exporter(exporter),
      _host_metadata(host_metadata),
      _builder(std::make_unique<request_builder>(conf,
                                                 enricher,
                                                 mapping,
                                                 logger,
                                                 host_metadata)),
      _last_send(std::time(nullptr)) {}

/**
 * @brief This stream is write-only.
 *
 * @param d reset
 * @param deadline unused
 * @throw exceptions::shutdown always
 */
bool stream::read(std::shared_ptr<io::data>& d,
                  time_t deadline [[maybe_unused]]) {
  d.reset();
  throw exceptions::shutdown("cannot read from OTLP stream");
}

/**
 * @brief Return the number of events to acknowledge and reset it.
 *
 * Call with _protect held.
 *
 * @return number of events to report to the muxer
 */
int stream::_take_acknowledged_locked() {
  const int acknowledged = static_cast<int>(_acknowledged);
  _acknowledged = 0;
  return acknowledged;
}

/**
 * @brief Detach the current batch, unless it is empty or max_inflight_requests
 * exports are already in flight.
 *
 * Call with _protect held, and dispatch the result after releasing it.
 *
 * @return the batch to dispatch, or nullopt
 */
std::optional<stream::pending_export> stream::_prepare_send_locked() {
  if (_builder->empty())
    return std::nullopt;

  if (_inflight >= _conf->max_inflight_requests) {
    SPDLOG_LOGGER_DEBUG(_logger, "otlp: {} exports in flight, deferring batch",
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

/**
 * @brief Hand a detached batch to the exporter. Call without _protect held.
 *
 * The completion callback runs on a gRPC thread: it takes _protect, releases
 * the in-flight slot, updates the statistics and logs the batch sent.
 *
 * @param batch
 */
void stream::_dispatch(pending_export&& batch) {
  const uint64_t nb_data = batch.nb_data;
  const int nb_hosts = batch.request.resource_metrics_size();
  const auto start = std::chrono::steady_clock::now();
  _exporter->export_async(
      std::move(batch.request), nb_data,
      [this, nb_hosts, start](const ::grpc::Status& status,
                              const exporter_base::ExportResponse&,
                              uint64_t sent) {
        std::lock_guard<std::mutex> l(_protect);
        --_inflight;
        if (status.ok()) {
          ++_stat_batches_sent;
          _stat_datapoints_sent += sent;
          /* failures are logged by the exporter */
          SPDLOG_LOGGER_INFO(
              _logger,
              "otlp: {} datapoints of {} hosts sent in {} ms ({} datapoints "
              "in {} batches since start)",
              sent, nb_hosts,
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start)
                  .count(),
              _stat_datapoints_sent, _stat_batches_sent);
        } else {
          ++_stat_export_errors;
        }
      });
}

/**
 * @brief Add an event to the current batch.
 *
 * Service and host statuses go to the request builder, AgentHostInfo to the
 * host information store. Every event is acknowledged, including a status whose
 * host name is unknown (counted in dropped_no_host_name). The batch is sent
 * once it holds max_datapoints_per_batch datapoints.
 *
 * @param d event
 * @return number of events to acknowledge
 */
int stream::write(std::shared_ptr<io::data> const& d) {
  SPDLOG_LOGGER_TRACE(_logger, "OTLP: event category:{}, element:{}",
                      category_of_type(d->type()), element_of_type(d->type()));

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
      case neb::pb_agent_host_info::static_type(): {
        ++_stat_host_metadata_received;
        if (_host_metadata) {
          _host_metadata->set(
              std::static_pointer_cast<neb::pb_agent_host_info>(d)->obj());
        }
        ++_acknowledged;
        break;
      }
      default:
        /* acknowledge immediately. */
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

/**
 * @brief Send the current batch if max_send_interval seconds have passed since
 * the last send.
 *
 * @return number of events to acknowledge
 */
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

/**
 * @brief Send the current batch whatever its age, without waiting for the
 * export to complete.
 *
 * @return number of events to acknowledge
 */
int32_t stream::stop() {
  SPDLOG_LOGGER_TRACE(_logger, "OTPL: stream Try to stop");
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

/**
 * @brief Fill the stream statistics: batches and datapoints sent, export
 * errors, statuses dropped for an unknown host name, host information received
 * and known, in-flight exports and pending datapoints.
 *
 * @param tree
 */
void stream::statistics(nlohmann::json& tree) const {
  std::lock_guard<std::mutex> l(_protect);
  tree["batches_sent"] = _stat_batches_sent;
  tree["datapoints_sent"] = _stat_datapoints_sent;
  tree["export_errors"] = _stat_export_errors;
  tree["dropped_no_host_name"] = _stat_dropped_no_host_name;
  tree["host_metadata_received"] = _stat_host_metadata_received;
  if (_host_metadata)
    tree["host_metadata_known"] = _host_metadata->size();
  tree["inflight_requests"] = _inflight;
  tree["pending_datapoints"] = _builder->nb_data();
}

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

#include "com/centreon/broker/otlp/request_builder.hh"
#include "com/centreon/broker/otlp/semconv_mapping.hh"
#include "com/centreon/common/perfdata.hh"

using namespace com::centreon::broker::otlp;
using com::centreon::common::perfdata;

namespace otel_metrics = ::opentelemetry::proto::metrics::v1;
namespace otel_common = ::opentelemetry::proto::common::v1;

namespace {

constexpr const char* k_scope_name = "com.centreon.broker.otlp";
constexpr const char* k_service_name = "centreon-broker";
constexpr const char* k_service_namespace = "centreon";

void set_attribute(otel_common::KeyValue* kv,
                   std::string_view key,
                   std::string_view value) {
  kv->set_key(std::string(key));
  kv->mutable_value()->set_string_value(std::string(value));
}

void set_attribute(otel_common::KeyValue* kv,
                   std::string_view key,
                   int64_t value) {
  kv->set_key(std::string(key));
  kv->mutable_value()->set_int_value(value);
}

/* Broker timestamps are seconds; OTLP wants nanoseconds. */
uint64_t to_unix_nano(int64_t seconds) {
  return static_cast<uint64_t>(seconds) * 1000000000ULL;
}

}  // namespace

request_builder::request_builder(
    const otlp_config::pointer& conf,
    const std::shared_ptr<resource_enricher>& enricher,
    const mapping_provider::pointer& mapping,
    const std::shared_ptr<spdlog::logger>& logger)
    : _conf(conf), _enricher(enricher), _mapping(mapping), _logger(logger) {}

request_builder::ScopeMetrics* request_builder::_scope_for_host(
    uint64_t host_id,
    const std::string& host_name) {
  auto found = _scope_by_host.find(host_id);
  if (found != _scope_by_host.end())
    return found->second;

  SPDLOG_LOGGER_TRACE(_logger,
                      "create resource metrics for Host id:{} and add to scope",
                      host_id);

  ResourceMetrics* rm = _request.add_resource_metrics();
  auto* resource = rm->mutable_resource();

  /* host.name is the correlation key; everything else on the
   * resource describes the emitter, per the OTel definition of service.*. */
  set_attribute(resource->add_attributes(), "host.name", host_name);
  set_attribute(resource->add_attributes(), "service.name", k_service_name);
  set_attribute(resource->add_attributes(), "service.namespace",
                k_service_namespace);
  set_attribute(resource->add_attributes(), "service.version",
                CENTREON_BROKER_VERSION);
  /* No uuid exists in today, so the Centreon id is kept as a fallback identity.
   */
  set_attribute(resource->add_attributes(), "centreon.host.id",
                static_cast<int64_t>(host_id));

  ScopeMetrics* sm = rm->add_scope_metrics();
  sm->mutable_scope()->set_name(k_scope_name);
  sm->mutable_scope()->set_version(CENTREON_BROKER_VERSION);

  _scope_by_host.emplace(host_id, sm);
  return sm;
}

request_builder::Metric* request_builder::_metric_for(
    uint64_t host_id,
    const std::string& host_name,
    const std::string& name,
    const std::string& unit,
    instrument instr) {
  auto key = std::make_pair(host_id, name);
  auto found = _metric_index.find(key);
  if (found != _metric_index.end())
    return found->second;

  ScopeMetrics* sm = _scope_for_host(host_id, host_name);
  Metric* m = sm->add_metrics();
  m->set_name(name);
  m->set_unit(unit);
  switch (instr) {
    case instrument::sum_monotonic:
      // additive quantity whose increments can only be positive
      m->mutable_sum()->set_is_monotonic(true);
      m->mutable_sum()->set_aggregation_temporality(
          otel_metrics::
              AGGREGATION_TEMPORALITY_CUMULATIVE);  // cumulative temporality,
                                                    // each exported value
                                                    // means: the total
                                                    // accumulated since a start
                                                    // time.
      break;
    case instrument::sum_non_monotonic:
      // additive quantity whose increments can be positive or negative
      m->mutable_sum()->set_is_monotonic(false);
      m->mutable_sum()->set_aggregation_temporality(
          otel_metrics::
              AGGREGATION_TEMPORALITY_CUMULATIVE);  // cumulative temporality,
                                                    // each exported value
                                                    // means: the total
                                                    // accumulated since a start
                                                    // time.
      break;
    case instrument::gauge:
      m->mutable_gauge();
      break;
  }

  SPDLOG_LOGGER_TRACE(
      _logger, "metric {} created for host id:{} with the instrument = {}",
      name, host_id, instr == instrument::gauge ? "Gauge" : "SUM");

  _metric_index.emplace(std::move(key), m);
  return m;
}

request_builder::NumberDataPoint* request_builder::_new_point(
    Metric* m,
    instrument instr [[maybe_unused]]) {
  if (m->has_gauge())
    return m->mutable_gauge()->add_data_points();
  return m->mutable_sum()->add_data_points();
}

void request_builder::_add_perfdata(uint64_t host_id,
                                    const std::string& host_name,
                                    uint64_t service_id,
                                    const std::string& description,
                                    const std::string& perfdata_str,
                                    uint64_t ts) {

  auto tag_identity = [&](NumberDataPoint* dp) {
    if (!service_id)
      return;
    if (!description.empty())
      set_attribute(dp->add_attributes(), "centreon.service.description",
                    description);
    set_attribute(dp->add_attributes(), "centreon.service.id",
                  static_cast<int64_t>(service_id));
  };

  std::list<perfdata> parsed = perfdata::parse_perfdata(
      host_id, service_id, perfdata_str.c_str(), _logger);

  /* One snapshot for the all status, so a reload in the middle cannot modifier
   * the map mapping_provider swapping tables and table is shared pointer */
  const mapping_table::pointer table = _mapping->get();
  for (const perfdata& pd : parsed) {
    const mapping map =
        map_metric(pd.name(), pd.unit(), pd.value_type(), *table);

    Metric* m = _metric_for(host_id, host_name, map.name, map.unit, map.instr);
    NumberDataPoint* dp = _new_point(m, map.instr);
    dp->set_time_unix_nano(ts);
    dp->set_as_double(pd.value() * map.scale);
    tag_identity(dp);
    /* The raw label is always preserved so no information is lost by mapping
     * and operators can still find a metric by its Centreon name. */
    set_attribute(dp->add_attributes(), "centreon.metric.name", pd.name());
    for (const auto& [k, v] : map.attributes)
      set_attribute(dp->add_attributes(), k, v);
    ++_nb_data;

  }
}

bool request_builder::add_service_status(const ServiceStatus& status) {
  std::optional<std::string> host_name = _enricher->host_name(status.host_id());
  if (!host_name) {
    ++_dropped_no_host_name;
    SPDLOG_LOGGER_DEBUG(_logger,
                        "no host name for host_id {}, dropping service {}",
                        status.host_id(), status.service_id());
    return false;
  }

  std::string description;
  if (auto d =
          _enricher->service_description(status.host_id(), status.service_id()))
    description = std::move(*d);

  const uint64_t ts = to_unix_nano(status.last_check());

  _add_perfdata(status.host_id(), *host_name, status.service_id(), description,
                status.perfdata(), ts);

  if (_conf->send_status) {
    /* One unitless enum for every check, so unlike thresholds a single metric
     * name is correct here. */
    Metric* sm = _metric_for(status.host_id(), *host_name,
                             "centreon.check.state", "1", instrument::gauge);
    NumberDataPoint* sdp = _new_point(sm, instrument::gauge);
    sdp->set_time_unix_nano(ts);
    sdp->set_as_double(static_cast<double>(status.state()));
    if (!description.empty())
      set_attribute(sdp->add_attributes(), "centreon.service.description",
                    description);
    set_attribute(sdp->add_attributes(), "centreon.service.id",
                  static_cast<int64_t>(status.service_id()));
    set_attribute(sdp->add_attributes(), "centreon.state.type",
                  status.state_type() == ServiceStatus::HARD ? "hard" : "soft");
    ++_nb_data;
  }

  return true;
}

bool request_builder::add_host_status(const HostStatus& status) {
  if (!_conf->send_status)
    return true;

  std::optional<std::string> host_name = _enricher->host_name(status.host_id());
  if (!host_name) {
    ++_dropped_no_host_name;
    SPDLOG_LOGGER_DEBUG(_logger, "no host name for host_id {}, dropping it",
                        status.host_id());
    return false;
  }

  const uint64_t ts = to_unix_nano(status.last_check());

  /* service_id 0: the host check itself */
  _add_perfdata(status.host_id(), *host_name, 0, {}, status.perfdata(), ts);

  if (_conf->send_status) {
    Metric* m = _metric_for(status.host_id(), *host_name,
                            "centreon.host.state", "1", instrument::gauge);
    NumberDataPoint* dp = _new_point(m, instrument::gauge);
    dp->set_time_unix_nano(ts);
    dp->set_as_double(static_cast<double>(status.state()));
    set_attribute(dp->add_attributes(), "centreon.state.type",
                  status.state_type() == HostStatus::HARD ? "hard" : "soft");
    ++_nb_data;
  }
  return true;
}

request_builder::ExportRequest request_builder::take() {
  ExportRequest out;
  out.Swap(&_request);
  _scope_by_host.clear();
  _metric_index.clear();
  _nb_data = 0;
  return out;
}

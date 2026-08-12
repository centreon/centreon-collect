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

/* Semantic convention version the mapping table targets. Consumers use it to
 * migrate attribute names automatically across semconv releases, so it must be
 * bumped whenever the table adopts conventions from a newer one. */
constexpr const char* k_schema_url = "https://opentelemetry.io/schemas/1.27.0";

/* State vocabularies for the one_hot encoding, indexed by the enum value.
 *
 * Both include "pending": engine reports 4 for a check that has never run
 * (engine/src/broker.cc, `has_been_checked() ? get_current_state() : 4`).
 * HostStatus::State does not even declare 4, but engine casts it in anyway and
 * proto3 open enums let it through, so a consumer must handle it. Index 3 is
 * unused for hosts, hence the empty slot. */
constexpr const char* k_service_states[] = {"ok", "warning", "critical",
                                            "unknown", "pending"};
constexpr const char* k_host_states[] = {"up", "down", "unreachable", "",
                                         "pending"};
constexpr int k_nb_states = 5;

const char* state_name(int state, bool is_host) {
  if (state < 0 || state >= k_nb_states)
    return "";
  return is_host ? k_host_states[state] : k_service_states[state];
}

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
    const std::shared_ptr<spdlog::logger>& logger)
    : _conf(conf),
      _enricher(enricher),
      _logger(logger),
      /* The plugin never tells us when its counter started, so the honest
       * answer is "when this exporter started observing". That is what lets a
       * consumer distinguish a counter reset from a first observation. */
      _start_time_unix_nano(to_unix_nano(std::time(nullptr))) {}

request_builder::ScopeMetrics* request_builder::_scope_for_host(
    uint64_t host_id,
    const std::string& host_name) {
  auto found = _scope_by_host.find(host_id);
  if (found != _scope_by_host.end())
    return found->second;

  ResourceMetrics* rm = _request.add_resource_metrics();
  auto* resource = rm->mutable_resource();

  /* host.name is the correlation key with CLM; everything else on the
   * resource describes the emitter, per the OTel definition of service.*. */
  set_attribute(resource->add_attributes(), "host.name", host_name);
  set_attribute(resource->add_attributes(), "service.name", k_service_name);
  set_attribute(resource->add_attributes(), "service.namespace",
                k_service_namespace);
  set_attribute(resource->add_attributes(), "service.version",
                CENTREON_BROKER_VERSION);
  /* No host.id exists in CIM today, so the Centreon id is kept as a
   * vendor-namespaced fallback identity. */
  set_attribute(resource->add_attributes(), "centreon.host.id",
                static_cast<int64_t>(host_id));

  rm->set_schema_url(k_schema_url);

  ScopeMetrics* sm = rm->add_scope_metrics();
  sm->mutable_scope()->set_name(k_scope_name);
  sm->mutable_scope()->set_version(CENTREON_BROKER_VERSION);
  sm->set_schema_url(k_schema_url);

  _scope_by_host.emplace(host_id, sm);
  return sm;
}

request_builder::Metric* request_builder::_metric_for(
    uint64_t host_id,
    const std::string& host_name,
    const std::string& name,
    const std::string& unit,
    std::string_view description,
    instrument instr) {
  auto key = std::make_pair(host_id, name);
  auto found = _metric_index.find(key);
  if (found != _metric_index.end())
    return found->second;

  ScopeMetrics* sm = _scope_for_host(host_id, host_name);
  Metric* m = sm->add_metrics();
  m->set_name(name);
  m->set_unit(unit);
  if (!description.empty())
    m->set_description(std::string(description));
  switch (instr) {
    case instrument::sum_monotonic:
      m->mutable_sum()->set_is_monotonic(true);
      m->mutable_sum()->set_aggregation_temporality(
          otel_metrics::AGGREGATION_TEMPORALITY_CUMULATIVE);
      break;
    case instrument::sum_non_monotonic:
      m->mutable_sum()->set_is_monotonic(false);
      m->mutable_sum()->set_aggregation_temporality(
          otel_metrics::AGGREGATION_TEMPORALITY_CUMULATIVE);
      break;
    case instrument::gauge:
      m->mutable_gauge();
      break;
  }

  _metric_index.emplace(std::move(key), m);
  return m;
}

request_builder::NumberDataPoint* request_builder::_new_point(
    Metric* m,
    instrument instr [[maybe_unused]]) {
  /* Follow the instrument the Metric was created with rather than the one the
   * caller expects. gauge and sum share a protobuf oneof, so switching would
   * clear the field and silently discard every datapoint added so far. OTLP
   * also requires a single type per metric name within a scope, so the first
   * observation legitimately fixes it. */
  if (m->has_gauge())
    return m->mutable_gauge()->add_data_points();

  /* Cumulative sums carry a start time so a consumer can tell a counter reset
   * from a fresh series. Gauges have no aggregation window and must not. */
  NumberDataPoint* dp = m->mutable_sum()->add_data_points();
  dp->set_start_time_unix_nano(_start_time_unix_nano);
  return dp;
}

bool request_builder::add_service_status(const ServiceStatus& status) {
  std::optional<std::string> host_name =
      _enricher->host_name(status.host_id());
  if (!host_name) {
    /* Without host.name the series cannot be correlated and would pollute the
     * backend with unattributable data. Drop the datapoints but let the caller
     * acknowledge the event: a host that is never resolvable would otherwise
     * stall the pipeline forever. */
    ++_dropped_no_host_name;
    SPDLOG_LOGGER_DEBUG(
        _logger, "otlp: no host name for host_id {}, dropping service {}",
        status.host_id(), status.service_id());
    return false;
  }

  std::string description;
  if (auto d = _enricher->service_description(status.host_id(),
                                              status.service_id()))
    description = std::move(*d);

  const uint64_t ts = to_unix_nano(status.last_check());

  /* Common identity carried on every datapoint this status produces. The
   * Centreon service lives here, never in service.name, which OTel reserves
   * for the emitting service and Prometheus turns into the job label. */
  auto tag_identity = [&](NumberDataPoint* dp) {
    if (!description.empty())
      set_attribute(dp->add_attributes(), "centreon.service.description",
                    description);
    set_attribute(dp->add_attributes(), "centreon.service.id",
                  static_cast<int64_t>(status.service_id()));
  };

  std::list<perfdata> parsed = perfdata::parse_perfdata(
      status.host_id(), status.service_id(), status.perfdata().c_str(),
      _logger);

  for (const perfdata& pd : parsed) {
    const mapping map =
        map_metric(pd.name(), pd.unit(), pd.value_type());

    Metric* m = _metric_for(status.host_id(), *host_name, map.name, map.unit,
                            map.description, map.instr);
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

    if (_conf->send_thresholds) {
      /* Created on first finite bound, so a perfdata without thresholds does
       * not leave an empty Metric in the payload. */
      Metric* tm = nullptr;
      auto add_threshold = [&](float bound, const char* level,
                               const char* which) {
        if (!std::isfinite(bound))
          return;
        if (!tm)
          /* Same unit as the value it annotates: a shared threshold metric
           * would mix bytes, ratios and seconds into one series name. */
          tm = _metric_for(status.host_id(), *host_name,
                           threshold_metric_name(map.name), map.unit,
                           absl::StrCat("Warning and critical bounds of ",
                                        map.name),
                           instrument::gauge);
        NumberDataPoint* tdp = _new_point(tm, instrument::gauge);
        tdp->set_time_unix_nano(ts);
        tdp->set_as_double(bound * map.scale);
        tag_identity(tdp);
        set_attribute(tdp->add_attributes(), "centreon.metric.name",
                      pd.name());
        set_attribute(tdp->add_attributes(), "centreon.threshold.level",
                      level);
        set_attribute(tdp->add_attributes(), "centreon.threshold.bound",
                      which);
        for (const auto& [k, v] : map.attributes)
          set_attribute(tdp->add_attributes(), k, v);
        ++_nb_data;
      };
      add_threshold(pd.warning(), "warning", "upper");
      add_threshold(pd.critical(), "critical", "upper");
      /* The lower bound of a bare "warn=500" is a constant zero, so it doubles
       * the threshold series for no information on the many checks that only
       * ever set an upper limit. */
      if (_conf->threshold_bounds == threshold_bound_mode::all) {
        add_threshold(pd.warning_low(), "warning", "lower");
        add_threshold(pd.critical_low(), "critical", "lower");
      }
    }

    if (_conf->send_min_max) {
      Metric* bm = nullptr;
      auto add_bound = [&](float bound, const char* which) {
        if (!std::isfinite(bound))
          return;
        if (!bm)
          bm = _metric_for(
              status.host_id(), *host_name, bound_metric_name(map.name),
              map.unit, absl::StrCat("Minimum and maximum of ", map.name),
              instrument::gauge);
        NumberDataPoint* bdp = _new_point(bm, instrument::gauge);
        bdp->set_time_unix_nano(ts);
        bdp->set_as_double(bound * map.scale);
        tag_identity(bdp);
        set_attribute(bdp->add_attributes(), "centreon.metric.name",
                      pd.name());
        set_attribute(bdp->add_attributes(), "centreon.bound.type", which);
        for (const auto& [k, v] : map.attributes)
          set_attribute(bdp->add_attributes(), k, v);
        ++_nb_data;
      };
      add_bound(pd.min(), "min");
      add_bound(pd.max(), "max");
    }
  }

  if (_conf->send_status)
    _add_state(status.host_id(), *host_name, ts, status.state(),
               status.state_type() == ServiceStatus::HARD, false, tag_identity);

  return true;
}

void request_builder::_add_state(
    uint64_t host_id,
    const std::string& host_name,
    uint64_t ts,
    int state,
    bool hard,
    bool is_host,
    const std::function<void(NumberDataPoint*)>& tag_identity) {
  const char* const metric = is_host ? "centreon.host.state"
                                     : "centreon.check.state";
  const char* const type_metric = is_host ? "centreon.host.state_type"
                                          : "centreon.check.state_type";
  const std::string_view desc =
      is_host ? "Host check state" : "Service check state";

  /* One unitless enum for every check, so unlike thresholds a single metric
   * name is correct here. */
  Metric* sm = _metric_for(host_id, host_name, metric, "1", desc,
                           instrument::gauge);

  if (_conf->state_encoding == state_encoding_mode::one_hot) {
    /* One datapoint per state, 1 for the current one and 0 for the others, so
     * that counting criticals is a sum rather than an equality test. Costs one
     * series per state instead of one per check. */
    for (int s = 0; s < k_nb_states; ++s) {
      const char* name = state_name(s, is_host);
      if (!*name)  // 3 is not a host state
        continue;
      NumberDataPoint* dp = _new_point(sm, instrument::gauge);
      dp->set_time_unix_nano(ts);
      dp->set_as_double(s == state ? 1.0 : 0.0);
      tag_identity(dp);
      set_attribute(dp->add_attributes(), "centreon.state", name);
      ++_nb_data;
    }
  } else {
    NumberDataPoint* dp = _new_point(sm, instrument::gauge);
    dp->set_time_unix_nano(ts);
    dp->set_as_double(static_cast<double>(state));
    tag_identity(dp);
    ++_nb_data;
  }

  /* hard/soft is its own metric, never an attribute of the state above: it
   * flips on every soft to hard transition, and an attribute that changes ends
   * one series and starts another, leaving the state timeline full of holes. */
  if (_conf->send_state_type) {
    Metric* tm = _metric_for(
        host_id, host_name, type_metric, "1",
        "Check state type, 1 when the state is hard and 0 when it is soft",
        instrument::gauge);
    NumberDataPoint* tdp = _new_point(tm, instrument::gauge);
    tdp->set_time_unix_nano(ts);
    tdp->set_as_double(hard ? 1.0 : 0.0);
    tag_identity(tdp);
    ++_nb_data;
  }
}

bool request_builder::add_host_status(const HostStatus& status) {
  if (!_conf->send_status)
    return true;

  std::optional<std::string> host_name =
      _enricher->host_name(status.host_id());
  if (!host_name) {
    ++_dropped_no_host_name;
    return false;
  }

  /* A host datapoint is identified by its resource alone, so there is no
   * service identity to tag on. */
  _add_state(status.host_id(), *host_name, to_unix_nano(status.last_check()),
             status.state(), status.state_type() == HostStatus::HARD, true,
             [](NumberDataPoint*) {});
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

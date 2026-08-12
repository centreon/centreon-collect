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

#ifndef CCB_OTLP_REQUEST_BUILDER_HH
#define CCB_OTLP_REQUEST_BUILDER_HH

#include <functional>

#include "bbdo/neb.pb.h"
#include "com/centreon/broker/otlp/otlp_config.hh"
#include "com/centreon/broker/otlp/resource_enricher.hh"
#include "com/centreon/broker/otlp/semconv_mapping.hh"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"

namespace com::centreon::broker::otlp {

/**
 * @brief Accumulates check results into an OTLP export request.
 *
 * One ResourceMetrics per host, which is what lets a backend join these
 * metrics with CLM logs on host.name. The Centreon service is carried as
 * datapoint attributes rather than a resource attribute so that grouping
 * holds.
 */
class request_builder {
  using ExportRequest = ::opentelemetry::proto::collector::metrics::v1::
      ExportMetricsServiceRequest;
  using ResourceMetrics = ::opentelemetry::proto::metrics::v1::ResourceMetrics;
  using ScopeMetrics = ::opentelemetry::proto::metrics::v1::ScopeMetrics;
  using Metric = ::opentelemetry::proto::metrics::v1::Metric;
  using NumberDataPoint = ::opentelemetry::proto::metrics::v1::NumberDataPoint;

  const otlp_config::pointer _conf;
  std::shared_ptr<resource_enricher> _enricher;
  std::shared_ptr<spdlog::logger> _logger;

  ExportRequest _request;
  /* host_id -> its ResourceMetrics in _request, so all of a host's series land
   * under one resource. Pointers stay valid: protobuf repeated message fields
   * hold their elements by pointer, so appending does not move them. */
  absl::flat_hash_map<uint64_t, ScopeMetrics*> _scope_by_host;
  /* (host_id, metric name) -> the Metric to append datapoints to, so repeated
   * checks on a host share one Metric entry rather than duplicating it. */
  absl::flat_hash_map<std::pair<uint64_t, std::string>, Metric*> _metric_index;

  uint64_t _nb_data = 0;
  uint64_t _dropped_no_host_name = 0;

  /* Start of the window over which cumulative sums accumulate, as far as this
   * exporter can know it. Fixed for the lifetime of the builder: it must not
   * move from batch to batch or every export would look like a counter reset.
   */
  const uint64_t _start_time_unix_nano;

  ScopeMetrics* _scope_for_host(uint64_t host_id,
                                const std::string& host_name);
  Metric* _metric_for(uint64_t host_id,
                      const std::string& host_name,
                      const std::string& name,
                      const std::string& unit,
                      std::string_view description,
                      instrument instr);
  NumberDataPoint* _new_point(Metric* m, instrument instr);
  /* Emits the state, in whichever encoding the configuration asks for, plus
   * the companion state_type metric. Shared by services and hosts, which
   * differ only in metric names and state vocabulary. */
  void _add_state(uint64_t host_id,
                  const std::string& host_name,
                  uint64_t ts,
                  int state,
                  bool hard,
                  bool is_host,
                  const std::function<void(NumberDataPoint*)>& tag_identity);

 public:
  request_builder(const otlp_config::pointer& conf,
                  const std::shared_ptr<resource_enricher>& enricher,
                  const std::shared_ptr<spdlog::logger>& logger);

  /**
   * @brief Parse a service status' perfdata and append everything it yields.
   *
   * @return false when the host name could not be resolved and the status was
   * therefore skipped.
   */
  bool add_service_status(const ServiceStatus& status);

  /**
   * @brief Append the host's check state.
   */
  bool add_host_status(const HostStatus& status);

  uint64_t nb_data() const { return _nb_data; }
  uint64_t dropped_no_host_name() const { return _dropped_no_host_name; }
  bool empty() const { return _nb_data == 0; }
  /* The value stamped on every cumulative sum datapoint. */
  uint64_t start_time_unix_nano() const { return _start_time_unix_nano; }

  /**
   * @brief Hand over the accumulated request and start a fresh one.
   */
  ExportRequest take();

  /* Test seam. */
  const ExportRequest& peek() const { return _request; }
};

}  // namespace com::centreon::broker::otlp

#endif  // !CCB_OTLP_REQUEST_BUILDER_HH

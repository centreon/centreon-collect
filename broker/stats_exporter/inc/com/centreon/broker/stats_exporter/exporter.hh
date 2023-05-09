/*
** Copyright 2023 Centreon
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

#ifndef CCB_STATS_EXPORTER_EXPORTER_HH
#define CCB_STATS_EXPORTER_EXPORTER_HH

#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/metrics/provider.h>

#include "broker.pb.h"
#include "com/centreon/broker/namespace.hh"

namespace metrics_api = opentelemetry::metrics;

CCB_BEGIN()

namespace stats_exporter {
/**
 * @brief Export stats to an opentelemetry collector.
 *
 */
class exporter {
  const std::string _url;

  /* Thread pool */
  opentelemetry::nostd::shared_ptr<metrics_api::ObservableInstrument> _thread_pool_size;
  opentelemetry::nostd::shared_ptr<metrics_api::ObservableInstrument> _thread_pool_latency;

 public:
  exporter(const std::string& url);
  ~exporter() noexcept = default;
};

}

CCB_END()

#endif /* !CCB_STATS_EXPORTER_EXPORTER_HH */


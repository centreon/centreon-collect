/*
** Copyright 2024 Centreon
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

#ifndef CCE_MOD_OTL_SERVER_METRIC_HH
#define CCE_MOD_OTL_SERVER_METRIC_HH

#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"

namespace com::centreon::engine::modules::otl_server {

using metric_ptr = std::shared_ptr<::opentelemetry::proto::collector::metrics::
                                       v1::ExportMetricsServiceRequest>;

/**
 * @brief some metrics will be computed and other not
 * This bean represents a metric, it embeds all ExportMetricsServiceRequest in
 * order to avoid useless copies
 *
 */
class metric {
  const metric_ptr _parent;
  const ::opentelemetry::proto::resource::v1::Resource& _resource;
  const ::opentelemetry::proto::common::v1::InstrumentationScope& _scope;
  const ::opentelemetry::proto::metrics::v1::Metric& _metric;

 public:
  metric(const metric_ptr& parent,
         const ::opentelemetry::proto::resource::v1::Resource& resource,
         const ::opentelemetry::proto::common::v1::InstrumentationScope& scope,
         const ::opentelemetry::proto::metrics::v1::Metric& metric)
      : _parent(parent), _resource(resource), _scope(scope), _metric(metric) {}

  const ::opentelemetry::proto::resource::v1::Resource& get_resource() const {
    return _resource;
  }

  const ::opentelemetry::proto::common::v1::InstrumentationScope& get_scope()
      const {
    return _scope;
  }

  const ::opentelemetry::proto::metrics::v1::Metric& get_metric() const {
    return _metric;
  }

  template <typename metric_handler>
  void extract_metrics(const metric_ptr& metrics, metric_handler&& handler);
};

template <typename metric_handler>
void metric::extract_metrics(const metric_ptr& metrics,
                             metric_handler&& handler) {
  for (const auto& resource_metric : metrics->resource_metrics()) {
    const ::opentelemetry::proto::resource::v1::Resource& resource =
        resource_metric.resource();
    for (const auto& scope_metrics : resource_metric.scope_metrics()) {
      const ::opentelemetry::proto::common::v1::InstrumentationScope& scope =
          scope_metrics.scope();
      for (const auto& pb_metric : scope_metrics.metrics()) {
        handler(metric(metrics, resource, scope, pb_metric));
      }
    }
  }
}

};  // namespace com::centreon::engine::modules::otl_server

#endif
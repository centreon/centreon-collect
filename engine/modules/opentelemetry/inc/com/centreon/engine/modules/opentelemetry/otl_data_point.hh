/**
 * Copyright 2024 Centreon
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

#ifndef CCE_MOD_OTL_DATA_POINT_HH
#define CCE_MOD_OTL_DATA_POINT_HH

namespace com::centreon::engine::modules::opentelemetry {

/**
 * @brief the goal of this little template is to pass an initializer at object
 * construction
 * Example:
 * @code {.c++}
 *   static initialized_data_class<po::options_description> desc(
 *    [](po::options_description& desc) {
 *      desc.add_options()("extractor", po::value<std::string>(),
 *                         "extractor type");
 *    });
 *
 * @endcode
 *
 *
 * @tparam data_class
 */
template <class data_class>
struct initialized_data_class : public data_class {
  template <typename initializer_type>
  initialized_data_class(initializer_type&& initializer) {
    initializer(*this);
  }
};

using metric_request_ptr =
    std::shared_ptr<::opentelemetry::proto::collector::metrics::v1::
                        ExportMetricsServiceRequest>;

/**
 * @brief the server grpc model used is the callback model
 * So you need to give to the server this handler to handle incoming requests
 *
 */
using metric_handler = std::function<void(const metric_request_ptr&)>;

/**
 * @brief some metrics will be computed and other not
 * This bean represents a DataPoint, it embeds all ExportMetricsServiceRequest
 * (_parent attribute) in order to avoid useless copies. Many attributes are
 * references to _parent attribute
 * As we can receive several types of otl_data_point, we tries to expose common
 * data by unique getters
 */
class otl_data_point {
 public:
  enum class data_point_type {
    number,
    histogram,
    exponential_histogram,
    summary
  };

 private:
  const metric_request_ptr _parent;
  const ::opentelemetry::proto::resource::v1::Resource& _resource;
  const ::opentelemetry::proto::common::v1::InstrumentationScope& _scope;
  const ::opentelemetry::proto::metrics::v1::Metric& _metric;
  const google::protobuf::Message& _data_point;
  const ::google::protobuf::RepeatedPtrField<
      ::opentelemetry::proto::common::v1::KeyValue>& _data_point_attributes;
  const ::google::protobuf::RepeatedPtrField<
      ::opentelemetry::proto::metrics::v1::Exemplar>& _exemplars;
  uint64_t _start_nano_timestamp;
  uint64_t _nano_timestamp;
  data_point_type _type;
  double _value;

  otl_data_point(
      const metric_request_ptr& parent,
      const ::opentelemetry::proto::resource::v1::Resource& resource,
      const ::opentelemetry::proto::common::v1::InstrumentationScope& scope,
      const ::opentelemetry::proto::metrics::v1::Metric& metric,
      const ::opentelemetry::proto::metrics::v1::NumberDataPoint& data_pt);

  otl_data_point(
      const metric_request_ptr& parent,
      const ::opentelemetry::proto::resource::v1::Resource& resource,
      const ::opentelemetry::proto::common::v1::InstrumentationScope& scope,
      const ::opentelemetry::proto::metrics::v1::Metric& metric,
      const ::opentelemetry::proto::metrics::v1::HistogramDataPoint& data_pt);

  otl_data_point(
      const metric_request_ptr& parent,
      const ::opentelemetry::proto::resource::v1::Resource& resource,
      const ::opentelemetry::proto::common::v1::InstrumentationScope& scope,
      const ::opentelemetry::proto::metrics::v1::Metric& metric,
      const ::opentelemetry::proto::metrics::v1::ExponentialHistogramDataPoint&
          data_pt);

  otl_data_point(
      const metric_request_ptr& parent,
      const ::opentelemetry::proto::resource::v1::Resource& resource,
      const ::opentelemetry::proto::common::v1::InstrumentationScope& scope,
      const ::opentelemetry::proto::metrics::v1::Metric& metric,
      const ::opentelemetry::proto::metrics::v1::SummaryDataPoint& data_pt);

 public:
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

  const google::protobuf::Message& get_data_point() const {
    return _data_point;
  }

  uint64_t get_start_nano_timestamp() const { return _start_nano_timestamp; }
  uint64_t get_nano_timestamp() const { return _nano_timestamp; }

  data_point_type get_type() { return _type; }

  const ::google::protobuf::RepeatedPtrField<
      ::opentelemetry::proto::common::v1::KeyValue>&
  get_data_point_attributes() const {
    return _data_point_attributes;
  }

  double get_value() const { return _value; }

  const ::google::protobuf::RepeatedPtrField<
      ::opentelemetry::proto::metrics::v1::Exemplar>&
  get_exemplars() const {
    return _exemplars;
  }

  template <typename data_point_handler>
  static void extract_data_points(const metric_request_ptr& metrics,
                                  data_point_handler&& handler);
};

/**
 * @brief extracts all data_points from an ExportMetricsServiceRequest object
 *
 * @tparam data_point_handler
 * @param metrics
 * @param handler called on every data point found
 */
template <typename data_point_handler>
void otl_data_point::extract_data_points(const metric_request_ptr& metrics,
                                         data_point_handler&& handler) {
  using namespace ::opentelemetry::proto::metrics::v1;
  for (const ResourceMetrics& resource_metric : metrics->resource_metrics()) {
    const ::opentelemetry::proto::resource::v1::Resource& resource =
        resource_metric.resource();
    for (const ScopeMetrics& scope_metrics : resource_metric.scope_metrics()) {
      const ::opentelemetry::proto::common::v1::InstrumentationScope& scope =
          scope_metrics.scope();

      for (const Metric& pb_metric : scope_metrics.metrics()) {
        switch (pb_metric.data_case()) {
          case Metric::kGauge:
            for (const NumberDataPoint& iter : pb_metric.gauge().data_points())
              handler(
                  otl_data_point(metrics, resource, scope, pb_metric, iter));
            break;
          case Metric::kSum:
            for (const NumberDataPoint& iter : pb_metric.sum().data_points())
              handler(
                  otl_data_point(metrics, resource, scope, pb_metric, iter));
            break;
          case Metric::kHistogram:
            for (const HistogramDataPoint& iter :
                 pb_metric.histogram().data_points())
              handler(
                  otl_data_point(metrics, resource, scope, pb_metric, iter));
            break;
          case Metric::kExponentialHistogram:
            for (const ExponentialHistogramDataPoint& iter :
                 pb_metric.exponential_histogram().data_points())
              handler(
                  otl_data_point(metrics, resource, scope, pb_metric, iter));
            break;
          case Metric::kSummary:
            for (const SummaryDataPoint& iter :
                 pb_metric.summary().data_points())
              handler(
                  otl_data_point(metrics, resource, scope, pb_metric, iter));
            break;
          default:
            break;
        }
      }
    }
  }
}

};  // namespace com::centreon::engine::modules::opentelemetry

#endif

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

#include "otl_data_point.hh"

using namespace com::centreon::engine::modules::opentelemetry;
using namespace ::opentelemetry::proto::metrics::v1;

/**
 * @brief SummaryDataPoint doesn't have Exemplars so we use it to return an
 * array of exemplars in any case
 *
 */
static const ::google::protobuf::RepeatedPtrField<
    ::opentelemetry::proto::metrics::v1::Exemplar>
    _empty_exemplars;

otl_data_point::otl_data_point(
    const metric_request_ptr& parent,
    const ::opentelemetry::proto::resource::v1::Resource& resource,
    const ::opentelemetry::proto::common::v1::InstrumentationScope& scope,
    const ::opentelemetry::proto::metrics::v1::Metric& metric,
    const ::opentelemetry::proto::metrics::v1::NumberDataPoint& data_pt)
    : _parent(parent),
      _resource(resource),
      _scope(scope),
      _metric(metric),
      _data_point(data_pt),
      _data_point_attributes(data_pt.attributes()),
      _exemplars(data_pt.exemplars()),
      _nano_timestamp(data_pt.time_unix_nano()),
      _type(data_point_type::number) {
  _value = data_pt.as_double() ? data_pt.as_double() : data_pt.as_int();
}

otl_data_point::otl_data_point(
    const metric_request_ptr& parent,
    const ::opentelemetry::proto::resource::v1::Resource& resource,
    const ::opentelemetry::proto::common::v1::InstrumentationScope& scope,
    const ::opentelemetry::proto::metrics::v1::Metric& metric,
    const ::opentelemetry::proto::metrics::v1::HistogramDataPoint& data_pt)
    : _parent(parent),
      _resource(resource),
      _scope(scope),
      _metric(metric),
      _data_point(data_pt),
      _data_point_attributes(data_pt.attributes()),
      _exemplars(data_pt.exemplars()),
      _nano_timestamp(data_pt.time_unix_nano()),
      _type(data_point_type::histogram) {
  _value = data_pt.count();
}

otl_data_point::otl_data_point(
    const metric_request_ptr& parent,
    const ::opentelemetry::proto::resource::v1::Resource& resource,
    const ::opentelemetry::proto::common::v1::InstrumentationScope& scope,
    const ::opentelemetry::proto::metrics::v1::Metric& metric,
    const ::opentelemetry::proto::metrics::v1::ExponentialHistogramDataPoint&
        data_pt)
    : _parent(parent),
      _resource(resource),
      _scope(scope),
      _metric(metric),
      _data_point(data_pt),
      _data_point_attributes(data_pt.attributes()),
      _exemplars(data_pt.exemplars()),
      _nano_timestamp(data_pt.time_unix_nano()),
      _type(data_point_type::exponential_histogram) {
  _value = data_pt.count();
}

otl_data_point::otl_data_point(
    const metric_request_ptr& parent,
    const ::opentelemetry::proto::resource::v1::Resource& resource,
    const ::opentelemetry::proto::common::v1::InstrumentationScope& scope,
    const ::opentelemetry::proto::metrics::v1::Metric& metric,
    const ::opentelemetry::proto::metrics::v1::SummaryDataPoint& data_pt)
    : _parent(parent),
      _resource(resource),
      _scope(scope),
      _metric(metric),
      _data_point(data_pt),
      _data_point_attributes(data_pt.attributes()),
      _exemplars(_empty_exemplars),
      _nano_timestamp(data_pt.time_unix_nano()),
      _type(data_point_type::summary) {
  _value = data_pt.count();
}

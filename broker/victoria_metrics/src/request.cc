/**
 * Copyright 2022-2024 Centreon
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

#include "com/centreon/broker/victoria_metrics/request.hh"
#include "bbdo/storage/metric.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/cache/global_cache.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::victoria_metrics;

/**
 * @brief a little filter use for string labels
 * it escape , ", space and \
 *
 * @param to_filter
 * @return std::string result
 */
template <class string_class>
static std::string string_filter(const string_class& to_filter) {
  std::string ret;
  ret.reserve(to_filter.length());
  for (char c : to_filter) {
    if (c == ',') {
      ret += "\\,";
    } else if (c == '"') {
      ret += "\\\"";
    } else if (c == ' ') {
      ret += "\\ ";
    } else if (c == '\\') {
      ret += "\\\\";
    } else {
      ret.push_back(c);
    }
  }
  return ret;
}

request::request(boost::beast::http::verb method,
                 const std::string& server_name,
                 boost::beast::string_view target,
                 const std::shared_ptr<spdlog::logger>& logger,
                 unsigned size_to_reserve,
                 const http_tsdb::line_protocol_query& metric_formatter,
                 const http_tsdb::line_protocol_query& status_formatter,
                 const std::string& authorization)
    : http_tsdb::request(method, server_name, target),
      _logger{logger},
      _metric_formatter(metric_formatter),
      _status_formatter(status_formatter) {
  body().reserve(size_to_reserve);
  set(boost::beast::http::field::authorization, authorization);
}

static constexpr std::string_view _sz_metric = "metric,id=";
static constexpr std::string_view _sz_status = "status,id=";
static constexpr std::string_view _sz_name = ",name=";
static constexpr std::string_view _sz_unit = ",unit=";
static constexpr std::string_view _sz_host_id = ",host_id=";
static constexpr std::string_view _sz_serv_id = ",serv_id=";
static constexpr std::string_view _sz_severity_id = ",severity_id=";
static constexpr std::string_view _sz_val = " val=";

void request::add_metric(const storage::pb_metric& metric) {
  absl::StrAppend(&body(), _sz_metric, metric.obj().metric_id());
  absl::StrAppend(&body(), _sz_name, string_filter(metric.obj().name()));
  absl::StrAppend(&body(), _sz_host_id, metric.obj().host_id(), _sz_serv_id,
                  metric.obj().service_id());

  append_metric_info(metric.obj());
  _metric_formatter.append_metric(metric, body());
  absl::StrAppend(&body(), _sz_val, metric.obj().value());
  body().push_back(' ');
  absl::StrAppend(&body(), metric.obj().time());
  body().push_back('\n');
  ++_nb_metric;
}

void request::add_status(const storage::pb_status& status) {
  const Status status_obj = status.obj();
  if (status_obj.state() > 2) {
    if (status_obj.state() !=
        3) {  // we don't write unknown but it's not an error
      SPDLOG_LOGGER_ERROR(_logger, "unknown state: {}", status_obj.state());
    }
    ++_nb_status;
    return;
  }

  absl::StrAppend(&body(), _sz_status, status_obj.index_id());
  append_status_info(status_obj);
  _status_formatter.append_status(status, body());
  switch (status_obj.state()) {
    case 0:
      absl::StrAppend(&body(), _sz_val, 100);
      break;
    case 1:
      absl::StrAppend(&body(), _sz_val, 75);
      break;
    case 2:
      absl::StrAppend(&body(), _sz_val, 0);
      break;
  }
  body().push_back(' ');
  absl::StrAppend(&body(), status_obj.time());
  body().push_back('\n');
  ++_nb_status;
}

void request::append_metric_info(const Metric& metric) {
  cache::global_cache::lock l;
  const cache::metric_info* metric_inf =
      cache::global_cache::instance_ptr()->get_metric_info(metric.metric_id());
  if (metric_inf) {
    absl::StrAppend(&body(), _sz_unit, string_filter(metric_inf->unit));
    const cache::resource_info* res_info =
        cache::global_cache::instance_ptr()->get_service(metric.host_id(),
                                                         metric.service_id());
    if (res_info) {
      absl::StrAppend(&body(), _sz_severity_id, res_info->severity_id);
    }
  }
}

void request::append_status_info(const Status& status) {
  absl::StrAppend(&body(), _sz_host_id, status.host_id(), _sz_serv_id,
                  status.service_id());
  cache::global_cache::lock l;
  const cache::resource_info* res_info =
      cache::global_cache::instance_ptr()->get_service(status.host_id(),
                                                       status.service_id());
  if (res_info) {
    absl::StrAppend(&body(), _sz_severity_id, res_info->severity_id);
  }
}

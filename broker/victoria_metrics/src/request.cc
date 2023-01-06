/*
** Copyright 2022 Centreon
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

#include "com/centreon/broker/victoria_metrics/request.hh"
#include "bbdo/storage/metric.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::victoria_metrics;

request::request(boost::beast::http::verb method,
                 boost::beast::string_view target,
                 unsigned size_to_reserve,
                 const http_tsdb::line_protocol_query& metric_formatter,
                 const http_tsdb::line_protocol_query& status_formatter)
    : http_tsdb::request(method, target),
      _metric_formatter(metric_formatter),
      _status_formatter(status_formatter) {
  body().reserve(size_to_reserve);
}

static const absl::string_view _sz_metric = "metric";
static const absl::string_view _sz_status = "status";
static const absl::string_view _sz_space = " ";
static const absl::string_view _sz_value = " val=";

void request::add_metric(const storage::metric& metric) {
  absl::StrAppend(&body(), _sz_metric, metric.metric_id);
  //_metric_formatter.append_metric(metric, body());
  absl::StrAppend(&body(), _sz_value, metric.value);
  body().push_back(' ');
  absl::StrAppend(&body(), metric.time.get_time_t());
  body().push_back('\n');
  ++_nb_metric;
}

void request::add_metric(const Metric& metric) {
  absl::StrAppend(&body(), _sz_metric, metric.metric_id());
  //_metric_formatter.append_metric(metric, body());
  absl::StrAppend(&body(), _sz_value, metric.value());
  body().push_back(' ');
  absl::StrAppend(&body(), metric.time());
  body().push_back('\n');
  ++_nb_metric;
}

void request::add_status(const storage::status& status) {
  if (status.state < 0 || status.state > 2) {
    if (status.state != 3) {  // we don't write unknow but it's not an error
      SPDLOG_LOGGER_ERROR(log_v2::victoria_metrics(), "unknown state: {}",
                          status.state);
    }
    ++_nb_status;
    return;
  }
  absl::StrAppend(&body(), _sz_status, status.index_id);
  switch (status.state) {
    case 0:
      absl::StrAppend(&body(), _sz_value, 100);
      break;
    case 1:
      absl::StrAppend(&body(), _sz_value, 75);
      break;
    case 2:
      absl::StrAppend(&body(), _sz_value, 0);
      break;
  }
  body().push_back(' ');
  absl::StrAppend(&body(), status.time.get_time_t());
  body().push_back('\n');
  ++_nb_status;
}

void request::add_status(const Status& status) {
  if (status.state() < 0 || status.state() > 2) {
    if (status.state() != 3) {  // we don't write unknow but it's not an error
      SPDLOG_LOGGER_ERROR(log_v2::victoria_metrics(), "unknown state: {}",
                          status.state());
    }
    ++_nb_status;
    return;
  }

  absl::StrAppend(&body(), _sz_status, status.index_id());

  switch (status.state()) {
    case 0:
      absl::StrAppend(&body(), _sz_value, 100);
      break;
    case 1:
      absl::StrAppend(&body(), _sz_value, 75);
      break;
    case 2:
      absl::StrAppend(&body(), _sz_value, 0);
      break;
  }
  body().push_back(' ');
  absl::StrAppend(&body(), status.time());
  body().push_back('\n');
  ++_nb_status;
}

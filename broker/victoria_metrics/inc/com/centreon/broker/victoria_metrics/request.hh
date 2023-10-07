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

#ifndef CCB_VICTORIA_METRICS_REQUEST_HH
#define CCB_VICTORIA_METRICS_REQUEST_HH

#include "com/centreon/broker/http_tsdb/http_tsdb_config.hh"
#include "com/centreon/broker/http_tsdb/line_protocol_query.hh"
#include "com/centreon/broker/http_tsdb/stream.hh"

namespace com::centreon::broker {

namespace victoria_metrics {
class request : public http_tsdb::request {
  std::shared_ptr<spdlog::logger> _logger;
  const http_tsdb::line_protocol_query& _metric_formatter;
  const http_tsdb::line_protocol_query& _status_formatter;

  void append_metric_info(const Metric& metric);

  void append_status_info(const Status& status);

 public:
  request(boost::beast::http::verb method,
          const std::string& server_name,
          boost::beast::string_view target,
          const std::shared_ptr<spdlog::logger>& logger,
          unsigned size_to_reserve,
          const http_tsdb::line_protocol_query& metric_formatter,
          const http_tsdb::line_protocol_query& status_formatter,
          const std::string& authorization = "");

  virtual void add_metric(const storage::pb_metric& metric) override;

  virtual void add_status(const storage::pb_status& status) override;
};

}  // namespace victoria_metrics

}  // namespace com::centreon::broker

#endif  // !CCB_VICTORIA_METRICS_REQUEST_HH

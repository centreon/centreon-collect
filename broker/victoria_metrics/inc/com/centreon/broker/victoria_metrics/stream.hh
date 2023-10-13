/**
 * Copyright 2022-2023 Centreon
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

#ifndef CCB_VICTORIA_METRICS_STREAM_HH
#define CCB_VICTORIA_METRICS_STREAM_HH

#include "com/centreon/broker/http_tsdb/line_protocol_query.hh"
#include "com/centreon/broker/http_tsdb/stream.hh"

namespace com::centreon::broker {

// Forward declaration.
class database_config;

namespace victoria_metrics {

class stream : public http_tsdb::stream {
  unsigned _body_size_to_reserve;

  http_tsdb::line_protocol_query _metric_formatter;
  http_tsdb::line_protocol_query _status_formatter;

  std::string _authorization;
  std::string _account_id;

 protected:
  stream(const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<http_tsdb::http_tsdb_config>& conf,
         const std::string& account_id,
         http_client::client::connection_creator conn_creator =
             http_client::http_connection::load);

  http_tsdb::request::pointer create_request() const override;

 public:
  static const std::string allowed_macros;

  static std::shared_ptr<stream> load(
      const std::shared_ptr<asio::io_context>& io_context,
      const std::shared_ptr<http_tsdb::http_tsdb_config>& conf,
      const std::string& account_id,
      http_client::client::connection_creator conn_creator =
          http_client::http_connection::load);

  const std::string& get_authorization() const { return _authorization; }
};

};  // namespace victoria_metrics

}  // namespace com::centreon::broker

#endif  // !CCB_VICTORIA_METRICS_STREAM_HH

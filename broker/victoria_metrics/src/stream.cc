/**
 * Copyright 2022-2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "com/centreon/broker/victoria_metrics/stream.hh"
#include "bbdo/storage/metric.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/victoria_metrics/request.hh"
#include "common/crypto/base64.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::victoria_metrics;
using log_v2 = com::centreon::common::log_v2::log_v2;

const std::string stream::allowed_macros =
    "$INSTANCE$,$INSTANCEID$,$HOST$,$SERVICE$,$HOSTGROUP$,$SERVICE_GROUP$,"
    "$RESOURCEID$,"
    "$HOST_TAG_CAT_ID$,$HOST_TAG_GROUP_ID$,$HOST_TAG_CAT_NAME$,"
    "$HOST_TAG_GROUP_NAME$,$SERV_TAG_CAT_ID$,$SERV_TAG_GROUP_ID$,"
    "$SERV_TAG_CAT_NAME$,$SERV_TAG_GROUP_NAME$,$MIN$,$MAX$";

stream::stream(const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<http_tsdb::http_tsdb_config>& conf,
               const std::string& account_id,
               http::connection_creator conn_creator)
    : http_tsdb::stream("victoria_metrics",
                        io_context,
                        log_v2::instance().get(log_v2::VICTORIA_METRICS),
                        conf,
                        conn_creator),
      _metric_formatter(allowed_macros,
                        conf->get_metric_columns(),
                        http_tsdb::line_protocol_query::data_type::metric,
                        _logger),
      _status_formatter(allowed_macros,
                        conf->get_status_columns(),
                        http_tsdb::line_protocol_query::data_type::status,
                        _logger),
      _account_id(account_id) {
  // in order to avoid reallocation of request body
  _body_size_to_reserve = conf->get_max_queries_per_transaction() *
                          (128 + std::max(conf->get_metric_columns().size(),
                                          conf->get_status_columns().size()) *
                                     20);

  _authorization = "Basic ";
  _authorization +=
      common::crypto::base64_encode(conf->get_user() + ':' + conf->get_pwd());
}

std::shared_ptr<stream> stream::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<http_tsdb::http_tsdb_config>& conf,
    const std::string& account_id,
    http::connection_creator conn_creator) {
  return std::shared_ptr<stream>(
      new stream(io_context, conf, account_id, conn_creator));
}

http_tsdb::request::pointer stream::create_request() const {
  auto ret = std::make_shared<request>(
      boost::beast::http::verb::post, _conf->get_server_name(),
      _conf->get_http_target(), _logger, _body_size_to_reserve,
      _metric_formatter, _status_formatter, _authorization);

  ret->set(boost::beast::http::field::content_type, "text/plain");
  ret->set(boost::beast::http::field::accept, "application/json");
  ret->set(boost::beast::http::field::accept_encoding, "gzip");
  ret->set("account_id", _account_id);

  return ret;
}

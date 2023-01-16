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

#include "com/centreon/broker/victoria_metrics/stream.hh"
#include "bbdo/storage/metric.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/pool.hh"
#include "com/centreon/broker/victoria_metrics/request.hh"
#include "com/centreon/broker/victoria_metrics/victoria_config.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::victoria_metrics;

stream::stream(const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<victoria_config>& conf,
               const std::shared_ptr<persistent_cache>& cache,
               http_client::client::connection_creator conn_creator)
    : http_tsdb::stream("victoria_metrics",
                        io_context,
                        log_v2::victoria_metrics(),
                        conf,
                        cache,
                        conn_creator),
      _metric_formatter(conf->get_metric_columns(),
                        http_tsdb::line_protocol_query::data_type::metric,
                        cache,
                        log_v2::victoria_metrics()),
      _status_formatter(conf->get_status_columns(),
                        http_tsdb::line_protocol_query::data_type::status,
                        cache,
                        log_v2::victoria_metrics()) {
  // in order to avoid reallocation of request body
  _body_size_to_reserve = conf->get_max_queries_per_transaction() *
                          (128 + std::max(conf->get_metric_columns().size(),
                                          conf->get_status_columns().size()) *
                                     20);

  char hostname[1024];
  hostname[sizeof(hostname) - 1] = 0;
  gethostname(hostname, sizeof(hostname) - 1);
  _hostname = hostname;

  _authorization = "Basic ";
  _authorization +=
      misc::string::base64_encode(conf->get_user() + ':' + conf->get_pwd());
}

std::shared_ptr<stream> stream::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<victoria_config>& conf,
    const std::shared_ptr<persistent_cache>& cache,
    http_client::client::connection_creator conn_creator) {
  return std::shared_ptr<stream>(
      new stream(io_context, conf, cache, conn_creator));
}

http_tsdb::request::pointer stream::create_request() const {
  auto ret = std::make_shared<request>(
      boost::beast::http::verb::post,
      std::static_pointer_cast<victoria_config>(_conf)->get_http_target(),
      _body_size_to_reserve, _metric_formatter, _status_formatter,
      _authorization);

  ret->set(boost::beast::http::field::host, _hostname);
  ret->set(boost::beast::http::field::content_type, "text/plain");
  ret->set(boost::beast::http::field::accept, "application/json");
  ret->set(boost::beast::http::field::accept_encoding, "gzip");

  return ret;
}
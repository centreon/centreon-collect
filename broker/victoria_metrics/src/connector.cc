/**
* Copyright 2022 Centreon
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

#include "com/centreon/broker/victoria_metrics/connector.hh"
#include "com/centreon/broker/http_client/https_connection.hh"
#include "com/centreon/broker/pool.hh"
#include "com/centreon/broker/victoria_metrics/stream.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::victoria_metrics;

static constexpr multiplexing::muxer_filter _victoria_stream_filter = {
    storage::metric::static_type(), storage::status::static_type(),
    storage::pb_metric::static_type(), storage::pb_status::static_type()};

connector::connector(const std::shared_ptr<http_tsdb::http_tsdb_config>& conf,
                     const std::string& account_id)
    : io::endpoint(false, _victoria_stream_filter),
      _conf(conf),
      _account_id(account_id) {}

std::shared_ptr<io::stream> connector::open() {
  if (!_conf->is_crypted()) {
    return stream::load(pool::io_context_ptr(), _conf, _account_id,
                        http_client::http_connection::load);
  } else {
    return stream::load(pool::io_context_ptr(), _conf, _account_id,
                        http_client::https_connection::load);
  }
}
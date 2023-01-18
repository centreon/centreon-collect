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

#include "com/centreon/broker/victoria_metrics/connector.hh"
#include "com/centreon/broker/http_client/https_connection.hh"
#include "com/centreon/broker/pool.hh"
#include "com/centreon/broker/victoria_metrics/stream.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::victoria_metrics;

connector::connector(const std::shared_ptr<http_tsdb::http_tsdb_config>& conf,
                     const std::shared_ptr<persistent_cache>& cache)
    : io::endpoint(false), _conf(conf), _cache(cache) {}

std::unique_ptr<io::stream> connector::open() {
  std::shared_ptr<stream> s;
  if (!_conf->is_crypted()) {
    s = stream::load(pool::io_context_ptr(), _conf, _cache,
                     http_client::http_connection::load);
  } else {
    s = stream::load(pool::io_context_ptr(), _conf, _cache,
                     http_client::https_connection::load);
  }

  return std::make_unique<stream_unique_wrapper>(s);
}
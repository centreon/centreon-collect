/**
 * Copyright 2011-2017, 2021 Centreon
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

#include "com/centreon/broker/influxdb/connector.hh"
#include "bbdo/storage/index_mapping.hh"
#include "bbdo/storage/metric_mapping.hh"
#include "com/centreon/broker/influxdb/internal.hh"
#include "com/centreon/broker/influxdb/stream.hh"
#include "com/centreon/broker/neb/host.hh"
#include "com/centreon/broker/neb/instance.hh"
#include "com/centreon/broker/neb/service.hh"
#include "com/centreon/broker/persistent_cache.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::influxdb;

static constexpr multiplexing::muxer_filter _influxdb_stream_filter = {
    storage::metric::static_type(), storage::status::static_type(),
    storage::pb_metric::static_type(), storage::pb_status::static_type(),
    // cache events
    neb::instance::static_type(), neb::pb_instance::static_type(),
    neb::host::static_type(), neb::pb_host::static_type(),
    neb::service::static_type(), neb::pb_service::static_type(),
    storage::index_mapping::static_type(),
    storage::pb_index_mapping::static_type(),
    storage::metric_mapping::static_type(),
    storage::pb_metric_mapping::static_type(),
    make_type(io::extcmd, extcmd::de_pb_bench)};

static constexpr multiplexing::muxer_filter _influxdb_forbidden_filter =
    multiplexing::muxer_filter(_influxdb_stream_filter).reverse();

/**
 *  Default constructor.
 */
connector::connector()
    : io::endpoint(false, _influxdb_stream_filter, _influxdb_forbidden_filter) {
}

/**
 *  Set connection parameters.
 *
 */
void connector::connect_to(std::string const& user,
                           std::string const& passwd,
                           std::string const& addr,
                           unsigned short port,
                           std::string const& db,
                           uint32_t queries_per_transaction,
                           std::string const& status_ts,
                           std::vector<column> const& status_cols,
                           std::string const& metric_ts,
                           std::vector<column> const& metric_cols,
                           std::shared_ptr<persistent_cache> const& cache) {
  _user = user;
  _password = passwd;
  _addr = addr;
  _port = port, _db = db;
  _queries_per_transaction = queries_per_transaction;
  _status_ts = status_ts;
  _status_cols = status_cols;
  _metric_ts = metric_ts;
  _metric_cols = metric_cols;
  _cache = cache;
}

/**
 * @brief Connect to an influxdb DB.
 *
 * @return An Influxdb connection object.
 */
std::shared_ptr<io::stream> connector::open() {
  return std::unique_ptr<stream>(
      new stream(_user, _password, _addr, _port, _db, _queries_per_transaction,
                 _status_ts, _status_cols, _metric_ts, _metric_cols, _cache));
}

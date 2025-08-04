/**
 * Copyright 2015,2017 Centreon
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

#include "com/centreon/broker/graphite/connector.hh"
#include "bbdo/storage/index_mapping.hh"
#include "bbdo/storage/metric.hh"
#include "bbdo/storage/metric_mapping.hh"
#include "com/centreon/broker/graphite/internal.hh"
#include "com/centreon/broker/graphite/stream.hh"
#include "com/centreon/broker/neb/host.hh"
#include "com/centreon/broker/neb/instance.hh"
#include "com/centreon/broker/neb/service.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::graphite;

static constexpr multiplexing::muxer_filter _graphite_stream_filter = {
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

static constexpr multiplexing::muxer_filter _graphite_forbidden_filter =
    multiplexing::muxer_filter(_graphite_stream_filter).reverse();

/**
 *  Default constructor.
 */
connector::connector()
    : io::endpoint(false, _graphite_stream_filter, _graphite_forbidden_filter) {
}

/**
 *  Set connection parameters.
 *
 */
void connector::connect_to(std::string const& metric_naming,
                           std::string const& status_naming,
                           std::string const& escape_string,
                           std::string const& db_user,
                           std::string const& db_passwd,
                           std::string const& db_addr,
                           unsigned short db_port,
                           uint32_t queries_per_transaction,
                           std::shared_ptr<persistent_cache> const& cache) {
  _escape_string = escape_string;
  _metric_naming = metric_naming;
  _status_naming = status_naming;
  _user = db_user;
  _password = db_passwd;
  _addr = db_addr;
  _port = db_port, _queries_per_transaction = queries_per_transaction;
  _persistent_cache = cache;
}

/**
 *  Connect to a graphite DB.
 *
 *  @return Graphite connection object.
 */
std::shared_ptr<io::stream> connector::open() {
  return std::unique_ptr<stream>(new stream(
      _metric_naming, _status_naming, _escape_string, _user, _password, _addr,
      _port, _queries_per_transaction, _persistent_cache));
}

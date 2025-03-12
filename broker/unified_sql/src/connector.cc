/**
 * Copyright 2011-2015,2017 Centreon
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

#include "com/centreon/broker/unified_sql/connector.hh"
#include "common/log_v2/log_v2.hh"

#include "com/centreon/broker/unified_sql/stream.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::unified_sql;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 *  Default constructor.
 */
connector::connector()
    : io::endpoint(false,
                   stream::get_muxer_filter(),
                   stream::get_forbidden_filter()),
      _logger_sql(log_v2::instance().get(log_v2::SQL)),
      _logger_sto(log_v2::instance().get(log_v2::PERFDATA)) {}

/**
 *  Set connection parameters.
 *
 *  @param[in] rrd_len                 RRD unified_sql length.
 *  @param[in] interval_length         Length of a time unit.
 *  @param[in] store_in_data_bin       True to store performance data in
 *                                     the data_bin table.
 */
void connector::connect_to(const database_config& dbcfg,
                           uint32_t rrd_len,
                           uint32_t interval_length,
                           uint32_t loop_timeout,
                           uint32_t instance_timeout,
                           bool store_in_data_bin,
                           bool store_in_resources,
                           bool store_in_hosts_services) {
  _dbcfg = dbcfg;
  _rrd_len = rrd_len;
  _interval_length = interval_length;
  _loop_timeout = loop_timeout;
  _instance_timeout = instance_timeout;
  _store_in_data_bin = store_in_data_bin;
  _store_in_resources = store_in_resources;
  _store_in_hosts_services = store_in_hosts_services;
}

/**
 * @brief Open a connection object to the database.
 *
 * @return Storage connection object.
 */
std::shared_ptr<io::stream> connector::open() {
  return std::make_unique<stream>(
      _dbcfg, _rrd_len, _interval_length, _loop_timeout, _instance_timeout,
      _store_in_data_bin, _store_in_resources, _store_in_hosts_services,
      get_io_context(), _logger_sql, _logger_sto);
}

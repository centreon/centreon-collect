/*
** Copyright 2011-2013,2017 Centreon
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

#ifndef CCB_UNIFIED_SQL_CONNECTOR_HH
#define CCB_UNIFIED_SQL_CONNECTOR_HH

#include <memory>
#include "com/centreon/broker/io/endpoint.hh"
#include "com/centreon/broker/sql/database_config.hh"

namespace com::centreon::broker {

namespace unified_sql {
/**
 *  @class connector connector.hh "com/centreon/broker/unified_sql/connector.hh"
 *  @brief Connect to a database.
 *
 *  Send perfdata in a Centreon Storage database.
 */
class connector : public io::endpoint {
  database_config _dbcfg;
  uint32_t _interval_length;
  uint32_t _loop_timeout;
  uint32_t _instance_timeout;
  uint32_t _rrd_len;
  bool _store_in_data_bin;
  bool _store_in_resources;
  bool _store_in_hosts_services;

  std::shared_ptr<spdlog::logger> _logger_sql;
  std::shared_ptr<spdlog::logger> _logger_sto;

 public:
  connector();
  ~connector() noexcept = default;
  connector(const connector&) = delete;
  connector& operator=(const connector&) = delete;
  bool operator==(const connector& other) = delete;
  void connect_to(const database_config& dbcfg,
                  uint32_t rrd_len,
                  uint32_t interval_length,
                  uint32_t loop_timeout,
                  uint32_t instance_timeout,
                  bool store_in_data_bin = true,
                  bool store_in_resources = true,
                  bool store_in_hosts_services = true);
  std::shared_ptr<io::stream> open() override;
};
}  // namespace unified_sql

}  // namespace com::centreon::broker

#endif  // !CCB_UNIFIED_SQL_CONNECTOR_HH

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

#ifndef CCB_VICTORIA_METRICS_CONNECTOR_HH
#define CCB_VICTORIA_METRICS_CONNECTOR_HH

#include "com/centreon/broker/database_config.hh"
#include "com/centreon/broker/io/endpoint.hh"
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace victoria_metrics {

class connector : public io::endpoint {
 public:
  connector();
  ~connector();
  connector(const connector&) = delete;
  connector& operator=(const connector&) = delete;
  void connect_to(std::string const& user,
                  std::string const& passwd,
                  std::string const& addr,
                  unsigned short _port,
                  std::string const& db,
                  uint32_t queries_per_transaction,
                  std::string const& status_ts,
                  std::vector<column> const& status_cols,
                  std::string const& metric_ts,
                  std::vector<column> const& metric_cols,
                  std::shared_ptr<persistent_cache> const& cache);
  std::unique_ptr<io::stream> open() override;

 private:
  std::string _user;
  std::string _password;
  std::string _addr;
  unsigned short _port;
  std::string _db;
  uint32_t _queries_per_transaction;
  std::string _status_ts;
  std::vector<column> _status_cols;
  std::string _metric_ts;
  std::vector<column> _metric_cols;
  std::shared_ptr<persistent_cache> _cache;
};
}  // namespace victoria_metrics

CCB_END()

#endif  // !CCB_VICTORIA_METRICS_CONNECTOR_HH

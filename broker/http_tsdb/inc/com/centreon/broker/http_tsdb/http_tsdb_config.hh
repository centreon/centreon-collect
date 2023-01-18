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

#ifndef CCB_HTTP_TSDB_CONFIG_HH
#define CCB_HTTP_TSDB_CONFIG_HH

#include "column.hh"
#include "com/centreon/broker/http_client/http_config.hh"

CCB_BEGIN()

namespace http_tsdb {
class http_tsdb_config : public http_client::http_config {
  std::string _http_target;
  std::string _user;
  std::string _pwd;
  unsigned _max_queries_per_transaction;
  duration _max_send_interval;
  std::vector<column> _status_columns;
  std::vector<column> _metric_columns;

 public:
  http_tsdb_config(const http_client::http_config& http_conf,
                   const std::string& http_target,
                   const std::string& user,
                   const std::string& pwd,
                   unsigned max_queries_per_transaction,
                   duration max_send_interval,
                   const std::vector<column>& status_columns,
                   const std::vector<column>& metric_columns)
      : http_client::http_config(http_conf),
        _http_target(http_target),
        _user(user),
        _pwd(pwd),
        _max_queries_per_transaction(max_queries_per_transaction),
        _max_send_interval(max_send_interval),
        _status_columns(status_columns),
        _metric_columns(metric_columns) {}

  http_tsdb_config() : _max_queries_per_transaction(0) {}
  http_tsdb_config(const http_client::http_config& http_conf,
                   unsigned max_queries_per_transaction,
                   duration max_send_interval)
      : http_client::http_config(http_conf),
        _max_queries_per_transaction(max_queries_per_transaction),
        _max_send_interval(max_send_interval) {}

  const std::string& get_http_target() const { return _http_target; }
  const std::string& get_user() const { return _user; }
  const std::string& get_pwd() const { return _pwd; }

  unsigned get_max_queries_per_transaction() const {
    return _max_queries_per_transaction;
  };
  duration get_max_send_interval() const { return _max_send_interval; }
  const std::vector<column>& get_status_columns() { return _status_columns; }
  const std::vector<column>& get_metric_columns() { return _metric_columns; }
};
}  // namespace http_tsdb

CCB_END()

#endif

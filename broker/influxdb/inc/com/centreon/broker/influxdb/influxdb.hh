/**
 * Copyright 2015-2024 Centreon
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

#ifndef CCB_INFLUXDB_INFLUXDB_HH
#define CCB_INFLUXDB_INFLUXDB_HH

#include "com/centreon/broker/influxdb/influxdb.hh"
#include "com/centreon/broker/influxdb/line_protocol_query.hh"

namespace com::centreon::broker::influxdb {
/**
 *  @class influxdb influxdb.hh "com/centreon/broker/influxdb/influxdb.hh"
 *  @brief Influxdb connection/query manager.
 *
 *  This object manage connection and query to influxdb through the Lina
 *  API.
 */
class influxdb {
 public:
  influxdb(std::string const& user,
           std::string const& passwd,
           std::string const& addr,
           uint16_t port,
           std::string const& db,
           std::string const& status_ts,
           std::vector<column> const& status_cols,
           std::string const& metric_ts,
           std::vector<column> const& metric_cols,
           macro_cache const& cache,
           const std::shared_ptr<spdlog::logger>& logger);

  /**
   *  Destructor.
   */
  ~influxdb() noexcept = default;

  influxdb(influxdb const& f) = delete;
  influxdb& operator=(influxdb const& f) = delete;

  void clear();
  void write(storage::metric const& m);
  void write(storage::status const& s);
  void write(const storage::pb_metric& m);
  void write(const storage::pb_status& s);
  void commit();

 private:
  std::string _post_header;
  std::string _query;
  line_protocol_query _status_query;
  line_protocol_query _metric_query;

  asio::io_context _io_context;
  asio::ip::tcp::socket _socket;

  std::string _host;
  uint16_t _port;

  macro_cache const& _cache;

  /* Logger */
  std::shared_ptr<spdlog::logger> _logger;

  void _connect_socket();
  bool _check_answer_string(std::string const& ans,
                            const std::string& addr,
                            uint16_t port);
  void _create_queries(std::string const& user,
                       std::string const& passwd,
                       std::string const& db,
                       std::string const& status_ts,
                       std::vector<column> const& status_cols,
                       std::string const& metric_ts,
                       std::vector<column> const& metric_cols);
};
}  // namespace com::centreon::broker::influxdb

#endif  // !CCB_INFLUXDB_INFLUXDB12_HH

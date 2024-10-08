/**
 * Copyright 2011-2017 Centreon
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

#ifndef CCB_INFLUXDB_STREAM_HH
#define CCB_INFLUXDB_STREAM_HH

#include "com/centreon/broker/influxdb/influxdb.hh"

namespace com::centreon::broker {

// Forward declaration.
class database_config;

namespace influxdb {
/**
 *  @class stream stream.hh "com/centreon/broker/influxdb/stream.hh"
 *  @brief Influxdb stream.
 *
 *  Insert metrics into influxdb.
 */
class stream : public io::stream {
  // Database parameters
  const std::string _user;
  const std::string _password;
  const std::string _address;
  const std::string _db;
  uint32_t _queries_per_transaction;

  // Internal working members
  int _pending_queries;
  uint32_t _actual_query;
  bool _commit;

  // Status members
  std::string _status;
  mutable std::mutex _statusm;

  // Cache
  macro_cache _cache;

  /* Logger */
  std::shared_ptr<spdlog::logger> _logger;

  std::unique_ptr<influxdb> _influx_db;

 public:
  stream(std::string const& user,
         std::string const& passwd,
         std::string const& addr,
         unsigned short port,
         std::string const& db,
         uint32_t queries_per_transaction,
         std::string const& status_ts,
         std::vector<column> const& status_cols,
         std::string const& metric_ts,
         std::vector<column> const& metric_cols,
         std::shared_ptr<persistent_cache> const& cache);

  /**
   *  Destructor.
   */
  ~stream() noexcept = default;
  int flush() override;
  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  void statistics(nlohmann::json& tree) const override;
  int write(std::shared_ptr<io::data> const& d) override;
  int32_t stop() override;
};
}  // namespace influxdb

}  // namespace com::centreon::broker

#endif  // !CCB_INFLUXDB_STREAM_HH

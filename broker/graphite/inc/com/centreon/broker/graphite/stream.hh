/*
** Copyright 2015-2017 Centreon
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

#ifndef CCB_GRAPHITE_STREAM_HH
#define CCB_GRAPHITE_STREAM_HH

#include "bbdo/storage/metric.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/graphite/macro_cache.hh"
#include "com/centreon/broker/graphite/query.hh"
#include "com/centreon/broker/io/stream.hh"

namespace com::centreon::broker {

// Forward declaration.
class database_config;

namespace graphite {
/**
 *  @class stream stream.hh "com/centreon/broker/graphite/stream.hh"
 *  @brief Graphite stream.
 *
 *  Insert metrics/statuses into graphite.
 */
class stream : public io::stream {
  // Database parameters
  const std::string _metric_naming;
  const std::string _status_naming;
  const std::string _db_user;
  const std::string _db_password;
  const std::string _db_host;
  const unsigned short _db_port;
  uint32_t _queries_per_transaction;

  // Internal working members
  int _pending_queries;
  uint32_t _actual_query;
  bool _commit_flag;

  // Status members
  std::string _status;
  mutable std::mutex _statusm;

  // Cache
  macro_cache _cache;

  // Query
  query _metric_query;
  query _status_query;
  std::string _query;
  std::string _auth_query;
  asio::io_context _io_context;
  asio::ip::tcp::socket _socket;

  // Logger
  uint32_t _logger_id;

  // Process metric/status and generate query.
  bool _process_metric(storage::metric const& me);
  bool _process_status(storage::status const& st);
  bool _process_metric(storage::pb_metric const& me);
  bool _process_status(storage::pb_status const& st);

  void _commit();

 public:
  stream(std::string const& metric_naming,
         std::string const& status_naming,
         std::string const& escape_string,
         std::string const& db_user,
         std::string const& db_password,
         std::string const& db_host,
         unsigned short db_port,
         uint32_t queries_per_transaction,
         std::shared_ptr<persistent_cache> const& cache);
  ~stream();
  int32_t flush() override;
  int32_t stop() override;
  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  void statistics(nlohmann::json& tree) const override;
  int32_t write(std::shared_ptr<io::data> const& d) override;
};
}  // namespace graphite

}

#endif  // !CCB_GRAPHITE_STREAM_HH

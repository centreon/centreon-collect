/**
 * Copyright 2011-2021 Centreon
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

#ifndef CCB_STORAGE_STREAM_HH
#define CCB_STORAGE_STREAM_HH

#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/sql/mysql.hh"

namespace com::centreon::broker {

// Forward declaration.
class database_config;

namespace storage {
/**
 *  @class stream stream.hh "com/centreon/broker/storage/stream.hh"
 *  @brief Storage stream.
 *
 *  Handle perfdata and insert proper informations in index_data and
 *  metrics table of a centstorage DB.
 */
class stream : public io::stream {
  int32_t _pending_events;
  bool _stopped;
  std::shared_ptr<spdlog::logger> _logger_sql;
  std::shared_ptr<spdlog::logger> _logger_storage;
  struct index_info {
    std::string host_name;
    uint32_t index_id;
    bool locked;
    uint32_t rrd_retention;
    std::string service_description;
    bool special;
  };
  struct metric_info {
    bool locked;
    uint32_t metric_id;
    uint16_t type;
    float value;
    std::string unit_name;
    float warn;
    float warn_low;
    bool warn_mode;
    float crit;
    float crit_low;
    bool crit_mode;
    float min;
    float max;
  };
  struct metric_value {
    time_t c_time;
    uint32_t metric_id;
    short status;
    float value;
  };

  std::string _status;
  mutable std::mutex _statusm;
  std::shared_ptr<spdlog::logger> _logger;

  void _update_status(std::string const& status);

 public:
  stream(database_config const& dbcfg,
         uint32_t rrd_len,
         uint32_t interval_length,
         bool store_in_db = true);
  ~stream();
  stream(const stream&) = delete;
  stream& operator=(const stream&) = delete;
  int32_t stop() override;
  int32_t flush() override;
  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  void statistics(nlohmann::json& tree) const override;
  int32_t write(std::shared_ptr<io::data> const& d) override;
};
}  // namespace storage

}  // namespace com::centreon::broker

#endif  // !CCB_STORAGE_STREAM_HH

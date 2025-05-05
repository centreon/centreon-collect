/**
 * Copyright 2025 Centreon
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

#ifndef CCB_UNIFIED_SQL_DATABASE_CONFIGURATOR_HH
#define CCB_UNIFIED_SQL_DATABASE_CONFIGURATOR_HH
#include "com/centreon/broker/sql/mysql.hh"
#include "com/centreon/broker/unified_sql/stream.hh"
#include "common/engine_conf/state.pb.h"

using com::centreon::engine::configuration::DiffState;

namespace com::centreon::broker::unified_sql {
class database_configurator {
  const DiffState& _diff;
  stream* _stream;
  std::shared_ptr<spdlog::logger> _logger;
  database::mysql_stmt _enable_hosts;

  void _disable_pollers_with_full_conf();
  void _disable_hosts();

  absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t>
  _add_severities_mariadb(const ::google::protobuf::RepeatedPtrField<
                          engine::configuration::Severity>& lst);
  absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t>
  _add_severities_mysql(const ::google::protobuf::RepeatedPtrField<
                        engine::configuration::Severity>& lst);

  void _add_hosts_mariadb(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
          lst);
  void _add_hosts_mysql(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
          lst);
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t>
  _add_host_resources_mariadb(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
          lst);
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t>
  _add_host_resources_mysql(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
          lst);

 public:
  database_configurator(const DiffState& diff,
                        stream* stream,
                        const std::shared_ptr<spdlog::logger>& logger)
      : _diff(diff), _stream(stream), _logger(logger) {}

  database_configurator(const database_configurator&) = delete;

  void process();
};
}  // namespace com::centreon::broker::unified_sql

#endif /* !CCB_UNIFIED_SQL_DATABASE_CONFIGURATOR_HH */

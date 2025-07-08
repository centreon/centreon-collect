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
  std::unique_ptr<database::mysql_bulk_stmt>
      _add_anomalydetection_resources_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_anomalydetections_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_host_resources_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_hosts_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_service_resources_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_services_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_severities_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_tags_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_customvariables_stmt;
  std::unique_ptr<database::mysql_stmt_base> _disable_services_stmt;
  std::unique_ptr<database::mysql_stmt_base> _disable_service_resources_stmt;

  void _disable_pollers_with_full_conf();
  void _disable_hosts_and_services();

  void _add_severities_mariadb(
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::Severity>& lst,
      absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t>& cache);
  void _add_severities_mysql(
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::Severity>& lst,
      absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t>& cache);
  void _add_tags_mariadb(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Tag>&
          lst,
      absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t>& cache);
  void _add_tags_mysql(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Tag>&
          lst,
      absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t>& cache);
  void _add_hosts_mariadb(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
          lst);
  void _add_hosts_mysql(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
          lst);
  void _disable_hosts(const ::google::protobuf::RepeatedField<uint64_t>& lst);
  void _add_host_resources_mariadb(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
          lst,
      absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t>& cache);
  void _add_host_resources_mysql(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
          lst,
      absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t>& cache);
  void _add_service_resources_mariadb(
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::Service>& lst,
      absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t>& cache);
  void _add_service_resources_mysql(
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::Service>& lst,
      absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t>& cache);
  void _add_services_mariadb(const ::google::protobuf::RepeatedPtrField<
                             engine::configuration::Service>& lst);
  void _add_services_mysql(const ::google::protobuf::RepeatedPtrField<
                           engine::configuration::Service>& lst);
  void _add_anomalydetections_mariadb(
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::Anomalydetection>& lst);
  void _add_anomalydetections_mysql(
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::Anomalydetection>& lst);
  void _add_anomalydetection_resources_mariadb(
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::Anomalydetection>& lst,
      absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t>& cache);
  void _add_anomalydetection_resources_mysql(
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::Anomalydetection>& lst,
      absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t>& cache);
  void _add_customvariables_mariadb(
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::CustomVariable>& lst);
  void _add_customvariables_mysql(const ::google::protobuf::RepeatedPtrField<
                                  engine::configuration::CustomVariable>& lst);
  void _disable_services_mariadb(const ::google::protobuf::RepeatedPtrField<
                                 engine::configuration::HostServiceId>& lst);
  void _disable_services_mysql(const ::google::protobuf::RepeatedPtrField<
                               engine::configuration::HostServiceId>& lst);
  void _disable_service_resources_mariadb(
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::HostServiceId>& lst);
  void _disable_service_resources_mysql(
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::HostServiceId>& lst);

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

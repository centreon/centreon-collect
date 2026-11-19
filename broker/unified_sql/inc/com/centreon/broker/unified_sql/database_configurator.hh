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
#include "com/centreon/broker/unified_sql/stream.hh"
#include "common/engine_conf/state.pb.h"

using com::centreon::engine::configuration::DiffState;
using com::centreon::engine::configuration::State;

namespace com::centreon::broker::unified_sql {
class database_configurator {
  enum action {
    ADDED,
    MODIFIED,
    DELETED,
  };
  stream* _stream;
  std::shared_ptr<spdlog::logger> _logger;
  database::mysql_stmt _enable_hosts;
  std::unique_ptr<database::mysql_bulk_stmt>
      _add_anomalydetection_resources_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_anomalydetections_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_host_resources_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_hosts_stmt;
  std::unique_ptr<database::mysql_stmt_base> _wake_up_hosts_stmt;
  std::unique_ptr<database::mysql_stmt_base> _wake_up_host_resources_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_service_resources_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_services_stmt;
  std::unique_ptr<database::mysql_stmt_base> _wake_up_services_stmt;
  std::unique_ptr<database::mysql_stmt_base> _wake_up_service_resources_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_severities_stmt;
  std::unique_ptr<database::mysql_stmt_base> _del_severities_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_tags_stmt;
  std::unique_ptr<database::mysql_stmt_base> _del_tags_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_customvariables_stmt;
  std::unique_ptr<database::mysql_stmt_base> _disable_services_stmt;
  std::unique_ptr<database::mysql_stmt_base> _disable_service_resources_stmt;
  std::unique_ptr<database::mysql_stmt_base> _add_hostgroups_stmt;
  std::unique_ptr<database::mysql_stmt_base> _add_hostgroup_members_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_servicegroups_stmt;
  std::unique_ptr<database::mysql_bulk_stmt> _add_servicegroup_members_stmt;
  std::unique_ptr<database::mysql_stmt_base> _add_host_parents_stmt;

  void _disable_resources_for_pollers_with_full_conf(const DiffState& diff);
  void _disable_hosts_and_services(const DiffState& diff);

  void _add_severities_mariadb(const ::google::protobuf::RepeatedPtrField<
                               engine::configuration::Severity>& lst);
  void _add_severities_mysql(const ::google::protobuf::RepeatedPtrField<
                             engine::configuration::Severity>& lst);
  void _del_severities_mariadb(
      const ::google::protobuf::RepeatedPtrField<
          com::centreon::engine::configuration::KeyType>& keys);
  void _del_severities_mysql(
      const ::google::protobuf::RepeatedPtrField<
          com::centreon::engine::configuration::KeyType>& keys);
  void _add_tags_mariadb(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Tag>&
          lst);
  void _add_tags_mysql(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Tag>&
          lst);
  void _add_hosts_mariadb(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
          lst);
  void _del_tags_mariadb(const ::google::protobuf::RepeatedPtrField<
                         com::centreon::engine::configuration::KeyType>& keys);
  void _del_tags_mysql(const ::google::protobuf::RepeatedPtrField<
                       com::centreon::engine::configuration::KeyType>& keys);
  void _del_hostgroups(
      const ::google::protobuf::RepeatedPtrField<
          com::centreon::engine::configuration::PairGroupPoller>& keys);
  void _del_servicegroups(
      const ::google::protobuf::RepeatedPtrField<
          com::centreon::engine::configuration::PairGroupPoller>& keys);
  void _add_hosts_mysql(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
          lst);
  void _disable_hosts(const ::google::protobuf::RepeatedField<uint64_t>& lst);
  void _add_host_resources_mariadb(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
          lst);
  void _add_host_resources_mysql(
      const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
          lst);
  void _add_service_resources_mariadb(
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::Service>& lst);
  void _add_service_resources_mysql(const ::google::protobuf::RepeatedPtrField<
                                    engine::configuration::Service>& lst);
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
          engine::configuration::Anomalydetection>& lst);
  void _add_anomalydetection_resources_mysql(
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::Anomalydetection>& lst);
  void _add_customvariables_mariadb(
      uint64_t host_id,
      uint64_t service_id,
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::CustomVariable>& lst);
  void _add_customvariables_mysql(
      uint64_t host_id,
      uint64_t service_id,
      const ::google::protobuf::RepeatedPtrField<
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
  void _add_hostgroups_mariadb(const ::google::protobuf::RepeatedPtrField<
                                   engine::configuration::Hostgroup>& lst,
                               bool is_modification = false);
  void _add_hostgroups_mysql(const ::google::protobuf::RepeatedPtrField<
                                 engine::configuration::Hostgroup>& lst,
                             bool is_modification = false);
  void _add_servicegroups_mariadb(
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::Servicegroup>& lst,
      bool is_modification = false);
  void _add_servicegroups_mysql(
      const ::google::protobuf::RepeatedPtrField<
          engine::configuration::Servicegroup>& lst,
      bool is_modification = false);
  void _add_host_parents_mariadb(const ::google::protobuf::RepeatedPtrField<
                                     engine::configuration::Host>& lst,
                                 const action& act);
  void _add_host_parents_mysql(const ::google::protobuf::RepeatedPtrField<
                                   engine::configuration::Host>& lst,
                               const action& act);
  void _del_host_parents(
      const ::google::protobuf::RepeatedField<uint64_t>& lst);

  void _wake_up_resources_mariadb(const engine::configuration::State& state);
  void _wake_up_resources_mysql(const engine::configuration::State& state);

 public:
  database_configurator(stream* stream,
                        const std::shared_ptr<spdlog::logger>& logger)
      : _stream(stream), _logger(logger) {}

  database_configurator(const database_configurator&) = delete;

  void process_diff(const DiffState& diff);
  void process_state(const engine::configuration::State& state);
};
}  // namespace com::centreon::broker::unified_sql

#endif /* !CCB_UNIFIED_SQL_DATABASE_CONFIGURATOR_HH */

/**
 * Copyright 2025-2026 Centreon
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
#include "com/centreon/broker/unified_sql/database_configurator.hh"
#include <google/protobuf/repeated_ptr_field.h>
#include <iterator>
#include "broker/core/config/applier/state.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/sql/table_max_size.hh"
#include "com/centreon/common/utf8.hh"
#include "common/engine_conf/state.pb.h"

using namespace com::centreon::broker::database;
using namespace com::centreon::broker::misc;

using com::centreon::engine::configuration::ActionHostOn;
using com::centreon::engine::configuration::ActionServiceOn;

namespace com::centreon::broker::unified_sql {

/**
 * @brief Execute the configuration process.
 */
void database_configurator::process_diff(const DiffState& diff) {
  /* We start by disabling pollers with full conf. */
  _disable_resources_for_pollers_with_full_conf(diff);

  /* Then we process the diff. */

  /* Disabling removed hosts and services */
  _disable_hosts_and_services(diff);

  if (_stream->supports_bulk_prepared_statements()) {
    _logger->debug("Adding new resources");
    /* Adding new objects */
    _add_severities_mariadb(diff.severities().added());
    _add_tags_mariadb(diff.tags().added());
    _add_hosts_mariadb(diff.hosts().added());
    _add_host_resources_mariadb(diff.hosts().added());
    _add_services_mariadb(diff.services().added());
    _add_service_resources_mariadb(diff.services().added());
    _add_anomalydetections_mariadb(diff.anomalydetections().added());
    _add_anomalydetection_resources_mariadb(diff.anomalydetections().added());
    _add_hostgroups_mariadb(diff.hostgroups().added());
    _add_servicegroups_mariadb(diff.servicegroups().added());
    _add_host_parents_mariadb(diff.hosts().added(), action::ADDED);

    /* Modifying existing objects */
    _logger->debug("Modifying resources");
    _add_severities_mariadb(diff.severities().modified());
    _add_tags_mariadb(diff.tags().modified());

    _add_hosts_mariadb(diff.hosts().modified());
    _add_host_resources_mariadb(diff.hosts().modified());
    _add_services_mariadb(diff.services().modified());
    _add_service_resources_mariadb(diff.services().modified());
    _add_anomalydetections_mariadb(diff.anomalydetections().modified());
    _add_anomalydetection_resources_mariadb(
        diff.anomalydetections().modified());

    _add_hostgroups_mariadb(diff.hostgroups().modified(), true);
    _add_servicegroups_mariadb(diff.servicegroups().modified(), true);
    _add_host_parents_mariadb(diff.hosts().modified(), action::MODIFIED);

    /* Disabling removed objects */
    _logger->debug("Removing/disabling resources");
    _del_severities_mariadb(diff.severities().removed());
    _del_tags_mariadb(diff.tags().removed());
    _disable_hosts(diff.hosts().removed());
    _disable_services_mariadb(diff.services().removed());
    _disable_services_mariadb(diff.anomalydetections().removed());
    _disable_service_resources_mariadb(diff.services().removed());
    _disable_service_resources_mariadb(diff.anomalydetections().removed());
    _del_hostgroups(diff.hostgroups().removed());
    _del_servicegroups(diff.servicegroups().removed());
    _del_host_parents(diff.hosts().removed());
  } else {
    /* Adding new objects */
    _add_severities_mysql(diff.severities().added());
    _add_tags_mysql(diff.tags().added());
    _add_hosts_mysql(diff.hosts().added());
    _add_host_resources_mysql(diff.hosts().added());
    _add_services_mysql(diff.services().added());
    _add_service_resources_mysql(diff.services().added());
    _add_hostgroups_mysql(diff.hostgroups().added());
    _add_servicegroups_mysql(diff.servicegroups().added());
    _add_host_parents_mysql(diff.hosts().added(), action::ADDED);
    _add_anomalydetections_mysql(diff.anomalydetections().added());
    _add_anomalydetection_resources_mysql(diff.anomalydetections().added());

    /* Modifying existing objects */
    _add_severities_mysql(diff.severities().modified());
    _add_tags_mysql(diff.tags().modified());
    _add_hosts_mysql(diff.hosts().modified());
    _add_host_resources_mysql(diff.hosts().modified());
    _add_services_mysql(diff.services().modified());
    _add_service_resources_mysql(diff.services().modified());
    _add_anomalydetections_mysql(diff.anomalydetections().modified());
    _add_anomalydetection_resources_mysql(diff.anomalydetections().modified());
    _add_hostgroups_mysql(diff.hostgroups().modified(), true);
    _add_servicegroups_mysql(diff.servicegroups().modified(), true);
    _add_host_parents_mysql(diff.hosts().modified(), action::MODIFIED);

    /* Disabling removed objects */
    _disable_hosts(diff.hosts().removed());
    _disable_services_mysql(diff.services().removed());
    _disable_services_mysql(diff.anomalydetections().removed());
    _disable_service_resources_mysql(diff.services().removed());
    _disable_service_resources_mysql(diff.anomalydetections().removed());
    _del_hostgroups(diff.hostgroups().removed());
    _del_servicegroups(diff.servicegroups().removed());
    _del_host_parents(diff.hosts().removed());
  }
  _stream->get_mysql().commit();
}

void database_configurator::process_state(
    const engine::configuration::State& s) {
  _logger->info("Processing state for {} hosts", s.hosts_size());
  if (_stream->supports_bulk_prepared_statements())
    _wake_up_resources_mariadb(s);
  else
    _wake_up_resources_mysql(s);
  _stream->get_mysql().commit();
}

void database_configurator::_wake_up_resources_mariadb(
    const engine::configuration::State& state) {
  auto& mysql = _stream->get_mysql();
  // FIXME DBO: if the centralized cache is updated, no more need of this.
  // auto& cache = _stream->hosts_instances_cache();
  uint32_t count = 0;
  if (state.hosts().empty())
    return;

  /* Enable hosts */
  if (!_wake_up_hosts_stmt) {
    _wake_up_hosts_stmt = std::make_unique<database::mysql_bulk_stmt>(
        "UPDATE hosts SET enabled=1 WHERE host_id=? AND instance_id=?");
    mysql.prepare_statement(*_wake_up_hosts_stmt);
  }
  database::mysql_bulk_stmt* stmt =
      static_cast<database::mysql_bulk_stmt*>(_wake_up_hosts_stmt.get());

  auto bind = stmt->create_bind();
  bind->reserve(state.hosts_size());
  for (const auto& host : state.hosts()) {
    _logger->trace("Waking up poller {} host {} in table hosts",
                   host.poller_id(), host.host_id());
    bind->set_value_as_u32(0, host.host_id());
    bind->set_value_as_u32(1, host.poller_id());
    bind->next_row();
    count++;
  }
  _logger->debug("Waking up {} hosts", count);
  stmt->set_bind(std::move(bind));
  mysql.run_statement(*stmt);

  /* Enable host resources */
  if (!_wake_up_host_resources_stmt) {
    _wake_up_host_resources_stmt = std::make_unique<database::mysql_bulk_stmt>(
        "UPDATE resources SET enabled=1 WHERE parent_id=0 AND id=? AND "
        "poller_id=?");
    mysql.prepare_statement(*_wake_up_host_resources_stmt);
  }
  stmt = static_cast<database::mysql_bulk_stmt*>(
      _wake_up_host_resources_stmt.get());

  bind = stmt->create_bind();
  bind->reserve(state.hosts_size());
  for (const auto& host : state.hosts()) {
    _logger->trace("Waking up poller {} host {} in table resources",
                   host.poller_id(), host.host_id());
    bind->set_value_as_u32(0, host.host_id());
    bind->set_value_as_u32(1, host.poller_id());
    bind->next_row();
    // cache.insert_or_assign(host.host_id(), host.poller_id());
  }
  stmt->set_bind(std::move(bind));
  mysql.run_statement(*stmt);

  if (!state.services().empty()) {
    count = 0;
    /* Enable services */
    if (!_wake_up_services_stmt) {
      _wake_up_services_stmt = std::make_unique<database::mysql_bulk_stmt>(
          "UPDATE services SET enabled=1 WHERE host_id=? AND service_id=?");
      mysql.prepare_statement(*_wake_up_services_stmt);
    }
    stmt =
        static_cast<database::mysql_bulk_stmt*>(_wake_up_services_stmt.get());

    bind = stmt->create_bind();
    bind->reserve(state.services_size());
    for (const auto& svc : state.services()) {
      _logger->trace("Waking up service {}:{} in table services", svc.host_id(),
                     svc.service_id());
      bind->set_value_as_u32(0, svc.host_id());
      bind->set_value_as_u32(1, svc.service_id());
      bind->next_row();
      count++;
    }
    _logger->debug("Waking up {} services", count);
    stmt->set_bind(std::move(bind));
    mysql.run_statement(*stmt);

    /* Enable service resources */
    if (!_wake_up_service_resources_stmt) {
      _wake_up_service_resources_stmt =
          std::make_unique<database::mysql_bulk_stmt>(
              "UPDATE resources SET enabled=1 WHERE parent_id=? AND id=?");
      mysql.prepare_statement(*_wake_up_service_resources_stmt);
    }
    stmt = static_cast<database::mysql_bulk_stmt*>(
        _wake_up_service_resources_stmt.get());

    bind = stmt->create_bind();
    bind->reserve(state.services_size());
    for (const auto& service : state.services()) {
      _logger->trace("Waking up service {}:{} in table resources",
                     service.host_id(), service.service_id());
      bind->set_value_as_u32(0, service.host_id());
      bind->set_value_as_u32(1, service.service_id());
      bind->next_row();
    }
    stmt->set_bind(std::move(bind));
    mysql.run_statement(*stmt);
  }
}

void database_configurator::_wake_up_resources_mysql(
    const engine::configuration::State& state) {
  // FIXME DBO
  // auto& cache = _stream->hosts_instances_cache();
  if (state.hosts().empty())
    return;
  auto& mysql = _stream->get_mysql();

  /* Enable hosts */
  if (!_wake_up_hosts_stmt) {
    _wake_up_hosts_stmt = std::make_unique<database::mysql_stmt>(
        "UPDATE hosts SET enabled=1 WHERE host_id=? AND instance_id=?");
    mysql.prepare_statement(*_wake_up_hosts_stmt);
  }

  for (const auto& host : state.hosts()) {
    _wake_up_hosts_stmt->bind_value_as_u32(0, host.host_id());
    _wake_up_hosts_stmt->bind_value_as_u32(1, host.poller_id());
    mysql.run_statement(*_wake_up_hosts_stmt);
  }

  /* Enable host resources */
  if (!_wake_up_host_resources_stmt) {
    _wake_up_host_resources_stmt = std::make_unique<database::mysql_stmt>(
        "UPDATE resources SET enabled=1 WHERE parent_id=0 AND id=? AND "
        "poller_id=?");
    mysql.prepare_statement(*_wake_up_host_resources_stmt);
  }

  for (const auto& host : state.hosts()) {
    _wake_up_host_resources_stmt->bind_value_as_u32(0, host.host_id());
    _wake_up_host_resources_stmt->bind_value_as_u32(1, host.poller_id());
    mysql.run_statement(*_wake_up_host_resources_stmt);
    // cache.insert_or_assign(host.host_id(), host.poller_id());
  }

  /* Enable services */
  if (!_wake_up_services_stmt) {
    _wake_up_services_stmt = std::make_unique<database::mysql_stmt>(
        "UPDATE services SET enabled=1 WHERE host_id=? AND service_id=?");
    mysql.prepare_statement(*_wake_up_services_stmt);
  }

  for (const auto& svc : state.services()) {
    _wake_up_services_stmt->bind_value_as_u32(0, svc.host_id());
    _wake_up_services_stmt->bind_value_as_u32(1, svc.service_id());
    mysql.run_statement(*_wake_up_services_stmt);
  }

  /* Enable service resources */
  if (!_wake_up_service_resources_stmt) {
    _wake_up_service_resources_stmt = std::make_unique<database::mysql_stmt>(
        "UPDATE resources SET enabled=1 WHERE parent_id=? AND id=?");
    mysql.prepare_statement(*_wake_up_service_resources_stmt);
  }

  for (const auto& service : state.services()) {
    _wake_up_service_resources_stmt->bind_value_as_u32(0, service.host_id());
    _wake_up_service_resources_stmt->bind_value_as_u32(1, service.service_id());
    mysql.run_statement(*_wake_up_services_stmt);
  }
}

/**
 * @brief Disable hosts, services in the hosts, services and resources tables
 * for the pollers whose configuration is fully received. This is needed because
 * we don't know which hosts and services have been removed.
 */
void database_configurator::_disable_resources_for_pollers_with_full_conf(
    const DiffState& diff) {
  for (uint64_t instance_id : diff.full_conf_poller_id())
    _stream->clean_tables(instance_id);

  auto& mysql = _stream->get_mysql();

  // Removed hosts are disabled in the hosts table.
  if (!diff.hosts().removed().empty()) {
    std::string query(
        fmt::format("UPDATE hosts SET enabled=0 WHERE host_id IN ({})",
                    fmt::join(diff.hosts().removed(), ",")));
    mysql.run_query(query, database::mysql_error::disable_hosts, 0);

    // Services of removed hosts are disabled in the services table.
    query = fmt::format("UPDATE services SET enabled=0 WHERE host_id IN ({})",
                        fmt::join(diff.hosts().removed(), ","));
    mysql.run_query(query, database::mysql_error::disable_hosts, 0);

    // Same thing with resources table.
    query = fmt::format(
        "UPDATE resources SET enabled=0 WHERE parent_id IN ({0}) OR (parent_id "
        "= 0 AND id IN ({0}))",
        fmt::join(diff.hosts().removed(), ","));
    mysql.run_query(query, database::mysql_error::disable_hosts, 0);
  }

  // Little cleanup in hostgroups
  std::string query(
      "DELETE FROM hostgroups WHERE NOT EXISTS (SELECT 1 FROM hosts_hostgroups "
      "WHERE hostgroups.hostgroup_id = hosts_hostgroups.hostgroup_id)");
  mysql.run_query(query);

  //// We can't remove not used severities in case of full configurations.
  // This is not done because with a full conf, we don't know
  // which severities have been removed. And with a diff, we know that. And when
  // we mix two kinds of diff, we can't know.
  //
  // std::string query(
  //    "DELETE FROM severities WHERE NOT EXISTS ( SELECT 1 FROM resources WHERE
  //    " "resources.severity_id = severities.severity_id)");
  // mysql.run_query(query, database::mysql_error::delete_severities, 0);
  //
  // Same comment for tags.
}

/**
 * @brief Remove severities from the database. (code for MariaDB).
 *
 * @param keys The list of keys to remove.
 */
void database_configurator::_del_severities_mariadb(
    const ::google::protobuf::RepeatedPtrField<
        com::centreon::engine::configuration::KeyType>& keys) {
  if (keys.empty())
    return;

  _logger->debug("Removing {} severities", keys.size());
  mysql& mysql = _stream->get_mysql();
  if (!_del_severities_stmt) {
    std::string query("DELETE FROM severities WHERE id=? AND type=?");
    _del_severities_stmt = std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_del_severities_stmt);
  }
  database::mysql_bulk_stmt* stmt =
      static_cast<database::mysql_bulk_stmt*>(_del_severities_stmt.get());
  auto bind = stmt->create_bind();
  bind->reserve(keys.size());

  auto& bc = config::applier::state::instance().cache();
  for (const auto& msg : keys) {
    _logger->info("deleting severity id={} ; type={}", msg.id(), msg.type());
    bind->set_value_as_u64(0, msg.id());
    bind->set_value_as_u32(1, msg.type());
    bind->next_row();
    bc.erase_severity(msg.id(), msg.type());
  }

  stmt->set_bind(std::move(bind));
  mysql.run_statement(*stmt);
}

/**
 * @brief Remove severities from the database. (code for MySQL).
 *
 * @param keys The list of keys to remove.
 */
void database_configurator::_del_severities_mysql(
    const ::google::protobuf::RepeatedPtrField<
        com::centreon::engine::configuration::KeyType>& keys) {
  if (keys.empty())
    return;

  _logger->debug("Removing {} severities", keys.size());
  mysql& mysql = _stream->get_mysql();
  if (!_del_severities_stmt) {
    std::string query("DELETE FROM severities WHERE id=? AND type=?");
    _del_severities_stmt = std::make_unique<mysql_stmt>(query);
    mysql.prepare_statement(*_del_severities_stmt);
  }

  auto& bc = config::applier::state::instance().cache();
  for (const auto& msg : keys) {
    _logger->info("deleting severity id={} ; type={}", msg.id(), msg.type());
    _del_severities_stmt->bind_value_as_u64(0, msg.id());
    _del_severities_stmt->bind_value_as_u32(1, msg.type());
    mysql.run_statement(*_del_severities_stmt);
    bc.erase_severity(msg.id(), msg.type());
  }
}

/**
 * @brief Remove tags from the database. (code for MariaDB).
 *
 * @param keys The list of keys to remove.
 */
void database_configurator::_del_tags_mariadb(
    const ::google::protobuf::RepeatedPtrField<
        com::centreon::engine::configuration::TagKeyWithPoller>& keys) {
  auto& cache = _stream->tags_cache();

  if (keys.empty())
    return;

  _logger->debug("Removing {} tags", keys.size());
  mysql& mysql = _stream->get_mysql();
  if (!_del_tags_stmt) {
    std::string query("DELETE FROM tags WHERE id=? AND type=?");
    _del_tags_stmt = std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_del_tags_stmt);
  }
  database::mysql_bulk_stmt* stmt =
      static_cast<database::mysql_bulk_stmt*>(_del_tags_stmt.get());
  auto bind = stmt->create_bind();
  bind->reserve(keys.size());

  for (const auto& msg : keys) {
    _logger->info("deleting tag id={} ; type={}", msg.id(), msg.type());
    bind->set_value_as_u64(0, msg.id());
    bind->set_value_as_u32(1, msg.type());
    bind->next_row();
    cache.erase(std::make_pair(msg.id(), msg.type()));
  }

  stmt->set_bind(std::move(bind));
  mysql.run_statement(*stmt);
}

/**
 * @brief Remove tags from the database. (code for MySQL).
 *
 * @param keys The list of keys to remove.
 */
void database_configurator::_del_tags_mysql(
    const ::google::protobuf::RepeatedPtrField<
        com::centreon::engine::configuration::TagKeyWithPoller>& keys) {
  auto& cache = _stream->tags_cache();

  if (keys.empty())
    return;

  _logger->debug("Removing {} tags", keys.size());
  mysql& mysql = _stream->get_mysql();
  if (!_del_tags_stmt) {
    std::string query("DELETE FROM tags WHERE id=? AND type=?");
    _del_tags_stmt = std::make_unique<mysql_stmt>(query);
    mysql.prepare_statement(*_del_tags_stmt);
  }

  for (const auto& msg : keys) {
    _logger->info("deleting tag id={} ; type={}", msg.id(), msg.type());
    _del_tags_stmt->bind_value_as_u64(0, msg.id());
    _del_tags_stmt->bind_value_as_u32(1, msg.type());
    mysql.run_statement(*_del_tags_stmt);
    cache.erase(std::make_pair(msg.id(), msg.type()));
  }
}

void database_configurator::_disable_hosts(
    const ::google::protobuf::RepeatedField<uint64_t>& host_ids) {
  if (host_ids.empty())
    return;

  _logger->debug("Disabling {} hosts", host_ids.size());
  std::string query(
      fmt::format("UPDATE hosts SET enabled=0 WHERE host_id IN ({})",
                  fmt::join(host_ids, ",")));
  _stream->get_mysql().run_query(query, database::mysql_error::disable_hosts);
}

/**
 * @brief Disable hosts, services in the hosts, services and resources tables
 * corresponding to the removed hosts in the diff state.
 */
void database_configurator::_disable_hosts_and_services(const DiffState& diff) {
  if (diff.hosts().removed().empty())
    return;

  // Removed hosts are disabled in the hosts table.
  _disable_hosts(diff.hosts().removed());

  // Services of removed hosts are disabled in the services table.
  std::string query =
      fmt::format("UPDATE services SET enabled=0 WHERE host_id IN ({})",
                  fmt::join(diff.hosts().removed(), ","));
  _stream->get_mysql().run_query(query, database::mysql_error::disable_hosts,
                                 0);

  // Same thing with resources table.
  query = fmt::format(
      "UPDATE resources SET enabled=0 WHERE parent_id IN ({0}) OR (parent_id = "
      "0 AND id IN ({0}))",
      fmt::join(diff.hosts().removed(), ","));
  _stream->get_mysql().run_query(query, database::mysql_error::disable_hosts,
                                 0);
}

/**
 * @brief Add severities into the database. (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_severities_mariadb(
    const ::google::protobuf::RepeatedPtrField<engine::configuration::Severity>&
        lst) {
  if (lst.empty())
    return;
  std::list<std::pair<uint64_t, uint16_t>> keys;
  mysql& mysql = _stream->get_mysql();
  if (!_add_severities_stmt) {
    std::string query(
        "INSERT INTO severities (id,type,name,level,icon_id) VALUES "
        "(?,?,?,?,?) ON DUPLICATE KEY UPDATE "
        "name=VALUES(name), level=VALUES(level), icon_id=VALUES(icon_id)");
    _add_severities_stmt = std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_add_severities_stmt);
  }
  auto bind = _add_severities_stmt->create_bind();
  bind->reserve(lst.size());

  uint32_t count = 0;
  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.key().id(), msg.key().type());
    keys.push_back(key);

    _logger->info("Processing severity id={}, type={}", key.first, key.second);
    bind->set_value_as_u64(0, key.first);
    bind->set_value_as_u32(1, key.second);
    bind->set_value_as_str(
        2, common::truncate_utf8(msg.severity_name(),
                                 get_centreon_storage_severities_col_size(
                                     centreon_storage_severities_name)));
    bind->set_value_as_u32(3, msg.level());
    bind->set_value_as_u64(4, msg.icon_id());
    bind->next_row();
    count++;
  }
  _logger->debug("{} severities added/modified", count);
  _add_severities_stmt->set_bind(std::move(bind));

  try {
    std::promise<uint64_t> promise;
    std::future<uint64_t> future = promise.get_future();
    mysql.run_statement_and_get_int<uint64_t>(
        *_add_severities_stmt, std::move(promise),
        mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    auto& bc = config::applier::state::instance().cache();
    for (auto& k : keys) {
      uint64_t existing_id = bc.get_db_id_for_severity(k.first, k.second);
      if (!existing_id) {
        bc.set_db_id_for_severity(k.first, k.second, first_id);
        _logger->trace("Severity with id {} and type {} has severity_id {}",
                       k.first, k.second, first_id);
        first_id++;
      } else {
        _logger->trace("Severity with id {} and type {} has severity_id {}",
                       k.first, k.second, existing_id);
      }
    }
  } catch (const std::exception& e) {
    _logger->error("Error while executing <<_add_severities>>: {}", e.what());
  }
}

/**
 * @brief Add severities into the database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_severities_mysql(
    const ::google::protobuf::RepeatedPtrField<engine::configuration::Severity>&
        lst) {
  if (lst.empty())
    return;
  mysql& mysql = _stream->get_mysql();
  std::list<std::pair<uint64_t, uint16_t>> keys;

  std::vector<std::string> values;
  values.reserve(lst.size());

  uint32_t count = 0;
  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.key().id(), msg.key().type());
    keys.push_back(key);

    std::string value(fmt::format(
        "({},{},'{}',{},{})", msg.key().id(), msg.key().type(),
        misc::string::escape(msg.severity_name(),
                             get_centreon_storage_severities_col_size(
                                 centreon_storage_severities_name)),
        msg.level(), msg.icon_id()));
    values.emplace_back(value);
    count++;
  }
  std::string query(fmt::format(
      "INSERT INTO severities (id,type,name,level,icon_id) VALUES {} ON "
      "DUPLICATE KEY UPDATE "
      "name=VALUES(name),level=VALUES(level), icon_id=VALUES(icon_id)",
      fmt::join(values, ",")));

  _logger->debug("{} severities added/modified", count);

  try {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();
    mysql.run_query_and_get_int(query, std::move(promise),
                                mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    auto& bc = config::applier::state::instance().cache();
    for (auto& k : keys) {
      uint64_t existing_id = bc.get_db_id_for_severity(k.first, k.second);
      if (!existing_id) {
        bc.set_db_id_for_severity(k.first, k.second, first_id);
        _logger->trace("Severity with id {} and type {} has severity_id {}",
                       k.first, k.second, first_id);
        first_id++;
      } else {
        _logger->trace("Severity with id {} and type {} has severity_id {}",
                       k.first, k.second, existing_id);
      }
    }
  } catch (const std::exception& e) {
    _logger->error("Error while executing <<_add_severities>>: {}", e.what());
  }
}

// clang-format off
/** Database configuration
 * Query: INSERT ON DUPLICATE KEY UPDATE
 * Method: _add_tags
 * Return: absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t>
 * Key: {key::id, key::type}
 * Protobuf message: engine::configuration::Tag
 * Description: Add tags into the database.
 * Table: tags
 * Data:
 *  FIELD                 & TYPE   & COL NAME    & C_TYPE & OPTIONS
 *  ---------------------------------------------------------------
 *  ${0}                  & uint64 & tag_id      & uint64 & AU
 *  key::id               & uint64 & id          & uint64 &
 *  key::type             & uint32 & type        & uint32 &
 *  tag_name              & string & name        & string &
 *
 */
// clang-format on
/**
 * @brief Add tags into the database. (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_tags_mariadb(
    const ::google::protobuf::RepeatedPtrField<engine::configuration::Tag>&
        lst) {
  if (lst.empty())
    return;
  auto& cache = _stream->tags_cache();
  std::list<std::pair<uint64_t, uint16_t>> keys;
  mysql& mysql = _stream->get_mysql();
  if (!_add_tags_stmt) {
    std::string query(
        "INSERT INTO tags (id,type,name) VALUES (?,?,?) ON DUPLICATE KEY "
        "UPDATE id=VALUES(id),type=VALUES(type),name=VALUES(name)");
    _add_tags_stmt = std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_add_tags_stmt);
  }
  auto bind = _add_tags_stmt->create_bind();
  bind->reserve(lst.size());

  uint32_t count = 0;
  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.key().id(), msg.key().type());
    keys.push_back(key);

    _logger->info("Processing tag id={}, type={}", key.first, key.second);
    bind->set_value_as_u64(0, key.first);
    bind->set_value_as_u32(1, key.second);
    bind->set_value_as_str(
        2, common::truncate_utf8(
               msg.tag_name(),
               get_centreon_storage_tags_col_size(centreon_storage_tags_name)));
    bind->next_row();
    count++;
  }
  _logger->debug("{} tags added/modified", count);
  _add_tags_stmt->set_bind(std::move(bind));

  try {
    std::promise<uint64_t> promise;
    std::future<uint64_t> future = promise.get_future();
    mysql.run_statement_and_get_int<uint64_t>(
        *_add_tags_stmt, std::move(promise),
        mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& k : keys) {
      auto inserted = cache.emplace(k, first_id);
      if (inserted.second) {
        _logger->trace("Tag with id {} and type {} has tag_id {}", k.first,
                       k.second, first_id);
        first_id++;
      } else {
        _logger->trace("Tag with id {} and type {} has tag_id {}", k.first,
                       k.second, inserted.first->second);
      }
    }
  } catch (const std::exception& e) {
    _logger->error("Error while executing <<_add_tags>>: {}", e.what());
  }
}

/**
 * @brief Add tags into the database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_tags_mysql(
    const ::google::protobuf::RepeatedPtrField<engine::configuration::Tag>&
        lst) {
  if (lst.empty())
    return;
  auto& cache = _stream->tags_cache();
  std::list<std::pair<uint64_t, uint16_t>> keys;
  mysql& mysql = _stream->get_mysql();

  std::vector<std::string> values;
  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.key().id(), msg.key().type());
    keys.push_back(key);

    std::string value(fmt::format(
        "({},{},'{}')", msg.key().id(), msg.key().type(),
        misc::string::escape(msg.tag_name(), get_centreon_storage_tags_col_size(
                                                 centreon_storage_tags_name))));
    values.emplace_back(value);
  }
  std::string query(fmt::format(
      "INSERT (id,type,name) INTO tags VALUES {} ON DUPLICATE KEY UPDATE "
      "id=VALUES(id),type=VALUES(type),name=VALUES(name)",
      fmt::join(values, ",")));

  try {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();
    mysql.run_query_and_get_int(query, std::move(promise),
                                mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& k : keys) {
      auto inserted = cache.emplace(k, first_id);
      if (inserted.second) {
        _logger->trace("Tag with id {} and type {} has tag_id {}", k.first,
                       k.second, first_id);
        first_id++;
      } else {
        _logger->trace("Tag with id {} and type {} has tag_id {}", k.first,
                       k.second, inserted.first->second);
      }
    }
  } catch (const std::exception& e) {
    _logger->error("Error while executing <<_add_tags>>: {}", e.what());
  }
}

// clang-format off
/** Database configuration
 * Query: INSERT ON DUPLICATE KEY UPDATE
 * Method: _add_hosts
 * Protobuf message: engine::configuration::Host
 * Description: Add hosts into the database.
 * Table: hosts
 * Data:
 *   FIELD                                                                  & TYPE   & COL NAME                      & C_TYPE & OPTIONS
 *   ----------------------------------------------------------------------------------------------------------------------------------
 *   host_id                                                                & uint64 & host_id                       & int32  & U
 *   host_name                                                              & string & name                          & string &
 *   poller_id                                                              & uint64 & instance_id                   & int32  &
 *   action_url                                                             & string & action_url                    & string &
 *   checks_active                                                          & bool   & active_checks                 & bool   &
 *   address                                                                & string & address                       & string &
 *   alias                                                                  & string & alias                         & string &
 *   check_command                                                          & string & check_command                 & string &
 *   check_freshness                                                        & bool   & check_freshness               & bool   &
 *   check_interval                                                         & uint32 & check_interval                & double &
 *   check_period                                                           & string & check_period                  & string &
 *   checks_active                                                          & bool   & default_active_checks         & bool   &
 *   event_handler_enabled                                                  & bool   & default_event_handler_enabled & bool   &
 *   flap_detection_enabled                                                 & bool   & default_flap_detection        & bool   &
 *   notifications_enabled                                                  & bool   & default_notify                & bool   &
 *   checks_passive                                                         & bool   & default_passive_checks        & bool   &
 *   process_perf_data                                                      & bool   & default_process_perfdata      & bool   &
 *   display_name                                                           & string & display_name                  & string &
 *   ${true}                                                                & bool   & enabled                       & bool   &
 *   event_handler                                                          & string & event_handler                 & string &
 *   event_handler_enabled                                                  & bool   & event_handler_enabled         & bool   &
 *   first_notification_delay                                               & uint32 & first_notification_delay      & double &
 *   flap_detection_enabled                                                 & bool   & flap_detection                & bool   &
 *   ${msg.flap_detection_options() & ActionHostOn::action_hst_down}        & bool   & flap_detection_on_down        & bool   &
 *   ${msg.flap_detection_options() & ActionHostOn::action_hst_unreachable} & bool   & flap_detection_on_unreachable & bool   &
 *   ${msg.flap_detection_options() & ActionHostOn::action_hst_up}          & bool   & flap_detection_on_up          & bool   &
 *   freshness_threshold                                                    & uint32 & freshness_threshold           & double &
 *   high_flap_threshold                                                    & uint32 & high_flap_threshold           & double &
 *   icon_image                                                             & string & icon_image                    & string &
 *   icon_image_alt                                                         & string & icon_image_alt                & string &
 *   low_flap_threshold                                                     & uint32 & low_flap_threshold            & double &
 *   max_check_attempts                                                     & uint32 & max_check_attempts            & int32  &
 *   notes                                                                  & string & notes                         & string &
 *   notes_url                                                              & string & notes_url                     & string &
 *   notification_interval                                                  & uint32 & notification_interval         & double &
 *   notification period                                                    & string & notification_period           & string &
 *   notifications_enabled                                                  & bool   & notify                        & bool   &
 *   ${msg.notification_options() & ActionHostOn::action_hst_down}          & bool   & notify_on_down                & bool   &
 *   ${msg.notification_options() & ActionHostOn::action_hst_downtime}      & bool   & notify_on_downtime            & bool   &
 *   ${msg.notification_options() & ActionHostOn::action_hst_flapping}      & bool   & notify_on_flapping            & bool   &
 *   ${msg.notification_options() & ActionHostOn::action_hst_up}            & bool   & notify_on_recovery            & bool   &
 *   ${msg.notification_options() & ActionHostOn::action_hst_unreachable}   & bool   & notify_on_unreachable         & bool   &
 *   obsess_over_host                                                       & bool   & obsess_over_host              & bool   &
 *   checks_passive                                                         & bool   & passive_checks                & bool   &
 *   process_perf_data                                                      & bool   & process_perfdata              & bool   &
 *   retain_nonstatus_information                                           & bool   & retain_nonstatus_information  & bool   &
 *   retain_status_information                                              & bool   & retain_status_information     & bool   &
 *   retry_interval                                                         & uint32 & retry_interval                & double &
 *   ${msg.stalking_options() & ActionHostOn::action_hst_down}              & bool   & stalk_on_down                 & bool   &
 *   ${msg.stalking_options() & ActionHostOn::action_hst_unreachable}       & bool   & stalk_on_unreachable          & bool   &
 *   ${msg.stalking_options() & ActionHostOn::action_hst_up}                & bool   & stalk_on_up                   & bool   &
 *   statusmap_image                                                        & string & statusmap_image               & string &
 *   timezone                                                               & string & timezone                      & string & O
 */
// clang-format on
/**
 * @brief Add hosts into the database. (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_hosts_mariadb(
    const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
        lst) {
  if (lst.empty()) {
    _logger->debug("No need to add/update hosts, list empty");
    return;
  }

  mysql& mysql = _stream->get_mysql();
  if (!_add_hosts_stmt) {
    std::string query(
        "INSERT INTO hosts "
        "(host_id,name,instance_id,action_url,active_checks,address,alias,"
        "check_command,check_freshness,check_interval,check_period,default_"
        "active_checks,default_event_handler_enabled,default_flap_detection,"
        "default_notify,default_passive_checks,default_process_perfdata,"
        "display_name,enabled,event_handler,event_handler_enabled,first_"
        "notification_delay,flap_detection,flap_detection_on_down,flap_"
        "detection_on_unreachable,flap_detection_on_up,freshness_threshold,"
        "high_flap_threshold,icon_image,icon_image_alt,low_flap_threshold,max_"
        "check_attempts,notes,notes_url,notification_interval,notify,notify_on_"
        "down,notify_on_downtime,notify_on_flapping,notify_on_recovery,notify_"
        "on_unreachable,obsess_over_host,passive_checks,process_perfdata,"
        "retain_nonstatus_information,retain_status_information,retry_interval,"
        "stalk_on_down,stalk_on_unreachable,stalk_on_up,state,state_type, "
        "statusmap_image,timezone) VALUES "
        "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?"
        ",?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,4,1,?,?) ON DUPLICATE KEY UPDATE "
        "name=VALUES(name),instance_id=VALUES(instance_id),action_url=VALUES("
        "action_url),active_checks=VALUES(active_checks),address=VALUES("
        "address),alias=VALUES(alias),check_command=VALUES(check_command),"
        "check_freshness=VALUES(check_freshness),check_interval=VALUES(check_"
        "interval),check_period=VALUES(check_period),default_active_checks="
        "VALUES(default_active_checks),default_event_handler_enabled=VALUES("
        "default_event_handler_enabled),default_flap_detection=VALUES(default_"
        "flap_detection),default_notify=VALUES(default_notify),default_passive_"
        "checks=VALUES(default_passive_checks),default_process_perfdata=VALUES("
        "default_process_perfdata),display_name=VALUES(display_name),enabled="
        "VALUES(enabled),event_handler=VALUES(event_handler),event_handler_"
        "enabled=VALUES(event_handler_enabled),first_notification_delay=VALUES("
        "first_notification_delay),flap_detection=VALUES(flap_detection),flap_"
        "detection_on_down=VALUES(flap_detection_on_down),flap_detection_on_"
        "unreachable=VALUES(flap_detection_on_unreachable),flap_detection_on_"
        "up=VALUES(flap_detection_on_up),freshness_threshold=VALUES(freshness_"
        "threshold),high_flap_threshold=VALUES(high_flap_threshold),icon_image="
        "VALUES(icon_image),icon_image_alt=VALUES(icon_image_alt),low_flap_"
        "threshold=VALUES(low_flap_threshold),max_check_attempts=VALUES(max_"
        "check_attempts),notes=VALUES(notes),notes_url=VALUES(notes_url),"
        "notification_interval=VALUES(notification_interval),notify=VALUES("
        "notify),notify_on_down=VALUES(notify_on_down),notify_on_downtime="
        "VALUES(notify_on_downtime),notify_on_flapping=VALUES(notify_on_"
        "flapping),notify_on_recovery=VALUES(notify_on_recovery),notify_on_"
        "unreachable=VALUES(notify_on_unreachable),obsess_over_host=VALUES("
        "obsess_over_host),passive_checks=VALUES(passive_checks),process_"
        "perfdata=VALUES(process_perfdata),retain_nonstatus_information=VALUES("
        "retain_nonstatus_information),retain_status_information=VALUES(retain_"
        "status_information),retry_interval=VALUES(retry_interval),stalk_on_"
        "down=VALUES(stalk_on_down),stalk_on_unreachable=VALUES(stalk_on_"
        "unreachable),stalk_on_up=VALUES(stalk_on_up),state=COALESCE(state, 4),"
        "state_type=COALESCE(state_type, 1),"
        "statusmap_image=VALUES(statusmap_image),timezone=VALUES(timezone)");
    _add_hosts_stmt = std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_add_hosts_stmt);
  }
  auto bind = _add_hosts_stmt->create_bind();

  uint32_t count = 0;
  for (const auto& msg : lst) {
    _logger->debug("Processing host {} (id {} - poller {})", msg.host_name(),
                   msg.host_id(), msg.poller_id());
    bind->set_value_as_i32(0, msg.host_id());
    bind->set_value_as_str(
        1, common::truncate_utf8(msg.host_name(),
                                 get_centreon_storage_hosts_col_size(
                                     centreon_storage_hosts_name)));
    bind->set_value_as_i32(2, msg.poller_id());
    bind->set_value_as_str(
        3, common::truncate_utf8(msg.action_url(),
                                 get_centreon_storage_hosts_col_size(
                                     centreon_storage_hosts_action_url)));
    bind->set_value_as_bool(4, msg.checks_active());
    bind->set_value_as_str(
        5, common::truncate_utf8(msg.address(),
                                 get_centreon_storage_hosts_col_size(
                                     centreon_storage_hosts_address)));
    bind->set_value_as_str(
        6,
        common::truncate_utf8(msg.alias(), get_centreon_storage_hosts_col_size(
                                               centreon_storage_hosts_alias)));
    bind->set_value_as_str(
        7, common::truncate_utf8(msg.check_command(),
                                 get_centreon_storage_hosts_col_size(
                                     centreon_storage_hosts_check_command)));
    bind->set_value_as_bool(8, msg.check_freshness());
    bind->set_value_as_f64(9, msg.check_interval());
    bind->set_value_as_str(
        10, common::truncate_utf8(msg.check_period(),
                                  get_centreon_storage_hosts_col_size(
                                      centreon_storage_hosts_check_period)));
    bind->set_value_as_bool(11, msg.checks_active());
    bind->set_value_as_bool(12, msg.event_handler_enabled());
    bind->set_value_as_bool(13, msg.flap_detection_enabled());
    bind->set_value_as_bool(14, msg.notifications_enabled());
    bind->set_value_as_bool(15, msg.checks_passive());
    bind->set_value_as_bool(16, msg.process_perf_data());
    bind->set_value_as_str(
        17, common::truncate_utf8(msg.display_name(),
                                  get_centreon_storage_hosts_col_size(
                                      centreon_storage_hosts_display_name)));
    bind->set_value_as_bool(18, true);
    bind->set_value_as_str(
        19, common::truncate_utf8(msg.event_handler(),
                                  get_centreon_storage_hosts_col_size(
                                      centreon_storage_hosts_event_handler)));
    bind->set_value_as_bool(20, msg.event_handler_enabled());
    bind->set_value_as_f64(21, msg.first_notification_delay());
    bind->set_value_as_bool(22, msg.flap_detection_enabled());
    bind->set_value_as_bool(
        23, msg.flap_detection_options() & ActionHostOn::action_hst_down);
    bind->set_value_as_bool(24, msg.flap_detection_options() &
                                    ActionHostOn::action_hst_unreachable);
    bind->set_value_as_bool(
        25, msg.flap_detection_options() & ActionHostOn::action_hst_up);
    bind->set_value_as_f64(26, msg.freshness_threshold());
    bind->set_value_as_f64(27, msg.high_flap_threshold());
    bind->set_value_as_str(
        28, common::truncate_utf8(msg.icon_image(),
                                  get_centreon_storage_hosts_col_size(
                                      centreon_storage_hosts_icon_image)));
    bind->set_value_as_str(
        29, common::truncate_utf8(msg.icon_image_alt(),
                                  get_centreon_storage_hosts_col_size(
                                      centreon_storage_hosts_icon_image_alt)));
    bind->set_value_as_f64(30, msg.low_flap_threshold());
    bind->set_value_as_i32(31, msg.max_check_attempts());
    bind->set_value_as_str(
        32,
        common::truncate_utf8(msg.notes(), get_centreon_storage_hosts_col_size(
                                               centreon_storage_hosts_notes)));
    bind->set_value_as_str(
        33, common::truncate_utf8(msg.notes_url(),
                                  get_centreon_storage_hosts_col_size(
                                      centreon_storage_hosts_notes_url)));
    bind->set_value_as_f64(34, msg.notification_interval());
    bind->set_value_as_bool(35, msg.notifications_enabled());
    bind->set_value_as_bool(
        36, msg.notification_options() & ActionHostOn::action_hst_down);
    bind->set_value_as_bool(
        37, msg.notification_options() & ActionHostOn::action_hst_downtime);
    bind->set_value_as_bool(
        38, msg.notification_options() & ActionHostOn::action_hst_flapping);
    bind->set_value_as_bool(
        39, msg.notification_options() & ActionHostOn::action_hst_up);
    bind->set_value_as_bool(
        40, msg.notification_options() & ActionHostOn::action_hst_unreachable);
    bind->set_value_as_bool(41, msg.obsess_over_host());
    bind->set_value_as_bool(42, msg.checks_passive());
    bind->set_value_as_bool(43, msg.process_perf_data());
    bind->set_value_as_bool(44, msg.retain_nonstatus_information());
    bind->set_value_as_bool(45, msg.retain_status_information());
    bind->set_value_as_f64(46, msg.retry_interval());
    bind->set_value_as_bool(
        47, msg.stalking_options() & ActionHostOn::action_hst_down);
    bind->set_value_as_bool(
        48, msg.stalking_options() & ActionHostOn::action_hst_unreachable);
    bind->set_value_as_bool(
        49, msg.stalking_options() & ActionHostOn::action_hst_up);
    bind->set_value_as_str(
        50, common::truncate_utf8(msg.statusmap_image(),
                                  get_centreon_storage_hosts_col_size(
                                      centreon_storage_hosts_statusmap_image)));
    if (msg.has_timezone())
      bind->set_value_as_str(
          51, common::truncate_utf8(msg.timezone(),
                                    get_centreon_storage_hosts_col_size(
                                        centreon_storage_hosts_timezone)));
    else
      bind->set_null_str(51);
    bind->next_row();
    count++;
  }
  _logger->debug("Adding/updating {} hosts", count);
  _add_hosts_stmt->set_bind(std::move(bind));
  mysql.run_statement(*_add_hosts_stmt);
}

/**
 * @brief Add hosts into the database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_hosts_mysql(
    const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
        lst) {
  if (lst.empty()) {
    _logger->debug("No need to add/update hosts, list empty");
    return;
  }

  mysql& mysql = _stream->get_mysql();

  std::vector<std::string> values;
  std::string timezone;
  for (const auto& msg : lst) {
    if (msg.has_timezone()) {
      timezone = fmt::format(
          "'{}'", misc::string::escape(msg.timezone(),
                                       get_centreon_storage_hosts_col_size(
                                           centreon_storage_hosts_timezone)));
    } else
      timezone = "NULL";
    std::string value(fmt::format(
        "({},'{}',{},'{}',{},'{}','{}','{}',{},{},'{}',{},{},{},{},{},{},'{}',"
        "1,'{}',{},{},{},{},{},{},{},{},'{}','{}',{},{},'{}','{}',{},{},{},{},{"
        "},{},{},{},{},{},{},{},{},{},{},{},4, 1, '{}',{})",
        msg.host_id(),
        misc::string::escape(
            msg.host_name(),
            get_centreon_storage_hosts_col_size(centreon_storage_hosts_name)),
        msg.poller_id(),
        misc::string::escape(msg.action_url(),
                             get_centreon_storage_hosts_col_size(
                                 centreon_storage_hosts_action_url)),
        msg.checks_active(),
        misc::string::escape(msg.address(),
                             get_centreon_storage_hosts_col_size(
                                 centreon_storage_hosts_address)),
        misc::string::escape(msg.alias(), get_centreon_storage_hosts_col_size(
                                              centreon_storage_hosts_alias)),
        misc::string::escape(msg.check_command(),
                             get_centreon_storage_hosts_col_size(
                                 centreon_storage_hosts_check_command)),
        msg.check_freshness(), msg.check_interval(),
        misc::string::escape(msg.check_period(),
                             get_centreon_storage_hosts_col_size(
                                 centreon_storage_hosts_check_period)),
        msg.checks_active(), msg.event_handler_enabled(),
        msg.flap_detection_enabled(), msg.notifications_enabled(),
        msg.checks_passive(), msg.process_perf_data(),
        misc::string::escape(msg.display_name(),
                             get_centreon_storage_hosts_col_size(
                                 centreon_storage_hosts_display_name)),
        misc::string::escape(msg.event_handler(),
                             get_centreon_storage_hosts_col_size(
                                 centreon_storage_hosts_event_handler)),
        msg.event_handler_enabled(), msg.first_notification_delay(),
        msg.flap_detection_enabled(),
        msg.flap_detection_options() & ActionHostOn::action_hst_down ? 1 : 0,
        msg.flap_detection_options() & ActionHostOn::action_hst_unreachable ? 1
                                                                            : 0,
        msg.flap_detection_options() & ActionHostOn::action_hst_up ? 1 : 0,
        msg.freshness_threshold(), msg.high_flap_threshold(),
        misc::string::escape(msg.icon_image(),
                             get_centreon_storage_hosts_col_size(
                                 centreon_storage_hosts_icon_image)),
        misc::string::escape(msg.icon_image_alt(),
                             get_centreon_storage_hosts_col_size(
                                 centreon_storage_hosts_icon_image_alt)),
        msg.low_flap_threshold(), msg.max_check_attempts(),
        misc::string::escape(msg.notes(), get_centreon_storage_hosts_col_size(
                                              centreon_storage_hosts_notes)),
        misc::string::escape(msg.notes_url(),
                             get_centreon_storage_hosts_col_size(
                                 centreon_storage_hosts_notes_url)),
        msg.notification_interval(), msg.notifications_enabled(),
        msg.notification_options() & ActionHostOn::action_hst_down ? 1 : 0,
        msg.notification_options() & ActionHostOn::action_hst_downtime ? 1 : 0,
        msg.notification_options() & ActionHostOn::action_hst_flapping ? 1 : 0,
        msg.notification_options() & ActionHostOn::action_hst_up ? 1 : 0,
        msg.notification_options() & ActionHostOn::action_hst_unreachable ? 1
                                                                          : 0,
        msg.obsess_over_host(), msg.checks_passive(), msg.process_perf_data(),
        msg.retain_nonstatus_information(), msg.retain_status_information(),
        msg.retry_interval(),
        msg.stalking_options() & ActionHostOn::action_hst_down ? 1 : 0,
        msg.stalking_options() & ActionHostOn::action_hst_unreachable ? 1 : 0,
        msg.stalking_options() & ActionHostOn::action_hst_up ? 1 : 0,
        misc::string::escape(msg.statusmap_image(),
                             get_centreon_storage_hosts_col_size(
                                 centreon_storage_hosts_statusmap_image)),
        timezone));
    values.emplace_back(value);
  }
  std::string query(fmt::format(
      "INSERT INTO hosts "
      "(host_id,name,instance_id,action_url,active_checks,address,alias,"
      "check_command,check_freshness,check_interval,check_period,default_"
      "active_checks,default_event_handler_enabled,default_flap_detection,"
      "default_notify,default_passive_checks,default_process_perfdata,"
      "display_name,enabled,event_handler,event_handler_enabled,first_"
      "notification_delay,flap_detection,flap_detection_on_down,flap_"
      "detection_on_unreachable,flap_detection_on_up,freshness_threshold,"
      "high_flap_threshold,icon_image,icon_image_alt,low_flap_threshold,max_"
      "check_attempts,notes,notes_url,notification_interval,notify,notify_on_"
      "down,notify_on_downtime,notify_on_flapping,notify_on_recovery,notify_"
      "on_unreachable,obsess_over_host,passive_checks,process_perfdata,"
      "retain_nonstatus_information,retain_status_information,retry_interval,"
      "stalk_on_down,stalk_on_unreachable,stalk_on_up,state,state_type, "
      "statusmap_image,timezone) VALUES {} ON DUPLICATE KEY UPDATE "
      "name=VALUES(name),instance_id=VALUES(instance_id),action_url=VALUES("
      "action_url),active_checks=VALUES(active_checks),address=VALUES(address),"
      "alias=VALUES(alias),check_command=VALUES(check_command),check_freshness="
      "VALUES(check_freshness),check_interval=VALUES(check_interval),check_"
      "period=VALUES(check_period),default_active_checks=VALUES(default_active_"
      "checks),default_event_handler_enabled=VALUES(default_event_handler_"
      "enabled),default_flap_detection=VALUES(default_flap_detection),default_"
      "notify=VALUES(default_notify),default_passive_checks=VALUES(default_"
      "passive_checks),default_process_perfdata=VALUES(default_process_"
      "perfdata),display_name=VALUES(display_name),enabled=VALUES(enabled),"
      "event_handler=VALUES(event_handler),event_handler_enabled=VALUES(event_"
      "handler_enabled),first_notification_delay=VALUES(first_notification_"
      "delay),flap_detection=VALUES(flap_detection),flap_detection_on_down="
      "VALUES(flap_detection_on_down),flap_detection_on_unreachable=VALUES("
      "flap_detection_on_unreachable),flap_detection_on_up=VALUES(flap_"
      "detection_on_up),freshness_threshold=VALUES(freshness_threshold),high_"
      "flap_threshold=VALUES(high_flap_threshold),icon_image=VALUES(icon_image)"
      ",icon_image_alt=VALUES(icon_image_alt),low_flap_threshold=VALUES(low_"
      "flap_threshold),max_check_attempts=VALUES(max_check_attempts),notes="
      "VALUES(notes),notes_url=VALUES(notes_url),notification_interval=VALUES("
      "notification_interval),notify=VALUES(notify),notify_on_down=VALUES("
      "notify_on_down),notify_on_downtime=VALUES(notify_on_downtime),notify_on_"
      "flapping=VALUES(notify_on_flapping),notify_on_recovery=VALUES(notify_on_"
      "recovery),notify_on_unreachable=VALUES(notify_on_unreachable),obsess_"
      "over_host=VALUES(obsess_over_host),passive_checks=VALUES(passive_checks)"
      ",process_perfdata=VALUES(process_perfdata),retain_nonstatus_information="
      "VALUES(retain_nonstatus_information),retain_status_information=VALUES("
      "retain_status_information),retry_interval=VALUES(retry_interval),stalk_"
      "on_down=VALUES(stalk_on_down),stalk_on_unreachable=VALUES(stalk_on_"
      "unreachable),stalk_on_up=VALUES(stalk_on_up),state=COALESCE(state, 4), "
      "state_type=COALESCE(state_type, 1), "
      "statusmap_image=VALUES(statusmap_image),timezone=VALUES(timezone)",
      fmt::join(values, ",")));
  mysql.run_query(query);
}

/**
 * @brief Add hosts into the resources database. (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_host_resources_mariadb(
    const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
        lst) {
  auto& cache = _stream->resources_cache();
  // FIXME DBO
  // auto& hosts_instances_cache = _stream->hosts_instances_cache();
  if (lst.empty())
    return;

  std::list<std::pair<uint64_t, uint64_t>> keys;
  mysql& mysql = _stream->get_mysql();
  if (!_add_host_resources_stmt) {
    std::string query(
        "INSERT INTO resources (id,parent_id,internal_id,type,status, "
        "status_ordered, status_confirmed,max_check_attempts,poller_id,"
        "severity_id,name,alias,address, parent_name,icon_id,notes_url,"
        "notes,action_url, notifications_enabled,passive_checks_enabled,"
        "active_checks_enabled, enabled) VALUES"
        "(?,?,?,?,4,1,1,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON "
        "DUPLICATE KEY UPDATE parent_id=VALUES(parent_id), "
        "internal_id=VALUES(internal_id),type=VALUES(type), "
        "status=COALESCE(status, 4), status_ordered=COALESCE(status_ordered,1),"
        "status_confirmed=COALESCE(status_confirmed,1),"
        "max_check_attempts=VALUES(max_check_attempts),poller_id="
        "VALUES(poller_id),severity_id=VALUES(severity_id),name=VALUES(name),"
        "alias=VALUES(alias),address=VALUES(address),parent_name=VALUES(parent_"
        "name),icon_id=VALUES(icon_id),notes_url=VALUES(notes_url),notes="
        "VALUES(notes),action_url=VALUES(action_url),notifications_enabled="
        "VALUES(notifications_enabled),passive_checks_enabled=VALUES(passive_"
        "checks_enabled),active_checks_enabled=VALUES(active_checks_enabled),"
        "enabled=VALUES(enabled)");
    _add_host_resources_stmt = std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_add_host_resources_stmt);
  }
  auto bind = _add_host_resources_stmt->create_bind();

  auto& hosts_cache = _stream->host_name_id_cache();
  auto& bc = config::applier::state::instance().cache();
  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.host_id(), 0);
    keys.push_back(key);
    _logger->debug("Processing host resource '{}' (id {} - poller {})",
                   msg.host_name(), msg.host_id(), msg.poller_id());
    assert(msg.poller_id() != 0);
    // hosts_instances_cache.insert_or_assign(msg.host_id(), msg.poller_id());

    bind->set_value_as_u64(0, msg.host_id());
    bind->set_value_as_u64(1, 0);
    bind->set_null_u64(2);
    bind->set_value_as_u32(3, 1);
    bind->set_value_as_u32(4, msg.max_check_attempts());
    bind->set_value_as_u64(5, msg.poller_id());
    if (msg.has_severity_id()) {
      uint64_t db_sid = bc.get_db_id_for_severity(msg.severity_id(), 1);
      _logger->trace("host {} has severity_id config={} => db_sid={}",
                     msg.host_id(), msg.severity_id(), db_sid);
      if (db_sid)
        bind->set_value_as_u64(6, db_sid);
      else
        bind->set_null_u64(6);
    } else {
      _logger->trace("host {} has no severity_id", msg.host_id());
      bind->set_null_u64(6);
    }
    bind->set_value_as_str(
        7, common::truncate_utf8(msg.host_name(),
                                 get_centreon_storage_resources_col_size(
                                     centreon_storage_resources_name)));
    bind->set_value_as_str(
        8, common::truncate_utf8(msg.alias(),
                                 get_centreon_storage_resources_col_size(
                                     centreon_storage_resources_alias)));
    bind->set_value_as_str(
        9, common::truncate_utf8(msg.address(),
                                 get_centreon_storage_resources_col_size(
                                     centreon_storage_resources_address)));
    bind->set_null_str(10);
    if (msg.has_icon_id())
      bind->set_value_as_u64(11, msg.icon_id());
    else
      bind->set_null_u64(11);
    bind->set_value_as_str(
        12, common::truncate_utf8(msg.notes_url(),
                                  get_centreon_storage_resources_col_size(
                                      centreon_storage_resources_notes_url)));
    bind->set_value_as_str(
        13, common::truncate_utf8(msg.notes(),
                                  get_centreon_storage_resources_col_size(
                                      centreon_storage_resources_notes)));
    bind->set_value_as_str(
        14, common::truncate_utf8(msg.action_url(),
                                  get_centreon_storage_resources_col_size(
                                      centreon_storage_resources_action_url)));
    bind->set_value_as_bool(15, msg.notifications_enabled());
    bind->set_value_as_bool(16, msg.checks_passive());
    bind->set_value_as_bool(17, msg.checks_active());
    bind->set_value_as_bool(18, true);
    bind->next_row();
    _add_customvariables_mariadb(msg.host_id(), 0, msg.customvariables());
    _logger->debug("Adding to cache host '{}' with id {}", msg.host_name(),
                   msg.host_id());
    hosts_cache.insert_or_assign(msg.host_name(), msg.host_id());
    _logger->debug("host cache has {} items now", hosts_cache.size());
  }
  _add_host_resources_stmt->set_bind(std::move(bind));

  try {
    std::promise<uint64_t> promise;
    std::future<uint64_t> future = promise.get_future();
    mysql.run_statement_and_get_int<uint64_t>(
        *_add_host_resources_stmt, std::move(promise),
        mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& k : keys) {
      auto inserted = cache.emplace(k, first_id);
      if (inserted.second) {
        _logger->trace("Host resource with id {} has resource_id {}", k,
                       first_id);
        first_id++;
      } else {
        _logger->trace("Host resource with id {} has resource_id {}", k,
                       inserted.first->second);
      }
    }
  } catch (const std::exception& e) {
    _logger->error("Error while executing <<_add_host_resources>>: {}",
                   e.what());
  }
}

/**
 * @brief Add hosts into the resources database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_host_resources_mysql(
    const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
        lst) {
  if (lst.empty())
    return;

  auto& cache = _stream->resources_cache();
  // FIXME DBO
  // auto& hosts_instances_cache = _stream->hosts_instances_cache();
  mysql& mysql = _stream->get_mysql();
  std::list<std::pair<uint64_t, uint64_t>> keys;

  std::vector<std::string> values;
  auto& hosts_cache = _stream->host_name_id_cache();
  auto& bc = config::applier::state::instance().cache();
  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.host_id(), 0);
    keys.push_back(key);

    uint64_t sid = 0;
    if (msg.has_severity_id())
      sid = bc.get_db_id_for_severity(msg.severity_id(), 1);
    std::string value(fmt::format(
        "({},{},NULL,{},4,1,1,{},{},{},'{}','{}','{}',NULL,{},'{}','{}','{}',{}"
        ",{},{},1)",
        msg.host_id(), 0, 1, msg.max_check_attempts(), msg.poller_id(),
        sid ? fmt::to_string(sid) : "NULL",
        misc::string::escape(msg.host_name(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_name)),
        misc::string::escape(msg.alias(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_alias)),
        misc::string::escape(msg.address(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_address)),
        msg.icon_id(),
        misc::string::escape(msg.notes_url(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_notes_url)),
        misc::string::escape(msg.notes(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_notes)),
        misc::string::escape(msg.action_url(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_action_url)),
        msg.notifications_enabled(), msg.checks_passive(),
        msg.checks_active()));
    values.emplace_back(value);
    _add_customvariables_mysql(msg.host_id(), 0, msg.customvariables());
    hosts_cache.insert_or_assign(msg.host_name(), msg.host_id());
    // hosts_instances_cache.insert_or_assign(msg.host_id(), msg.poller_id());
  }
  std::string query(fmt::format(
      "INSERT INTO resources (id,parent_id,internal_id,type,status, "
      "status_ordered, status_confirmed,max_check_attempts,poller_id,"
      "severity_id,name,alias,address, parent_name,icon_id,notes_url,"
      "notes,action_url, notifications_enabled,passive_checks_enabled,"
      "active_checks_enabled, enabled) VALUES {} ON DUPLICATE KEY UPDATE "
      "parent_id=VALUES(parent_id),internal_id=VALUES(internal_id),type=VALUES("
      "type),max_check_attempts=VALUES(max_check_attempts),poller_id=VALUES("
      "poller_id),severity_id=VALUES(severity_id),name=VALUES(name),alias="
      "VALUES(alias),address=VALUES(address),parent_name=VALUES(parent_name),"
      "icon_id=VALUES(icon_id),notes_url=VALUES(notes_url),notes=VALUES(notes),"
      "action_url=VALUES(action_url),notifications_enabled=VALUES("
      "notifications_enabled),passive_checks_enabled=VALUES(passive_checks_"
      "enabled),active_checks_enabled=VALUES(active_checks_enabled),enabled="
      "VALUES(enabled)",
      fmt::join(values, ",")));

  try {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();
    mysql.run_query_and_get_int(query, std::move(promise),
                                mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& k : keys) {
      auto inserted = cache.emplace(k, first_id);
      if (inserted.second) {
        _logger->trace("Host resource with id {} has resource_id {}", k,
                       first_id);
        first_id++;
      } else
        _logger->trace("Host resource with id {} has resource_id {}", k,
                       inserted.first->second);
    }
  } catch (const std::exception& e) {
    _logger->error("Error while executing <<_add_host_resources>>: {}",
                   e.what());
  }
}

/**
 * @brief Add services into the database. (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_services_mariadb(
    const ::google::protobuf::RepeatedPtrField<engine::configuration::Service>&
        lst) {
  if (lst.empty()) {
    _logger->debug("No need to add/update services, list empty");
    return;
  }

  mysql& mysql = _stream->get_mysql();
  if (!_add_services_stmt) {
    std::string query(
        "INSERT INTO services "
        "(host_id,description,service_id,action_url,active_checks, "
        "check_command, check_freshness,check_interval,check_period,"
        "default_active_checks,default_event_handler_enabled,"
        "default_flap_detection,default_notify,default_passive_checks,"
        "default_process_perfdata,display_name,"
        "enabled,event_handler,event_handler_enabled,first_notification_delay,"
        "flap_detection,flap_detection_on_critical,flap_detection_on_ok,flap_"
        "detection_on_unknown,flap_detection_on_warning,freshness_threshold,"
        "high_flap_threshold,icon_image,icon_image_alt,low_flap_threshold,max_"
        "check_attempts,notes,notes_url,notification_interval,notification_"
        "period,notify,notify_on_critical,notify_on_downtime,notify_on_"
        "flapping,notify_on_recovery,notify_on_unknown,notify_on_warning,"
        "obsess_over_service,passive_checks,process_perfdata,retain_nonstatus_"
        "information,retain_status_information,retry_interval,stalk_on_"
        "critical,stalk_on_ok,stalk_on_unknown,stalk_on_warning,state, "
        "state_type) VALUES "
        "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?"
        ",?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,4,1) ON DUPLICATE KEY UPDATE "
        "description=VALUES(description),action_url=VALUES(action_url),active_"
        "checks=VALUES(active_checks),check_command=VALUES(check_command),"
        "check_freshness=VALUES(check_freshness),check_interval=VALUES(check_"
        "interval),check_period=VALUES(check_period),default_active_checks="
        "VALUES(default_active_checks),default_event_handler_enabled=VALUES("
        "default_event_handler_enabled),default_flap_detection=VALUES(default_"
        "flap_detection),default_notify=VALUES(default_notify),default_passive_"
        "checks=VALUES(default_passive_checks),default_process_perfdata=VALUES("
        "default_process_perfdata),display_name=VALUES(display_name),enabled="
        "VALUES(enabled),event_handler=VALUES(event_handler),event_handler_"
        "enabled=VALUES(event_handler_enabled),first_notification_delay=VALUES("
        "first_notification_delay),flap_detection=VALUES(flap_detection),flap_"
        "detection_on_critical=VALUES(flap_detection_on_critical),flap_"
        "detection_on_ok=VALUES(flap_detection_on_ok),flap_detection_on_"
        "unknown=VALUES(flap_detection_on_unknown),flap_detection_on_warning="
        "VALUES(flap_detection_on_warning),freshness_threshold=VALUES("
        "freshness_threshold),high_flap_threshold=VALUES(high_flap_threshold),"
        "icon_image=VALUES(icon_image),icon_image_alt=VALUES(icon_image_alt),"
        "low_flap_threshold=VALUES(low_flap_threshold),max_check_attempts="
        "VALUES(max_check_attempts),notes=VALUES(notes),notes_url=VALUES(notes_"
        "url),notification_interval=VALUES(notification_interval),notification_"
        "period=VALUES(notification_period),notify=VALUES(notify),notify_on_"
        "critical=VALUES(notify_on_critical),notify_on_downtime=VALUES(notify_"
        "on_downtime),notify_on_flapping=VALUES(notify_on_flapping),notify_on_"
        "recovery=VALUES(notify_on_recovery),notify_on_unknown=VALUES(notify_"
        "on_unknown),notify_on_warning=VALUES(notify_on_warning),obsess_over_"
        "service=VALUES(obsess_over_service),passive_checks=VALUES(passive_"
        "checks),process_perfdata=VALUES(process_perfdata),retain_nonstatus_"
        "information=VALUES(retain_nonstatus_information),retain_status_"
        "information=VALUES(retain_status_information),retry_interval=VALUES("
        "retry_interval),stalk_on_critical=VALUES(stalk_on_critical),stalk_on_"
        "ok=VALUES(stalk_on_ok),stalk_on_unknown=VALUES(stalk_on_unknown),"
        "stalk_on_warning=VALUES(stalk_on_warning),state=COALESCE(state, 4),"
        "state_type=COALESCE(state_type, 1)");
    _add_services_stmt = std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_add_services_stmt);
  }
  auto bind = _add_services_stmt->create_bind();

  uint32_t count = 0;
  for (const auto& msg : lst) {
    _logger->debug("Processing service {}:{}", msg.host_id(), msg.service_id());
    bind->set_value_as_i32(0, msg.host_id());
    bind->set_value_as_str(
        1, common::truncate_utf8(msg.service_description(),
                                 get_centreon_storage_services_col_size(
                                     centreon_storage_services_description)));
    bind->set_value_as_i32(2, msg.service_id());
    bind->set_value_as_str(
        3, common::truncate_utf8(msg.action_url(),
                                 get_centreon_storage_services_col_size(
                                     centreon_storage_services_action_url)));
    bind->set_value_as_bool(4, msg.checks_active());
    bind->set_value_as_str(
        5, common::truncate_utf8(msg.check_command(),
                                 get_centreon_storage_services_col_size(
                                     centreon_storage_services_check_command)));
    bind->set_value_as_bool(6, msg.check_freshness());
    bind->set_value_as_f64(7, msg.check_interval());
    bind->set_value_as_str(
        8, common::truncate_utf8(msg.check_period(),
                                 get_centreon_storage_services_col_size(
                                     centreon_storage_services_check_period)));
    bind->set_value_as_bool(9, msg.checks_active());
    bind->set_value_as_bool(10, msg.event_handler_enabled());
    bind->set_value_as_bool(11, msg.flap_detection_enabled());
    bind->set_value_as_bool(12, msg.notifications_enabled());
    bind->set_value_as_bool(13, msg.checks_passive());
    bind->set_value_as_bool(14, msg.process_perf_data());
    bind->set_value_as_str(
        15, common::truncate_utf8(msg.display_name(),
                                  get_centreon_storage_services_col_size(
                                      centreon_storage_services_display_name)));
    bind->set_value_as_bool(16, true);
    bind->set_value_as_str(
        17,
        common::truncate_utf8(msg.event_handler(),
                              get_centreon_storage_services_col_size(
                                  centreon_storage_services_event_handler)));
    bind->set_value_as_bool(18, msg.event_handler_enabled());
    bind->set_value_as_f64(19, msg.first_notification_delay());
    bind->set_value_as_bool(20, msg.flap_detection_enabled());
    bind->set_value_as_bool(21, msg.flap_detection_options() &
                                    ActionServiceOn::action_svc_critical);
    bind->set_value_as_bool(
        22, msg.flap_detection_options() & ActionServiceOn::action_svc_ok);
    bind->set_value_as_bool(
        23, msg.flap_detection_options() & ActionServiceOn::action_svc_unknown);
    bind->set_value_as_bool(
        24, msg.flap_detection_options() & ActionServiceOn::action_svc_warning);
    bind->set_value_as_f64(25, msg.freshness_threshold());
    bind->set_value_as_f64(26, msg.high_flap_threshold());
    bind->set_value_as_str(
        27, common::truncate_utf8(msg.icon_image(),
                                  get_centreon_storage_services_col_size(
                                      centreon_storage_services_icon_image)));
    bind->set_value_as_str(
        28,
        common::truncate_utf8(msg.icon_image_alt(),
                              get_centreon_storage_services_col_size(
                                  centreon_storage_services_icon_image_alt)));
    bind->set_value_as_f64(29, msg.low_flap_threshold());
    bind->set_value_as_i32(30, msg.max_check_attempts());
    bind->set_value_as_str(
        31, common::truncate_utf8(msg.notes(),
                                  get_centreon_storage_services_col_size(
                                      centreon_storage_services_notes)));
    bind->set_value_as_str(
        32, common::truncate_utf8(msg.notes_url(),
                                  get_centreon_storage_services_col_size(
                                      centreon_storage_services_notes_url)));
    bind->set_value_as_f64(33, msg.notification_interval());
    if (msg.has_notification_period())
      bind->set_value_as_str(
          34, common::truncate_utf8(
                  msg.notification_period(),
                  get_centreon_storage_services_col_size(
                      centreon_storage_services_notification_period)));
    else
      bind->set_null_str(34);
    bind->set_value_as_bool(35, msg.notifications_enabled());
    bind->set_value_as_bool(
        36, msg.notification_options() & ActionServiceOn::action_svc_critical);
    bind->set_value_as_bool(
        37, msg.notification_options() & ActionServiceOn::action_svc_downtime);
    bind->set_value_as_bool(
        38, msg.notification_options() & ActionServiceOn::action_svc_flapping);
    bind->set_value_as_bool(
        39, msg.notification_options() & ActionServiceOn::action_svc_ok);
    bind->set_value_as_bool(
        40, msg.notification_options() & ActionServiceOn::action_svc_unknown);
    bind->set_value_as_bool(
        41, msg.notification_options() & ActionServiceOn::action_svc_warning);
    bind->set_value_as_bool(42, msg.obsess_over_service());
    bind->set_value_as_bool(43, msg.checks_passive());
    bind->set_value_as_bool(44, msg.process_perf_data());
    bind->set_value_as_bool(45, msg.retain_nonstatus_information());
    bind->set_value_as_bool(46, msg.retain_status_information());
    bind->set_value_as_f64(47, msg.retry_interval());
    bind->set_value_as_bool(
        48, msg.stalking_options() & ActionServiceOn::action_svc_critical);
    bind->set_value_as_bool(
        49, msg.stalking_options() & ActionServiceOn::action_svc_ok);
    bind->set_value_as_bool(
        50, msg.stalking_options() & ActionServiceOn::action_svc_unknown);
    bind->set_value_as_bool(
        51, msg.stalking_options() & ActionServiceOn::action_svc_warning);
    bind->next_row();
    count++;
  }
  _logger->debug("Adding/updating {} services", count);
  _add_services_stmt->set_bind(std::move(bind));
  mysql.run_statement(*_add_services_stmt);
}

/**
 * @brief Add services into the database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_services_mysql(
    const ::google::protobuf::RepeatedPtrField<engine::configuration::Service>&
        lst) {
  if (lst.empty())
    return;

  mysql& mysql = _stream->get_mysql();

  std::vector<std::string> values;
  std::string notification_period;
  for (const auto& msg : lst) {
    if (msg.has_notification_period()) {
      notification_period = fmt::format(
          "'{}'", misc::string::escape(
                      msg.notification_period(),
                      get_centreon_storage_services_col_size(
                          centreon_storage_services_notification_period)));
    } else
      notification_period = "NULL";
    std::string value(fmt::format(
        "({},'{}',{},'{}',{},'{}',{},{},'{}',{},{},{},{},{},{},'{}',1,'{}',{},{"
        "},{},{},{},{},{},{},{},'{}','{}',{},{},'{}','{}',{},{},{},{},{},{},{"
        "},{},{},{},{},{},{},{},{},{},{},{},{},4,1)",
        msg.host_id(),
        misc::string::escape(msg.service_description(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_description)),
        msg.service_id(),
        misc::string::escape(msg.action_url(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_action_url)),
        msg.checks_active() ? 1 : 0,
        misc::string::escape(msg.check_command(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_check_command)),
        msg.check_freshness() ? 1 : 0, msg.check_interval(),
        misc::string::escape(msg.check_period(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_check_period)),
        msg.checks_active() ? 1 : 0, msg.event_handler_enabled() ? 1 : 0,
        msg.flap_detection_enabled() ? 1 : 0,
        msg.notifications_enabled() ? 1 : 0, msg.checks_passive() ? 1 : 0,
        msg.process_perf_data() ? 1 : 0,
        misc::string::escape(msg.display_name(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_display_name)),
        misc::string::escape(msg.event_handler(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_event_handler)),
        msg.event_handler_enabled() ? 1 : 0, msg.first_notification_delay(),
        msg.flap_detection_enabled() ? 1 : 0,
        msg.flap_detection_options() & ActionServiceOn::action_svc_critical ? 1
                                                                            : 0,
        msg.flap_detection_options() & ActionServiceOn::action_svc_ok ? 1 : 0,
        msg.flap_detection_options() & ActionServiceOn::action_svc_unknown ? 1
                                                                           : 0,
        msg.flap_detection_options() & ActionServiceOn::action_svc_warning ? 1
                                                                           : 0,
        msg.freshness_threshold(), msg.high_flap_threshold(),
        misc::string::escape(msg.icon_image(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_icon_image)),
        misc::string::escape(msg.icon_image_alt(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_icon_image_alt)),
        msg.low_flap_threshold(), msg.max_check_attempts(),
        misc::string::escape(msg.notes(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_notes)),
        misc::string::escape(msg.notes_url(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_notes_url)),
        msg.notification_interval(), notification_period,
        msg.notifications_enabled() ? 1 : 0,
        msg.notification_options() & ActionServiceOn::action_svc_critical ? 1
                                                                          : 0,
        msg.notification_options() & ActionServiceOn::action_svc_downtime ? 1
                                                                          : 0,
        msg.notification_options() & ActionServiceOn::action_svc_flapping ? 1
                                                                          : 0,
        msg.notification_options() & ActionServiceOn::action_svc_ok ? 1 : 0,
        msg.notification_options() & ActionServiceOn::action_svc_unknown ? 1
                                                                         : 0,
        msg.notification_options() & ActionServiceOn::action_svc_warning ? 1
                                                                         : 0,
        msg.obsess_over_service() ? 1 : 0, msg.checks_passive() ? 1 : 0,
        msg.process_perf_data() ? 1 : 0,
        msg.retain_nonstatus_information() ? 1 : 0,
        msg.retain_status_information() ? 1 : 0, msg.retry_interval(),
        msg.stalking_options() & ActionServiceOn::action_svc_critical ? 1 : 0,
        msg.stalking_options() & ActionServiceOn::action_svc_ok ? 1 : 0,
        msg.stalking_options() & ActionServiceOn::action_svc_unknown ? 1 : 0,
        msg.stalking_options() & ActionServiceOn::action_svc_warning ? 1 : 0));
    values.emplace_back(value);
  }
  std::string query(fmt::format(
      "INSERT INTO services "
      "(host_id,description,service_id,action_url,active_checks, "
      "check_command, check_freshness,check_interval,check_period,"
      "default_active_checks,default_event_handler_enabled,"
      "default_flap_detection,default_notify,default_passive_checks,"
      "default_process_perfdata,display_name,"
      "enabled,event_handler,event_handler_enabled,first_notification_delay,"
      "flap_detection,flap_detection_on_critical,flap_detection_on_ok,flap_"
      "detection_on_unknown,flap_detection_on_warning,freshness_threshold,"
      "high_flap_threshold,icon_image,icon_image_alt,low_flap_threshold,max_"
      "check_attempts,notes,notes_url,notification_interval,notification_"
      "period,notify,notify_on_critical,notify_on_downtime,notify_on_"
      "flapping,notify_on_recovery,notify_on_unknown,notify_on_warning,"
      "obsess_over_service,passive_checks,process_perfdata,retain_nonstatus_"
      "information,retain_status_information,retry_interval,stalk_on_"
      "critical,stalk_on_ok,stalk_on_unknown,stalk_on_warning,state, "
      "state_type) VALUES {} ON DUPLICATE KEY UPDATE "
      "description=VALUES(description),action_url=VALUES(action_url),active_"
      "checks=VALUES(active_checks),check_command=VALUES(check_command),check_"
      "freshness=VALUES(check_freshness),check_interval=VALUES(check_interval),"
      "check_period=VALUES(check_period),default_active_checks=VALUES(default_"
      "active_checks),default_event_handler_enabled=VALUES(default_event_"
      "handler_enabled),default_flap_detection=VALUES(default_flap_detection),"
      "default_notify=VALUES(default_notify),default_passive_checks=VALUES("
      "default_passive_checks),default_process_perfdata=VALUES(default_process_"
      "perfdata),display_name=VALUES(display_name),enabled=VALUES(enabled),"
      "event_handler=VALUES(event_handler),event_handler_enabled=VALUES(event_"
      "handler_enabled),first_notification_delay=VALUES(first_notification_"
      "delay),flap_detection=VALUES(flap_detection),flap_detection_on_critical="
      "VALUES(flap_detection_on_critical),flap_detection_on_ok=VALUES(flap_"
      "detection_on_ok),flap_detection_on_unknown=VALUES(flap_detection_on_"
      "unknown),flap_detection_on_warning=VALUES(flap_detection_on_warning),"
      "freshness_threshold=VALUES(freshness_threshold),high_flap_threshold="
      "VALUES(high_flap_threshold),icon_image=VALUES(icon_image),icon_image_"
      "alt=VALUES(icon_image_alt),low_flap_threshold=VALUES(low_flap_threshold)"
      ",max_check_attempts=VALUES(max_check_attempts),notes=VALUES(notes),"
      "notes_url=VALUES(notes_url),notification_interval=VALUES(notification_"
      "interval),notification_period=VALUES(notification_period),notify=VALUES("
      "notify),notify_on_critical=VALUES(notify_on_critical),notify_on_"
      "downtime=VALUES(notify_on_downtime),notify_on_flapping=VALUES(notify_on_"
      "flapping),notify_on_recovery=VALUES(notify_on_recovery),notify_on_"
      "unknown=VALUES(notify_on_unknown),notify_on_warning=VALUES(notify_on_"
      "warning),obsess_over_service=VALUES(obsess_over_service),passive_checks="
      "VALUES(passive_checks),process_perfdata=VALUES(process_perfdata),retain_"
      "nonstatus_information=VALUES(retain_nonstatus_information),retain_"
      "status_information=VALUES(retain_status_information),retry_interval="
      "VALUES(retry_interval),stalk_on_critical=VALUES(stalk_on_critical),"
      "stalk_on_ok=VALUES(stalk_on_ok),stalk_on_unknown=VALUES(stalk_on_"
      "unknown),stalk_on_warning=VALUES(stalk_on_warning)",
      fmt::join(values, ",")));
  mysql.run_query(query);
}

/**
 * @brief Disable services in the database. (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_disable_services_mariadb(
    const ::google::protobuf::RepeatedPtrField<
        engine::configuration::HostServiceId>& lst) {
  if (lst.empty())
    return;

  mysql& mysql = _stream->get_mysql();
  if (!_disable_services_stmt) {
    _disable_services_stmt = std::make_unique<mysql_bulk_stmt>(
        "UPDATE services SET enabled=0 WHERE host_id=? AND service_id=?");
    mysql.prepare_statement(*_disable_services_stmt);
  }
  auto* stmt = static_cast<mysql_bulk_stmt*>(_disable_services_stmt.get());
  auto bind = stmt->create_bind();

  for (const auto& msg : lst) {
    bind->set_value_as_i32(0, msg.host_id());
    bind->set_value_as_i32(1, msg.service_id());
    bind->next_row();
  }
  stmt->set_bind(std::move(bind));
  mysql.run_statement(*stmt);
}

/**
 * @brief Disable services in the database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_disable_services_mysql(
    const ::google::protobuf::RepeatedPtrField<
        engine::configuration::HostServiceId>& lst) {
  if (lst.empty())
    return;

  mysql& mysql = _stream->get_mysql();
  if (!_disable_services_stmt) {
    _disable_services_stmt = std::make_unique<mysql_stmt>(
        "UPDATE services SET enabled=0 WHERE host_id=? AND service_id=?");
    mysql.prepare_statement(*_disable_services_stmt);
  }
  for (const auto& msg : lst) {
    _disable_services_stmt->bind_value_as_i32(0, msg.host_id());
    _disable_services_stmt->bind_value_as_i32(1, msg.service_id());
    mysql.run_statement(*_disable_services_stmt);
  }
}

/**
 * @brief Disable services in the database. (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_disable_service_resources_mariadb(
    const ::google::protobuf::RepeatedPtrField<
        engine::configuration::HostServiceId>& lst) {
  mysql& mysql = _stream->get_mysql();
  if (!_disable_service_resources_stmt) {
    _disable_service_resources_stmt = std::make_unique<mysql_bulk_stmt>(
        "UPDATE resources SET enabled=0 WHERE parent_id=? AND id=?");
    mysql.prepare_statement(*_disable_service_resources_stmt);
  }
  auto* stmt =
      static_cast<mysql_bulk_stmt*>(_disable_service_resources_stmt.get());
  auto bind = stmt->create_bind();

  for (const auto& msg : lst) {
    bind->set_value_as_i64(0, msg.host_id());
    bind->set_value_as_i64(1, msg.service_id());
    bind->next_row();
    _logger->trace("Disabling service resource with id {}:{}", msg.host_id(),
                   msg.service_id());
  }
  stmt->set_bind(std::move(bind));
  mysql.run_statement(*stmt);
}

/**
 * @brief Disable services in the database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_disable_service_resources_mysql(
    const ::google::protobuf::RepeatedPtrField<
        engine::configuration::HostServiceId>& lst) {
  mysql& mysql = _stream->get_mysql();
  if (!_disable_service_resources_stmt) {
    _disable_service_resources_stmt = std::make_unique<mysql_stmt>(
        "UPDATE resources SET enabled=0 WHERE parent_id=? AND id=?");
    mysql.prepare_statement(*_disable_service_resources_stmt);
  }
  for (const auto& msg : lst) {
    _disable_service_resources_stmt->bind_value_as_i64(0, msg.host_id());
    _disable_service_resources_stmt->bind_value_as_i64(1, msg.service_id());
    mysql.run_statement(*_disable_service_resources_stmt);
    _logger->trace("Disabling service resource with id {}:{}", msg.host_id(),
                   msg.service_id());
  }
}

/**
 * @brief Add services into the database. (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_anomalydetections_mariadb(
    const ::google::protobuf::RepeatedPtrField<
        engine::configuration::Anomalydetection>& lst) {
  mysql& mysql = _stream->get_mysql();
  if (lst.empty()) {
    _logger->debug("No need to add/update anomaly detections, list empty");
    return;
  }
  if (!_add_anomalydetections_stmt) {
    std::string query(
        "INSERT INTO services "
        "(host_id,description,service_id,action_url,active_checks,check_"
        "freshness,check_interval,default_active_checks,default_event_handler_"
        "enabled,default_flap_detection,default_notify,default_passive_checks,"
        "default_process_perfdata,display_name,enabled,event_handler,event_"
        "handler_enabled,first_notification_delay,flap_detection,flap_"
        "detection_on_critical,flap_detection_on_ok,flap_detection_on_unknown,"
        "flap_detection_on_warning,freshness_threshold,high_flap_threshold,"
        "icon_image,icon_image_alt,low_flap_threshold,max_check_attempts,notes,"
        "notes_url,notification_interval,notification_period,notify,notify_on_"
        "critical,notify_on_downtime,notify_on_flapping,notify_on_recovery,"
        "notify_on_unknown,notify_on_warning,obsess_over_service,passive_"
        "checks,process_perfdata,retain_nonstatus_information,retain_status_"
        "information,retry_interval,stalk_on_critical,stalk_on_ok,stalk_on_"
        "unknown,stalk_on_warning, state, state_type) VALUES "
        "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?"
        ",?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,4,1) ON DUPLICATE KEY UPDATE "
        "description=VALUES(description),action_url=VALUES(action_url),active_"
        "checks=VALUES(active_checks),check_freshness=VALUES(check_freshness),"
        "check_interval=VALUES(check_interval),default_active_checks=VALUES("
        "default_active_checks),default_event_handler_enabled=VALUES(default_"
        "event_handler_enabled),default_flap_detection=VALUES(default_flap_"
        "detection),default_notify=VALUES(default_notify),default_passive_"
        "checks=VALUES(default_passive_checks),default_process_perfdata=VALUES("
        "default_process_perfdata),display_name=VALUES(display_name),enabled="
        "VALUES(enabled),event_handler=VALUES(event_handler),event_handler_"
        "enabled=VALUES(event_handler_enabled),first_notification_delay=VALUES("
        "first_notification_delay),flap_detection=VALUES(flap_detection),flap_"
        "detection_on_critical=VALUES(flap_detection_on_critical),flap_"
        "detection_on_ok=VALUES(flap_detection_on_ok),flap_detection_on_"
        "unknown=VALUES(flap_detection_on_unknown),flap_detection_on_warning="
        "VALUES(flap_detection_on_warning),freshness_threshold=VALUES("
        "freshness_threshold),high_flap_threshold=VALUES(high_flap_threshold),"
        "icon_image=VALUES(icon_image),icon_image_alt=VALUES(icon_image_alt),"
        "low_flap_threshold=VALUES(low_flap_threshold),max_check_attempts="
        "VALUES(max_check_attempts),notes=VALUES(notes),notes_url=VALUES(notes_"
        "url),notification_interval=VALUES(notification_interval),notification_"
        "period=VALUES(notification_period),notify=VALUES(notify),notify_on_"
        "critical=VALUES(notify_on_critical),notify_on_downtime=VALUES(notify_"
        "on_downtime),notify_on_flapping=VALUES(notify_on_flapping),notify_on_"
        "recovery=VALUES(notify_on_recovery),notify_on_unknown=VALUES(notify_"
        "on_unknown),notify_on_warning=VALUES(notify_on_warning),obsess_over_"
        "service=VALUES(obsess_over_service),passive_checks=VALUES(passive_"
        "checks),process_perfdata=VALUES(process_perfdata),retain_nonstatus_"
        "information=VALUES(retain_nonstatus_information),retain_status_"
        "information=VALUES(retain_status_information),retry_interval=VALUES("
        "retry_interval),stalk_on_critical=VALUES(stalk_on_critical),stalk_on_"
        "ok=VALUES(stalk_on_ok),stalk_on_unknown=VALUES(stalk_on_unknown),"
        "stalk_on_warning=VALUES(stalk_on_warning),state=COALESCE(state,4), "
        "state_type=COALESCE(state_type, 1)");
    _add_anomalydetections_stmt = std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_add_anomalydetections_stmt);
  }
  auto bind = _add_anomalydetections_stmt->create_bind();

  for (const auto& msg : lst) {
    bind->set_value_as_i32(0, msg.host_id());
    bind->set_value_as_str(
        1, common::truncate_utf8(msg.service_description(),
                                 get_centreon_storage_services_col_size(
                                     centreon_storage_services_description)));
    bind->set_value_as_i32(2, msg.service_id());
    bind->set_value_as_str(
        3, common::truncate_utf8(msg.action_url(),
                                 get_centreon_storage_services_col_size(
                                     centreon_storage_services_action_url)));
    bind->set_value_as_bool(4, msg.checks_active());
    bind->set_value_as_bool(5, msg.check_freshness());
    bind->set_value_as_f64(6, msg.check_interval());
    bind->set_value_as_bool(7, msg.checks_active());
    bind->set_value_as_bool(8, msg.event_handler_enabled());
    bind->set_value_as_bool(9, msg.flap_detection_enabled());
    bind->set_value_as_bool(10, msg.notifications_enabled());
    bind->set_value_as_bool(11, msg.checks_passive());
    bind->set_value_as_bool(12, msg.process_perf_data());
    bind->set_value_as_str(
        13, common::truncate_utf8(msg.display_name(),
                                  get_centreon_storage_services_col_size(
                                      centreon_storage_services_display_name)));
    bind->set_value_as_bool(14, true);
    bind->set_value_as_str(
        15,
        common::truncate_utf8(msg.event_handler(),
                              get_centreon_storage_services_col_size(
                                  centreon_storage_services_event_handler)));
    bind->set_value_as_bool(16, msg.event_handler_enabled());
    bind->set_value_as_f64(17, msg.first_notification_delay());
    bind->set_value_as_bool(18, msg.flap_detection_enabled());
    bind->set_value_as_bool(19, msg.flap_detection_options() &
                                    ActionServiceOn::action_svc_critical);
    bind->set_value_as_bool(
        20, msg.flap_detection_options() & ActionServiceOn::action_svc_ok);
    bind->set_value_as_bool(
        21, msg.flap_detection_options() & ActionServiceOn::action_svc_unknown);
    bind->set_value_as_bool(
        22, msg.flap_detection_options() & ActionServiceOn::action_svc_warning);
    bind->set_value_as_f64(23, msg.freshness_threshold());
    bind->set_value_as_f64(24, msg.high_flap_threshold());
    bind->set_value_as_str(
        25, common::truncate_utf8(msg.icon_image(),
                                  get_centreon_storage_services_col_size(
                                      centreon_storage_services_icon_image)));
    bind->set_value_as_str(
        26,
        common::truncate_utf8(msg.icon_image_alt(),
                              get_centreon_storage_services_col_size(
                                  centreon_storage_services_icon_image_alt)));
    bind->set_value_as_f64(27, msg.low_flap_threshold());
    bind->set_value_as_i32(28, msg.max_check_attempts());
    bind->set_value_as_str(
        29, common::truncate_utf8(msg.notes(),
                                  get_centreon_storage_services_col_size(
                                      centreon_storage_services_notes)));
    bind->set_value_as_str(
        30, common::truncate_utf8(msg.notes_url(),
                                  get_centreon_storage_services_col_size(
                                      centreon_storage_services_notes_url)));
    bind->set_value_as_f64(31, msg.notification_interval());
    if (msg.has_notification_period())
      bind->set_value_as_str(
          32, common::truncate_utf8(
                  msg.notification_period(),
                  get_centreon_storage_services_col_size(
                      centreon_storage_services_notification_period)));
    else
      bind->set_null_str(32);
    bind->set_value_as_bool(33, msg.notifications_enabled());
    bind->set_value_as_bool(
        34, msg.notification_options() & ActionServiceOn::action_svc_critical);
    bind->set_value_as_bool(
        35, msg.notification_options() & ActionServiceOn::action_svc_downtime);
    bind->set_value_as_bool(
        36, msg.notification_options() & ActionServiceOn::action_svc_flapping);
    bind->set_value_as_bool(
        37, msg.notification_options() & ActionServiceOn::action_svc_ok);
    bind->set_value_as_bool(
        38, msg.notification_options() & ActionServiceOn::action_svc_unknown);
    bind->set_value_as_bool(
        39, msg.notification_options() & ActionServiceOn::action_svc_warning);
    bind->set_value_as_bool(40, msg.obsess_over_service());
    bind->set_value_as_bool(41, msg.checks_passive());
    bind->set_value_as_bool(42, msg.process_perf_data());
    bind->set_value_as_bool(43, msg.retain_nonstatus_information());
    bind->set_value_as_bool(44, msg.retain_status_information());
    bind->set_value_as_f64(45, msg.retry_interval());
    bind->set_value_as_bool(
        46, msg.stalking_options() & ActionServiceOn::action_svc_critical);
    bind->set_value_as_bool(
        47, msg.stalking_options() & ActionServiceOn::action_svc_ok);
    bind->set_value_as_bool(
        48, msg.stalking_options() & ActionServiceOn::action_svc_unknown);
    bind->set_value_as_bool(
        49, msg.stalking_options() & ActionServiceOn::action_svc_warning);
    bind->next_row();
  }
  _add_anomalydetections_stmt->set_bind(std::move(bind));
  mysql.run_statement(*_add_anomalydetections_stmt);
}

/**
 * @brief Add services into the database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_anomalydetections_mysql(
    const ::google::protobuf::RepeatedPtrField<
        engine::configuration::Anomalydetection>& lst) {
  mysql& mysql = _stream->get_mysql();
  if (lst.empty()) {
    _logger->debug("No need to add/update anomaly detections, list empty");
    return;
  }

  std::vector<std::string> values;
  values.reserve(lst.size());
  std::string notification_period;
  for (const auto& msg : lst) {
    if (msg.has_notification_period()) {
      notification_period = fmt::format(
          "'{}'", misc::string::escape(
                      msg.notification_period(),
                      get_centreon_storage_services_col_size(
                          centreon_storage_services_notification_period)));
    } else
      notification_period = "NULL";

    std::string value(fmt::format(
        "({},'{}',{},'{}',{},{},{},{},{},{},{},{},{},'{}',1,'{}',{},{},{},{},{}"
        ",{},{},{},{},'{}','{}',{},{},'{}','{}',{},{},{},{},{},{},{},{},{},{}"
        ",{},{},{},{},{},{},{},{},{},4,1)",
        msg.host_id(),
        misc::string::escape(msg.service_description(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_description)),
        msg.service_id(),
        misc::string::escape(msg.action_url(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_action_url)),
        msg.checks_active(), msg.check_freshness(), msg.check_interval(),
        msg.checks_active(), msg.event_handler_enabled(),
        msg.flap_detection_enabled(), msg.notifications_enabled(),
        msg.checks_passive(), msg.process_perf_data(),
        misc::string::escape(msg.display_name(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_display_name)),
        misc::string::escape(msg.event_handler(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_event_handler)),
        msg.event_handler_enabled(), msg.first_notification_delay(),
        msg.flap_detection_enabled(),
        msg.flap_detection_options() & ActionServiceOn::action_svc_critical ? 1
                                                                            : 0,
        msg.flap_detection_options() & ActionServiceOn::action_svc_ok ? 1 : 0,
        msg.flap_detection_options() & ActionServiceOn::action_svc_unknown ? 1
                                                                           : 0,
        msg.flap_detection_options() & ActionServiceOn::action_svc_warning ? 1
                                                                           : 0,
        msg.freshness_threshold(), msg.high_flap_threshold(),
        misc::string::escape(msg.icon_image(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_icon_image)),
        misc::string::escape(msg.icon_image_alt(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_icon_image_alt)),
        msg.low_flap_threshold(), msg.max_check_attempts(),
        misc::string::escape(msg.notes(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_notes)),
        misc::string::escape(msg.notes_url(),
                             get_centreon_storage_services_col_size(
                                 centreon_storage_services_notes_url)),
        msg.notification_interval(), notification_period,
        msg.notifications_enabled(),
        msg.notification_options() & ActionServiceOn::action_svc_critical ? 1
                                                                          : 0,
        msg.notification_options() & ActionServiceOn::action_svc_downtime ? 1
                                                                          : 0,
        msg.notification_options() & ActionServiceOn::action_svc_flapping ? 1
                                                                          : 0,
        msg.notification_options() & ActionServiceOn::action_svc_ok ? 1 : 0,
        msg.notification_options() & ActionServiceOn::action_svc_unknown ? 1
                                                                         : 0,
        msg.notification_options() & ActionServiceOn::action_svc_warning ? 1
                                                                         : 0,
        msg.obsess_over_service(), msg.checks_passive(),
        msg.process_perf_data(), msg.retain_nonstatus_information(),
        msg.retain_status_information(), msg.retry_interval(),
        msg.stalking_options() & ActionServiceOn::action_svc_critical ? 1 : 0,
        msg.stalking_options() & ActionServiceOn::action_svc_ok ? 1 : 0,
        msg.stalking_options() & ActionServiceOn::action_svc_unknown ? 1 : 0,
        msg.stalking_options() & ActionServiceOn::action_svc_warning ? 1 : 0));
    values.emplace_back(value);
  }
  std::string query(fmt::format(
      "INSERT INTO services "
      "(host_id,description,service_id,action_url,active_checks,check_"
      "freshness,check_interval,default_active_checks,default_event_handler_"
      "enabled,default_flap_detection,default_notify,default_passive_checks,"
      "default_process_perfdata,display_name,enabled,event_handler,event_"
      "handler_enabled,first_notification_delay,flap_detection,flap_"
      "detection_on_critical,flap_detection_on_ok,flap_detection_on_unknown,"
      "flap_detection_on_warning,freshness_threshold,high_flap_threshold,"
      "icon_image,icon_image_alt,low_flap_threshold,max_check_attempts,notes,"
      "notes_url,notification_interval,notification_period,notify,notify_on_"
      "critical,notify_on_downtime,notify_on_flapping,notify_on_recovery,"
      "notify_on_unknown,notify_on_warning,obsess_over_service,passive_"
      "checks,process_perfdata,retain_nonstatus_information,retain_status_"
      "information,retry_interval,stalk_on_critical,stalk_on_ok,stalk_on_"
      "unknown,stalk_on_warning, state, state_type) VALUES {} ON DUPLICATE KEY "
      "UPDATE "
      "description=VALUES(description),action_url=VALUES(action_url),active_"
      "checks=VALUES(active_checks),check_freshness=VALUES(check_freshness),"
      "check_interval=VALUES(check_interval),default_active_checks=VALUES("
      "default_active_checks),default_event_handler_enabled=VALUES(default_"
      "event_handler_enabled),default_flap_detection=VALUES(default_flap_"
      "detection),default_notify=VALUES(default_notify),default_passive_checks="
      "VALUES(default_passive_checks),default_process_perfdata=VALUES(default_"
      "process_perfdata),display_name=VALUES(display_name),enabled=VALUES("
      "enabled),event_handler=VALUES(event_handler),event_handler_enabled="
      "VALUES(event_handler_enabled),first_notification_delay=VALUES(first_"
      "notification_delay),flap_detection=VALUES(flap_detection),flap_"
      "detection_on_critical=VALUES(flap_detection_on_critical),flap_detection_"
      "on_ok=VALUES(flap_detection_on_ok),flap_detection_on_unknown=VALUES("
      "flap_detection_on_unknown),flap_detection_on_warning=VALUES(flap_"
      "detection_on_warning),freshness_threshold=VALUES(freshness_threshold),"
      "high_flap_threshold=VALUES(high_flap_threshold),icon_image=VALUES(icon_"
      "image),icon_image_alt=VALUES(icon_image_alt),low_flap_threshold=VALUES("
      "low_flap_threshold),max_check_attempts=VALUES(max_check_attempts),notes="
      "VALUES(notes),notes_url=VALUES(notes_url),notification_interval=VALUES("
      "notification_interval),notification_period=VALUES(notification_period),"
      "notify=VALUES(notify),notify_on_critical=VALUES(notify_on_critical),"
      "notify_on_downtime=VALUES(notify_on_downtime),notify_on_flapping=VALUES("
      "notify_on_flapping),notify_on_recovery=VALUES(notify_on_recovery),"
      "notify_on_unknown=VALUES(notify_on_unknown),notify_on_warning=VALUES("
      "notify_on_warning),obsess_over_service=VALUES(obsess_over_service),"
      "passive_checks=VALUES(passive_checks),process_perfdata=VALUES(process_"
      "perfdata),retain_nonstatus_information=VALUES(retain_nonstatus_"
      "information),retain_status_information=VALUES(retain_status_information)"
      ",retry_interval=VALUES(retry_interval),stalk_on_critical=VALUES(stalk_"
      "on_critical),stalk_on_ok=VALUES(stalk_on_ok),stalk_on_unknown=VALUES("
      "stalk_on_unknown),stalk_on_warning=VALUES(stalk_on_warning), "
      "state=COALESCE("
      "state,4), state_type=COALESCE(state_type, 1)",
      fmt::join(values, ",")));
  mysql.run_query(query);
}

static uint32_t get_service_type(const engine::configuration::Service& msg) {
  if (absl::StartsWith(msg.host_name(), "_Module_Meta") &&
      absl::StartsWith(msg.service_description(), "meta_"))
    return 2;  // com::centreon::engine::service_type::METASERVICE
  else if (absl::StartsWith(msg.host_name(), "_Module_BAM") &&
           absl::StartsWith(msg.service_description(), "ba_"))
    return 3;  // com::centreon::engine::service_type::BA
  else
    return 0;  // com::centreon::engine::service_type::SERVICE
}

/**
 * @brief Add services into the resources database. (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_service_resources_mariadb(
    const ::google::protobuf::RepeatedPtrField<engine::configuration::Service>&
        lst) {
  if (lst.empty()) {
    _logger->debug("No service resources to add/update");
    return;
  }

  auto& cache = _stream->resources_cache();
  std::list<std::pair<uint64_t, uint64_t>> keys;
  mysql& mysql = _stream->get_mysql();
  if (!_add_service_resources_stmt) {
    std::string query(
        "INSERT INTO resources "
        "(id,parent_id,internal_id,type,status,status_ordered,"
        "status_confirmed,max_check_attempts,poller_id,severity_id,name,alias,"
        "parent_name, notes_url,notes,action_url,notifications_enabled,"
        "passive_checks_enabled,active_checks_enabled,enabled) VALUES "
        "(?,?,?,?,4,1,1,?,?,?,?,?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE "
        "internal_id=VALUES(internal_id),type=VALUES(type),status=COALESCE("
        "status,4),status_ordered=COALESCE(status_ordered,1),"
        "status_confirmed=COALESCE(status_confirmed,1),"
        "max_check_attempts=VALUES(max_check_attempts),"
        "poller_id=VALUES(poller_id),severity_id=VALUES(severity_id),"
        "name=VALUES(name),alias=VALUES(alias),parent_name=VALUES(parent_name),"
        "notes_url=VALUES(notes_url),notes=VALUES(notes),"
        "action_url=VALUES(action_url),notifications_enabled=VALUES("
        "notifications_enabled),passive_checks_enabled=VALUES(passive_checks_"
        "enabled),active_checks_enabled=VALUES(active_checks_enabled),enabled="
        "VALUES(enabled)");
    _add_service_resources_stmt = std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_add_service_resources_stmt);
  }
  auto bind = _add_service_resources_stmt->create_bind();
  auto& services_cache = _stream->service_description_id_cache();
  auto& global_cache = config::applier::state::instance().cache();

  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.host_id(), msg.service_id());
    keys.push_back(key);

    bind->set_value_as_u64(0, msg.service_id());
    bind->set_value_as_u64(1, msg.host_id());
    bind->set_null_u64(2);
    bind->set_value_as_u32(3, get_service_type(msg));
    bind->set_value_as_u32(4, msg.max_check_attempts());
    auto h = global_cache.host(msg.host_id());
    bind->set_value_as_u64(5, h->obj().instance_id());
    if (msg.has_severity_id()) {
      uint64_t db_sid =
          global_cache.get_db_id_for_severity(msg.severity_id(), 0);
      if (db_sid)
        bind->set_value_as_u64(6, db_sid);
      else
        bind->set_null_u64(6);
    } else
      bind->set_null_u64(6);
    bind->set_value_as_str(
        7, common::truncate_utf8(msg.display_name().empty()
                                     ? msg.service_description()
                                     : msg.display_name(),
                                 get_centreon_storage_resources_col_size(
                                     centreon_storage_resources_name)));
    bind->set_null_str(8);
    bind->set_value_as_str(
        9, common::truncate_utf8(msg.host_name(),
                                 get_centreon_storage_resources_col_size(
                                     centreon_storage_resources_parent_name)));
    bind->set_value_as_str(
        10, common::truncate_utf8(msg.notes_url(),
                                  get_centreon_storage_resources_col_size(
                                      centreon_storage_resources_notes_url)));
    bind->set_value_as_str(
        11, common::truncate_utf8(msg.notes(),
                                  get_centreon_storage_resources_col_size(
                                      centreon_storage_resources_notes)));
    bind->set_value_as_str(
        12, common::truncate_utf8(msg.action_url(),
                                  get_centreon_storage_resources_col_size(
                                      centreon_storage_resources_action_url)));
    bind->set_value_as_bool(13, msg.notifications_enabled());
    bind->set_value_as_bool(14, msg.checks_passive());
    bind->set_value_as_bool(15, msg.checks_active());
    bind->set_value_as_bool(16, true);
    bind->next_row();
    _add_customvariables_mariadb(msg.host_id(), 0, msg.customvariables());
    services_cache.insert_or_assign(
        std::make_pair(msg.host_id(), msg.service_description()),
        msg.service_id());
  }
  _add_service_resources_stmt->set_bind(std::move(bind));

  try {
    std::promise<uint64_t> promise;
    std::future<uint64_t> future = promise.get_future();
    mysql.run_statement_and_get_int<uint64_t>(
        *_add_service_resources_stmt, std::move(promise),
        mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& k : keys) {
      auto inserted = cache.emplace(k, first_id);
      if (inserted.second) {
        _logger->trace("Service resource with id {}:{} has resource_id {}",
                       k.first, k.second, first_id);
        first_id++;
      } else
        _logger->trace("Service resource with id {}:{} has resource_id {}",
                       k.first, k.second, inserted.first->second);
    }
  } catch (const std::exception& e) {
    _logger->error("Error while executing <<_add_service_resources>>: {}",
                   e.what());
  }
}

/**
 * @brief Add services into the resources database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_service_resources_mysql(
    const ::google::protobuf::RepeatedPtrField<engine::configuration::Service>&
        lst) {
  if (lst.empty()) {
    _logger->debug("No service resources to add/update");
    return;
  }

  auto& cache = _stream->resources_cache();
  mysql& mysql = _stream->get_mysql();
  std::list<std::pair<uint64_t, uint64_t>> keys;

  std::vector<std::string> values;
  values.reserve(lst.size());
  auto& services_cache = _stream->service_description_id_cache();
  auto& global_cache = config::applier::state::instance().cache();
  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.host_id(), msg.service_id());
    keys.push_back(key);

    auto h = global_cache.host(msg.host_id());
    uint64_t svc_sid = 0;
    if (msg.has_severity_id())
      svc_sid = global_cache.get_db_id_for_severity(msg.severity_id(), 0);
    std::string value(fmt::format(
        "({},{},NULL,{},4, 1, "
        "1,{},{},{},'{}',NULL,'{}','{}','{}','{}',{},{},{},"
        "1)",
        msg.service_id(), msg.host_id(), get_service_type(msg),
        msg.max_check_attempts(), h->obj().instance_id(),
        svc_sid ? fmt::to_string(svc_sid) : "NULL",
        misc::string::escape(msg.display_name().empty()
                                 ? msg.service_description()
                                 : msg.display_name(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_name)),
        misc::string::escape(msg.host_name(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_parent_name)),
        misc::string::escape(msg.notes_url(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_notes_url)),
        misc::string::escape(msg.notes(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_notes)),
        misc::string::escape(msg.action_url(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_action_url)),
        msg.notifications_enabled(), msg.checks_passive(),
        msg.checks_active()));
    values.emplace_back(value);
    _add_customvariables_mysql(msg.host_id(), 0, msg.customvariables());
    services_cache.insert_or_assign(
        std::make_pair(msg.host_id(), msg.service_description()),
        msg.service_id());
  }
  std::string query(fmt::format(
      "INSERT INTO resources "
      "(id,parent_id,internal_id,type,status,status_ordered,"
      "status_confirmed,max_check_attempts,poller_id,severity_id,name,alias,"
      "parent_name, notes_url,notes,action_url,notifications_enabled,"
      "passive_checks_enabled,active_checks_enabled,enabled) VALUES {} ON "
      "DUPLICATE KEY UPDATE "
      "internal_id=VALUES(internal_id),type=VALUES(type),max_check_attempts="
      "VALUES(max_check_attempts),poller_id=VALUES(poller_id),severity_id="
      "VALUES(severity_id),name=VALUES(name),alias=VALUES(alias),parent_name="
      "VALUES(parent_name),notes_url=VALUES(notes_url),notes=VALUES(notes),"
      "action_url=VALUES(action_url),notifications_enabled=VALUES("
      "notifications_enabled),passive_checks_enabled=VALUES(passive_checks_"
      "enabled),active_checks_enabled=VALUES(active_checks_enabled),enabled="
      "VALUES(enabled)",
      fmt::join(values, ",")));

  try {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();
    mysql.run_query_and_get_int(query, std::move(promise),
                                mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& k : keys) {
      auto inserted = cache.emplace(k, first_id);
      if (inserted.second) {
        _logger->trace("Service resource with id {}:{} has resource_id {}",
                       k.first, k.second, first_id);
        first_id++;
      } else
        _logger->trace("Service resource with id {}:{} has resource_id {}",
                       k.first, k.second, inserted.first->second);
    }
  } catch (const std::exception& e) {
    _logger->error("Error while executing <<_add_service_resources>>: {}",
                   e.what());
  }
}

/**
 * @brief Add anomaly detections into the resources database. (code for
 * MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_anomalydetection_resources_mariadb(
    const ::google::protobuf::RepeatedPtrField<
        engine::configuration::Anomalydetection>& lst) {
  if (lst.empty()) {
    _logger->debug("No anomaly detection resources to add/update");
    return;
  }
  auto& cache = _stream->resources_cache();
  std::list<std::pair<uint64_t, uint64_t>> keys;
  mysql& mysql = _stream->get_mysql();
  if (!_add_anomalydetection_resources_stmt) {
    std::string query(
        "INSERT INTO resources "
        "(id,parent_id,internal_id,type,status,status_ordered,status_"
        "confirmed,"
        "max_check_attempts,poller_id,severity_id,name,alias,parent_name,"
        "notes_url,notes,action_url,notifications_enabled,"
        "passive_checks_enabled,active_checks_enabled,enabled) VALUES "
        "(?,?,?,?,4,1,1,?,?,?,?,?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE "
        "internal_id=VALUES(internal_id),type=VALUES(type),status=COALESCE("
        "status,4),status_ordered=COALESCE(status_ordered,1),"
        "status_confirmed=COALESCE(status_confirmed,1),"
        "max_check_attempts=VALUES(max_check_attempts),"
        "poller_id=VALUES(poller_id),severity_id=VALUES(severity_id),"
        "name=VALUES(name),alias=VALUES(alias),parent_name="
        "VALUES(parent_name),notes_url=VALUES(notes_url),notes=VALUES(notes),"
        "action_url=VALUES(action_url),notifications_enabled=VALUES("
        "notifications_enabled),passive_checks_enabled=VALUES(passive_checks_"
        "enabled),active_checks_enabled=VALUES(active_checks_enabled),"
        "enabled="
        "VALUES(enabled)");
    _add_anomalydetection_resources_stmt =
        std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_add_anomalydetection_resources_stmt);
  }
  auto bind = _add_anomalydetection_resources_stmt->create_bind();

  auto& global_cache = config::applier::state::instance().cache();
  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.host_id(), msg.service_id());
    keys.push_back(key);

    bind->set_value_as_u64(0, msg.service_id());
    bind->set_value_as_u64(1, msg.host_id());
    bind->set_null_u64(2);
    bind->set_value_as_u32(3, 4);
    bind->set_value_as_u32(4, msg.max_check_attempts());
    auto h = global_cache.host(msg.host_id());
    bind->set_value_as_u64(5, h->obj().instance_id());
    if (msg.has_severity_id()) {
      uint64_t db_sid =
          global_cache.get_db_id_for_severity(msg.severity_id(), 0);
      if (db_sid)
        bind->set_value_as_u64(6, db_sid);
      else
        bind->set_null_u64(6);
    } else
      bind->set_null_u64(6);
    bind->set_value_as_str(
        7, common::truncate_utf8(msg.service_description(),
                                 get_centreon_storage_resources_col_size(
                                     centreon_storage_resources_name)));
    bind->set_null_str(8);
    bind->set_value_as_str(
        9, common::truncate_utf8(msg.host_name(),
                                 get_centreon_storage_resources_col_size(
                                     centreon_storage_resources_parent_name)));
    bind->set_value_as_str(
        10, common::truncate_utf8(msg.notes_url(),
                                  get_centreon_storage_resources_col_size(
                                      centreon_storage_resources_notes_url)));
    bind->set_value_as_str(
        11, common::truncate_utf8(msg.notes(),
                                  get_centreon_storage_resources_col_size(
                                      centreon_storage_resources_notes)));
    bind->set_value_as_str(
        12, common::truncate_utf8(msg.action_url(),
                                  get_centreon_storage_resources_col_size(
                                      centreon_storage_resources_action_url)));
    bind->set_value_as_bool(13, msg.notifications_enabled());
    bind->set_value_as_bool(14, msg.checks_passive());
    bind->set_value_as_bool(15, msg.checks_active());
    bind->set_value_as_bool(16, true);
    bind->next_row();
    _add_customvariables_mariadb(msg.host_id(), msg.service_id(),
                                 msg.customvariables());
  }
  _add_anomalydetection_resources_stmt->set_bind(std::move(bind));

  try {
    std::promise<uint64_t> promise;
    std::future<uint64_t> future = promise.get_future();
    mysql.run_statement_and_get_int<uint64_t>(
        *_add_anomalydetection_resources_stmt, std::move(promise),
        mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& k : keys) {
      auto inserted = cache.emplace(k, first_id);
      if (inserted.second) {
        _logger->trace(
            "Anomaly detection resource with id {}:{} has resource_id {}",
            k.first, k.second, first_id);
        first_id++;
      } else
        _logger->trace(
            "Anomaly detection resource with id {}:{} has resource_id {}",
            k.first, k.second, inserted.first->second);
    }
  } catch (const std::exception& e) {
    _logger->error(
        "Error while executing <<_add_anomalydetection_resources>>: {}",
        e.what());
  }
}

/**
 * @brief Add anomaly detections into the resources database. (code for
 * MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_anomalydetection_resources_mysql(
    const ::google::protobuf::RepeatedPtrField<
        engine::configuration::Anomalydetection>& lst) {
  if (lst.empty())
    return;

  mysql& mysql = _stream->get_mysql();
  std::list<std::pair<uint64_t, uint64_t>> keys;
  auto& cache = _stream->resources_cache();

  auto& global_cache = config::applier::state::instance().cache();
  std::vector<std::string> values;
  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.host_id(), msg.service_id());
    keys.push_back(key);

    auto h = global_cache.host(msg.host_id());
    uint64_t ad_sid = 0;
    if (msg.has_severity_id())
      ad_sid = global_cache.get_db_id_for_severity(msg.severity_id(), 0);
    std::string value(fmt::format(
        "({},{},NULL,{},4,1,1,{},{},{},'{}',NULL,'{}','{}','{}','{}',{},{},{},"
        "1)",
        msg.service_id(), msg.host_id(), 4, msg.max_check_attempts(),
        h->obj().instance_id(), ad_sid ? fmt::to_string(ad_sid) : "NULL",
        misc::string::escape(msg.service_description(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_name)),
        misc::string::escape(msg.host_name(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_parent_name)),
        misc::string::escape(msg.notes_url(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_notes_url)),
        misc::string::escape(msg.notes(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_notes)),
        misc::string::escape(msg.action_url(),
                             get_centreon_storage_resources_col_size(
                                 centreon_storage_resources_action_url)),
        msg.notifications_enabled(), msg.checks_passive(),
        msg.checks_active()));
    values.emplace_back(value);
    _add_customvariables_mysql(msg.host_id(), msg.service_id(),
                               msg.customvariables());
  }
  std::string query(fmt::format(
      "INSERT INTO resources "
      "(id,parent_id,internal_id,type,status,status_ordered,status_"
      "confirmed,"
      "max_check_attempts,poller_id,severity_id,name,alias,parent_name,"
      "notes_url,notes,action_url,notifications_enabled,"
      "passive_checks_enabled,active_checks_enabled,enabled) VALUES {} ON "
      "DUPLICATE KEY UPDATE "
      "internal_id=VALUES(internal_id),type=VALUES(type),max_check_attempts="
      "VALUES(max_check_attempts),poller_id=VALUES(poller_id),severity_id="
      "VALUES(severity_id),name=VALUES(name),alias=VALUES(alias),parent_name="
      "VALUES(parent_name),notes_url=VALUES(notes_url),notes=VALUES(notes),"
      "action_url=VALUES(action_url),notifications_enabled=VALUES("
      "notifications_enabled),passive_checks_enabled=VALUES(passive_checks_"
      "enabled),active_checks_enabled=VALUES(active_checks_enabled),enabled="
      "VALUES(enabled)",
      fmt::join(values, ",")));

  try {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();
    mysql.run_query_and_get_int(query, std::move(promise),
                                mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& k : keys) {
      auto inserted = cache.emplace(k, first_id);
      if (inserted.second) {
        _logger->trace(
            "Anomaly detection resource with id {}:{} has resource_id {}",
            k.first, k.second, first_id);
        first_id++;
      } else
        _logger->trace(
            "Anomaly detection resource with id {}:{} has resource_id {}",
            k.first, k.second, inserted.first->second);
    }
  } catch (const std::exception& e) {
    _logger->error(
        "Error while executing <<_add_anomalydetection_resources>>: {}",
        e.what());
  }
}

/**
 * @brief Add custom variables into the resources database. (code for
 * MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_customvariables_mariadb(
    uint64_t host_id,
    uint64_t service_id,
    const ::google::protobuf::RepeatedPtrField<
        engine::configuration::CustomVariable>& lst) {
  mysql& mysql = _stream->get_mysql();
  if (!_add_customvariables_stmt) {
    std::string query(
        "INSERT INTO customvariables "
        "(host_id,service_id,name,default_value,value,type,modified) "
        "VALUES (?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE "
        "default_value=VALUES(default_value),value=VALUES(value),type=VALUES("
        "type),modified=VALUES(modified)");
    _add_customvariables_stmt = std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_add_customvariables_stmt);
  }
  auto bind = _add_customvariables_stmt->create_bind();

  for (const auto& msg : lst) {
    bind->set_value_as_i32(0, host_id);
    if (service_id)
      bind->set_value_as_i32(1, service_id);
    else
      bind->set_null_i32(1);
    bind->set_value_as_str(
        2, common::truncate_utf8(msg.name(),
                                 get_centreon_storage_customvariables_col_size(
                                     centreon_storage_customvariables_name)));
    bind->set_value_as_str(
        3,
        common::truncate_utf8(
            msg.value(), get_centreon_storage_customvariables_col_size(
                             centreon_storage_customvariables_default_value)));
    bind->set_value_as_str(
        4, common::truncate_utf8(msg.value(),
                                 get_centreon_storage_customvariables_col_size(
                                     centreon_storage_customvariables_value)));
    /* var_type is 0 for hosts, 1 otherwise */
    if (service_id)
      bind->set_value_as_u32(5, 1);
    else
      bind->set_value_as_u32(5, 0);

    bind->set_value_as_bool(6, false);
    bind->next_row();
  }
  _add_customvariables_stmt->set_bind(std::move(bind));
  mysql.run_statement(*_add_customvariables_stmt);
}

/**
 * @brief Add custom variables into the resources database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_customvariables_mysql(
    uint64_t host_id,
    uint64_t service_id,
    const ::google::protobuf::RepeatedPtrField<
        engine::configuration::CustomVariable>& lst) {
  if (lst.empty())
    return;

  mysql& mysql = _stream->get_mysql();

  std::vector<std::string> values;
  for (const auto& msg : lst) {
    std::string value(fmt::format(
        "({},{},'{}','{}','{}',{},0)", host_id, service_id,
        misc::string::escape(msg.name(),
                             get_centreon_storage_customvariables_col_size(
                                 centreon_storage_customvariables_name)),
        misc::string::escape(
            msg.value(), get_centreon_storage_customvariables_col_size(
                             centreon_storage_customvariables_default_value)),
        misc::string::escape(msg.value(),
                             get_centreon_storage_customvariables_col_size(
                                 centreon_storage_customvariables_value)),
        service_id ? 1 : 0));
    values.emplace_back(value);
  }
  std::string query(fmt::format(
      "INSERT INTO customvariables "
      "(host_id,service_id,name,default_value,value,type,modified) VALUES {} "
      "ON DUPLICATE KEY UPDATE "
      "default_value=VALUES(default_value),value=VALUES(value),type=VALUES("
      "type),modified=VALUES(modified)",
      fmt::join(values, ",")));
  mysql.run_query(query);
}

/**
 * @brief Add host groups into the database (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_hostgroups_mariadb(
    const ::google::protobuf::RepeatedPtrField<
        engine::configuration::Hostgroup>& lst,
    bool is_modification) {
  if (lst.empty()) {
    _logger->debug("No need to add/update host groups, list empty");
    return;
  }

  mysql& mysql = _stream->get_mysql();
  if (!_add_hostgroups_stmt) {
    std::string query(
        "INSERT INTO hostgroups (hostgroup_id,name) VALUES (?,?) ON DUPLICATE "
        "KEY UPDATE name=VALUES(name)");
    _add_hostgroups_stmt = std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_add_hostgroups_stmt);
  }
  auto* stmt = static_cast<mysql_bulk_stmt*>(_add_hostgroups_stmt.get());
  auto bind = stmt->create_bind();

  uint32_t count = 0;
  for (const auto& msg : lst) {
    _logger->debug("Processing hostgroup {} (id {})", msg.hostgroup_name(),
                   msg.hostgroup_id());
    bind->set_value_as_i32(0, msg.hostgroup_id());
    bind->set_value_as_str(
        1, common::truncate_utf8(msg.hostgroup_name(),
                                 get_centreon_storage_hostgroups_col_size(
                                     centreon_storage_hostgroups_name)));
    bind->next_row();
    count++;
  }
  _logger->debug("Adding/updating {} host groups", count);

  stmt->set_bind(std::move(bind));
  mysql.run_statement(*stmt);

  // For modified hostgroups, delete existing members scoped to the poller
  // before inserting the new complete member list to avoid duplicates and
  // stale entries.
  if (is_modification) {
    for (const auto& msg_hg : lst) {
      if (msg_hg.poller_id() == 0)
        continue;
      std::string del_query(fmt::format(
          "DELETE FROM hosts_hostgroups WHERE hostgroup_id = {} "
          "AND host_id IN (SELECT host_id FROM hosts WHERE instance_id = {})",
          msg_hg.hostgroup_id(), msg_hg.poller_id()));
      _logger->debug(
          "Removing existing members of hostgroup {} for poller {} before "
          "re-inserting",
          msg_hg.hostgroup_id(), msg_hg.poller_id());
      mysql.run_query(del_query);
    }
  }

  if (!_add_hostgroup_members_stmt) {
    std::string query(
        "INSERT INTO hosts_hostgroups (host_id, hostgroup_id) VALUES (?,?)");
    _add_hostgroup_members_stmt = std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_add_hostgroup_members_stmt);
  }

  stmt = static_cast<mysql_bulk_stmt*>(_add_hostgroup_members_stmt.get());
  auto bind_members = stmt->create_bind();
  auto& hosts_cache = _stream->host_name_id_cache();
  auto& global_cache = config::applier::state::instance().cache();
  for (const auto& msg_hg : lst) {
    if (msg_hg.members().data().empty())
      continue;

    for (const auto& member : msg_hg.members().data()) {
      auto found = hosts_cache.find(member);
      if (found == hosts_cache.end()) {
        _logger->error(
            "Host '{}' doesn't exist, so cannot add it to hostgroup '{}'",
            member, msg_hg.hostgroup_name());
        continue;
      }
      auto h = global_cache.host(found->second);
      bind_members->set_value_as_i32(0, found->second);
      bind_members->set_value_as_i32(1, msg_hg.hostgroup_id());
      _logger->info(
          "enabling membership of host {} to host group {} on instance {}",
          found->second, msg_hg.hostgroup_id(), h->obj().instance_id());
      bind_members->next_row();
    }
  }
  if (!bind_members->empty()) {
    stmt->set_bind(std::move(bind_members));
    mysql.run_statement(*stmt);
  }
}

/**
 * @brief Add host groups into the database (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_hostgroups_mysql(
    const ::google::protobuf::RepeatedPtrField<
        engine::configuration::Hostgroup>& lst,
    bool is_modification) {
  if (lst.empty()) {
    _logger->debug("No need to add/update host groups, list empty");
    return;
  }

  auto& mysql = _stream->get_mysql();
  std::vector<std::string> values;
  values.reserve(lst.size());

  uint32_t count = 0;
  for (const auto& msg : lst) {
    std::string value(fmt::format(
        "({},'{}')", msg.hostgroup_id(),
        misc::string::escape(msg.hostgroup_name(),
                             get_centreon_storage_hostgroups_col_size(
                                 centreon_storage_hostgroups_name))));
    count++;
    values.emplace_back(value);
  }
  std::string query(
      fmt::format("INSERT INTO hostgroups VALUES {} ON DUPLICATE KEY UPDATE "
                  "name=VALUES(name)",
                  fmt::join(values, ",")));
  _logger->debug("Adding/updating {} host groups", count);
  mysql.run_query(query);

  // For modified hostgroups, delete existing members scoped to the poller
  // before inserting the new complete member list to avoid duplicates and
  // stale entries.
  if (is_modification) {
    for (const auto& msg_hg : lst) {
      if (msg_hg.poller_id() == 0)
        continue;
      std::string del_query(fmt::format(
          "DELETE FROM hosts_hostgroups WHERE hostgroup_id = {} "
          "AND host_id IN (SELECT host_id FROM hosts WHERE instance_id = {})",
          msg_hg.hostgroup_id(), msg_hg.poller_id()));
      _logger->debug(
          "Removing existing members of hostgroup {} for poller {} before "
          "re-inserting",
          msg_hg.hostgroup_id(), msg_hg.poller_id());
      mysql.run_query(del_query);
    }
  }

  auto& hosts_cache = _stream->host_name_id_cache();
  values.clear();
  auto& global_cache = config::applier::state::instance().cache();
  for (const auto& msg_hg : lst) {
    if (msg_hg.members().data().empty())
      continue;

    for (const auto& member : msg_hg.members().data()) {
      auto found = hosts_cache.find(member);
      if (found == hosts_cache.end()) {
        _logger->error(
            "Host '{}' doesn't exist, so cannot add it to hostgroup '{}'",
            member, msg_hg.hostgroup_name());
        continue;
      }
      std::string value(
          fmt::format("({}, {})", found->second, msg_hg.hostgroup_id()));
      values.emplace_back(value);
      auto h = global_cache.host(found->second);
      _logger->info(
          "enabling membership of host {} to host group {} on instance {}",
          found->second, msg_hg.hostgroup_id(), h->obj().instance_id());
    }
  }
  if (!values.empty()) {
    query = fmt::format(
        "INSERT INTO hosts_hostgroups (host_id, hostgroup_id) VALUES {}",
        fmt::join(values, ","));
    mysql.run_query(query);
  }
}

/**
 * @brief Add service groups into the database (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_servicegroups_mariadb(
    const ::google::protobuf::RepeatedPtrField<
        engine::configuration::Servicegroup>& lst,
    bool is_modification) {
  if (lst.empty()) {
    _logger->debug("No need to add/update service groups, list empty");
    return;
  }

  mysql& mysql = _stream->get_mysql();
  if (!_add_servicegroups_stmt) {
    std::string query(
        "INSERT INTO servicegroups (servicegroup_id,name) VALUES (?,?) ON "
        "DUPLICATE KEY UPDATE name=VALUES(name)");
    _add_servicegroups_stmt = std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_add_servicegroups_stmt);
  }
  auto* stmt = static_cast<mysql_bulk_stmt*>(_add_servicegroups_stmt.get());
  auto bind = stmt->create_bind();

  uint32_t count = 0;
  for (const auto& msg : lst) {
    _logger->debug("Processing servicegroup {} (id {})",
                   msg.servicegroup_name(), msg.servicegroup_id());
    bind->set_value_as_i32(0, msg.servicegroup_id());
    bind->set_value_as_str(
        1, common::truncate_utf8(msg.servicegroup_name(),
                                 get_centreon_storage_servicegroups_col_size(
                                     centreon_storage_servicegroups_name)));
    bind->next_row();
    count++;
  }
  _logger->debug("Adding/updating {} service groups", count);

  stmt->set_bind(std::move(bind));
  mysql.run_statement(*stmt);

  // For modified servicegroups, delete existing members scoped to the poller
  // before inserting the new complete member list to avoid duplicates and
  // stale entries.
  if (is_modification) {
    for (const auto& msg_sg : lst) {
      if (msg_sg.poller_id() == 0)
        continue;
      std::string del_query(fmt::format(
          "DELETE FROM services_servicegroups WHERE servicegroup_id = {} "
          "AND host_id IN (SELECT host_id FROM hosts WHERE instance_id = {})",
          msg_sg.servicegroup_id(), msg_sg.poller_id()));
      _logger->debug(
          "Removing existing members of servicegroup {} for poller {} before "
          "re-inserting",
          msg_sg.servicegroup_id(), msg_sg.poller_id());
      mysql.run_query(del_query);
    }
  }

  if (!_add_servicegroup_members_stmt) {
    std::string query(
        "INSERT INTO services_servicegroups (host_id, service_id, "
        "servicegroup_id) VALUES (?,?,?)");
    _add_servicegroup_members_stmt = std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_add_servicegroup_members_stmt);
  }

  stmt = static_cast<mysql_bulk_stmt*>(_add_servicegroup_members_stmt.get());
  auto bind_members = stmt->create_bind();
  auto& hosts_cache = _stream->host_name_id_cache();
  auto& services_cache = _stream->service_description_id_cache();
  auto& global_cache = config::applier::state::instance().cache();
  for (const auto& msg_sg : lst) {
    if (msg_sg.members().data().empty())
      continue;

    for (const auto& member : msg_sg.members().data()) {
      auto fnd_host = hosts_cache.find(member.first());
      if (fnd_host == hosts_cache.end()) {
        _logger->error(
            "Host '{}' does not exist, so cannot add any of its services to "
            "servicegroup '{}'",
            member.first(), msg_sg.servicegroup_name());
        continue;
      }
      auto fnd_service = services_cache.find(
          std::make_pair(fnd_host->second, member.second()));
      if (fnd_service == services_cache.end()) {
        _logger->error(
            "Service '{}' on host '{}' does not exist, so cannot add it to "
            "servicegroup '{}'",
            member.second(), member.first(), msg_sg.servicegroup_name());
        continue;
      }
      bind_members->set_value_as_i32(0, fnd_host->second);
      bind_members->set_value_as_i32(1, fnd_service->second);
      bind_members->set_value_as_i32(2, msg_sg.servicegroup_id());
      auto h = global_cache.host(fnd_host->second);
      _logger->info(
          "enabling membership of service ({}:{}) to service group {} on "
          "instance {}",
          fnd_host->second, fnd_service->second, msg_sg.servicegroup_id(),
          h->obj().instance_id());
      bind_members->next_row();
    }
  }
  if (!bind_members->empty()) {
    stmt->set_bind(std::move(bind_members));
    mysql.run_statement(*stmt);
  }
}

/**
 * @brief Add service groups into the database (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_servicegroups_mysql(
    const ::google::protobuf::RepeatedPtrField<
        engine::configuration::Servicegroup>& lst,
    bool is_modification) {
  if (lst.empty()) {
    _logger->debug("No need to add/update service groups, list empty");
    return;
  }

  auto& mysql = _stream->get_mysql();
  std::vector<std::string> values;
  for (const auto& msg : lst) {
    std::string value(fmt::format(
        "({},'{}')", msg.servicegroup_id(),
        misc::string::escape(msg.servicegroup_name(),
                             get_centreon_storage_servicegroups_col_size(
                                 centreon_storage_servicegroups_name))));
    values.emplace_back(value);
  }
  std::string query(
      fmt::format("INSERT INTO servicegroups VALUES {} ON DUPLICATE KEY UPDATE "
                  "name=VALUES(name)",
                  fmt::join(values, ",")));
  mysql.run_query(query);

  // For modified servicegroups, delete existing members scoped to the poller
  // before inserting the new complete member list to avoid duplicates and
  // stale entries.
  if (is_modification) {
    for (const auto& msg_sg : lst) {
      if (msg_sg.poller_id() == 0)
        continue;
      std::string del_query(fmt::format(
          "DELETE FROM services_servicegroups WHERE servicegroup_id = {} "
          "AND host_id IN (SELECT host_id FROM hosts WHERE instance_id = {})",
          msg_sg.servicegroup_id(), msg_sg.poller_id()));
      _logger->debug(
          "Removing existing members of servicegroup {} for poller {} before "
          "re-inserting",
          msg_sg.servicegroup_id(), msg_sg.poller_id());
      mysql.run_query(del_query);
    }
  }

  auto& hosts_cache = _stream->host_name_id_cache();
  auto& services_cache = _stream->service_description_id_cache();
  values.clear();
  auto& global_cache = config::applier::state::instance().cache();
  for (const auto& msg_sg : lst) {
    if (msg_sg.members().data().empty())
      continue;

    for (const auto& member : msg_sg.members().data()) {
      auto fnd_host = hosts_cache.find(member.first());
      if (fnd_host == hosts_cache.end()) {
        _logger->error(
            "Host '{}' does not exist, so cannot add any of its services to "
            "servicegroup '{}'",
            member.first(), msg_sg.servicegroup_name());
        continue;
      }
      auto fnd_service = services_cache.find(
          std::make_pair(fnd_host->second, member.second()));
      if (fnd_service == services_cache.end()) {
        _logger->error(
            "Service '{}' on host '{}' does not exist, so cannot add it to "
            "servicegroup '{}'",
            member.second(), member.first(), msg_sg.servicegroup_name());
        continue;
      }
      std::string value(fmt::format("({}, {}, {})", fnd_host->second,
                                    fnd_service->second,
                                    msg_sg.servicegroup_id()));
      values.emplace_back(value);
      auto h = global_cache.host(fnd_host->second);
      _logger->info(
          "enabling membership of service ({}:{}) to service group {} on "
          "instance {}",
          fnd_host->second, fnd_service->second, msg_sg.servicegroup_id(),
          h->obj().instance_id());
    }
  }
  if (!values.empty()) {
    query = fmt::format(
        "INSERT INTO services_servicegroups (host_id, service_id, "
        "servicegroup_id) VALUES {}",
        fmt::join(values, ","));
    mysql.run_query(query);
  }
}

/**
 * @brief Add host parents into the database. A list of hosts is given, and
 * for each host, we have the list of its pants. (code for MariaDB).
 *
 * @param lst The list of Host messages to add/update.
 * @param act The action that triggered this addition (ADDED or MODIFIED).
 */
void database_configurator::_add_host_parents_mariadb(
    const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
        lst,
    const action& act) {
  if (lst.empty())
    return;

  _logger->debug("Adding parents to hosts");
  mysql& mysql = _stream->get_mysql();
  if (act == action::MODIFIED) {
    /* We remove all parents before re-adding them */
    std::list<uint64_t> host_ids;
    for (const auto& msg : lst)
      host_ids.push_back(msg.host_id());
    std::string query(
        fmt::format("DELETE FROM hosts_hosts_parents WHERE child_id IN ({})",
                    fmt::join(host_ids, ",")));
    mysql.run_query(query);
  }
  if (!_add_host_parents_stmt) {
    std::string query(
        "INSERT INTO hosts_hosts_parents (child_id,parent_id) VALUES "
        "(?,?) ON DUPLICATE KEY UPDATE child_id=VALUES(child_id),"
        "parent_id=VALUES(parent_id)");
    _add_host_parents_stmt = std::make_unique<mysql_bulk_stmt>(query);
    mysql.prepare_statement(*_add_host_parents_stmt);
  }
  auto* stmt = static_cast<mysql_bulk_stmt*>(_add_host_parents_stmt.get());
  auto bind = stmt->create_bind();
  uint32_t count = 0;
  for (const auto& msg : lst) {
    for (const std::string& h : msg.parents().data()) {
      auto found = _stream->host_name_id_cache().find(h);
      if (found == _stream->host_name_id_cache().end()) {
        _logger->error(
            "Host '{}' does not exist, so cannot add it as parent of host "
            "'{}'",
            h, msg.host_name());
        continue;
      }
      bind->set_value_as_i32(0, msg.host_id());
      bind->set_value_as_i32(1, found->second);
      _logger->debug("Adding host {} as parent of host {}", found->second,
                     msg.host_id());
      bind->next_row();
      count++;
    }
  }
  if (count > 0) {
    _logger->debug("{} host parents added", count);
    stmt->set_bind(std::move(bind));
    _stream->get_mysql().run_statement(*stmt);
  }
}

/**
 * @brief Add host parents into the database. A list of hosts is given, and
 * for each host, we have the list of its pants. (code for Mysql).
 *
 * @param lst The list of Host messages to add/update.
 * @param act The action that triggered this addition (ADDED or MODIFIED).
 */
void database_configurator::_add_host_parents_mysql(
    const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>&
        lst,
    const action& act) {
  if (lst.empty())
    return;

  _logger->debug("Adding parents to hosts");
  mysql& mysql = _stream->get_mysql();
  if (act == action::MODIFIED) {
    /* We remove all parents before re-adding them */
    std::list<uint64_t> host_ids;
    for (const auto& msg : lst)
      host_ids.push_back(msg.host_id());
    std::string query(
        fmt::format("DELETE FROM hosts_hosts_parents WHERE child_id IN ({})",
                    fmt::join(host_ids, ",")));
    mysql.run_query(query);
  }
  if (!_add_host_parents_stmt) {
    std::string query(
        "INSERT INTO hosts_hosts_parents (child_id,parent_id) VALUES "
        "(?,?) ON DUPLICATE KEY UPDATE child_id=VALUES(child_id),"
        "parent_id=VALUES(parent_id)");
    _add_host_parents_stmt = std::make_unique<mysql_stmt>(query);
    mysql.prepare_statement(*_add_host_parents_stmt);
  }

  uint32_t count = 0;
  for (const auto& msg : lst) {
    for (const std::string& h : msg.parents().data()) {
      auto found = _stream->host_name_id_cache().find(h);
      if (found == _stream->host_name_id_cache().end()) {
        _logger->error(
            "Host '{}' does not exist, so cannot add it as parent of host "
            "'{}'",
            h, msg.host_name());
        continue;
      }
      _add_host_parents_stmt->bind_value_as_i32(0, msg.host_id());
      _add_host_parents_stmt->bind_value_as_i32(1, found->second);
      _logger->debug("Adding host {} as parent of host {}", found->second,
                     msg.host_id());
      mysql.run_statement(*_add_host_parents_stmt);
      count++;
    }
  }
  if (count > 0)
    _logger->debug("{} host parents added", count);
}

/**
 * @brief Delete host parents or children from the database.
 *
 * @param lst The list of host IDs to delete parents/children from.
 */
void database_configurator::_del_host_parents(
    const ::google::protobuf::RepeatedField<uint64_t>& lst) {
  if (lst.empty())
    return;

  _logger->debug("Removing parents from {} hosts", lst.size());
  mysql& mysql = _stream->get_mysql();
  std::string query(
      fmt::format("DELETE FROM hosts_hosts_parents WHERE child_id IN ({0}) OR "
                  "parent_id IN ({0})",
                  fmt::join(lst, ",")));
  mysql.run_query(query);
}

/**
 * @brief Delete hostgroups from the database.
 *
 * @param keys The list of hostgroup/poller_id pairs to delete.
 */
void database_configurator::_del_hostgroups(
    const ::google::protobuf::RepeatedPtrField<
        com::centreon::engine::configuration::PairGroupPoller>& keys) {
  if (keys.empty())
    return;

  _logger->debug("Removing {} hostgroups", keys.size());
  mysql& mysql = _stream->get_mysql();

  for (const auto& msg : keys) {
    _logger->debug("Removing poller {} hosts from hostgroup {}",
                   msg.poller_id(), msg.group_name());
    std::string query(fmt::format(
        "DELETE FROM hosts_hostgroups "
        "WHERE hostgroup_id = (SELECT hostgroup_id FROM hostgroups WHERE name"
        " = \'{}\' LIMIT 1) AND host_id IN (SELECT host_id FROM hosts WHERE "
        "instance_id={})",
        misc::string::escape(msg.group_name(),
                             get_centreon_storage_hostgroups_col_size(
                                 centreon_storage_hostgroups_name)),
        msg.poller_id()));
    _logger->debug("Executing query: {}", query);
    mysql.run_query(query);
  }
  // Little cleanup in hostgroups
  std::string query(
      "DELETE FROM hostgroups WHERE NOT EXISTS (SELECT 1 FROM "
      "hosts_hostgroups "
      "WHERE hostgroups.hostgroup_id = hosts_hostgroups.hostgroup_id)");
  mysql.run_query(query);
}

/**
 * @brief Delete servicegroups from the database.
 *
 * @param keys The list of servicegroup/poller_id pairs to delete.
 */
void database_configurator::_del_servicegroups(
    const ::google::protobuf::RepeatedPtrField<
        com::centreon::engine::configuration::PairGroupPoller>& keys) {
  if (keys.empty())
    return;

  _logger->debug("Removing {} servicegroups", keys.size());
  mysql& mysql = _stream->get_mysql();

  for (const auto& msg : keys) {
    _logger->debug("Removing poller {} services from servicegroup {}",
                   msg.poller_id(), msg.group_name());
    std::string query(fmt::format(
        "DELETE FROM services_servicegroups WHERE servicegroup_id = (SELECT "
        "servicegroup_id FROM servicegroups WHERE name = \'{}\') AND host_id "
        "IN (SELECT host_id FROM hosts WHERE instance_id = {})",
        misc::string::escape(msg.group_name(),
                             get_centreon_storage_servicegroups_col_size(
                                 centreon_storage_servicegroups_name)),
        msg.poller_id()));
    mysql.run_query(query);
  }
  // Little cleanup in servicegroups
  std::string query(
      "DELETE FROM servicegroups WHERE NOT EXISTS (SELECT 1 FROM "
      "services_servicegroups WHERE servicegroups.servicegroup_id = "
      "services_servicegroups.servicegroup_id)");
  mysql.run_query(query);
}

}  // namespace com::centreon::broker::unified_sql

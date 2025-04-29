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
#include "com/centreon/broker/unified_sql/database_configurator.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/sql/table_max_size.hh"
#include "com/centreon/broker/sql/bulk_query.hh"
#include "com/centreon/common/utf8.hh"

using namespace com::centreon::broker::database;
using namespace com::centreon::broker::misc;

namespace com::centreon::broker::unified_sql {

void database_configurator::process() {
  /* We start by disabling pollers with full conf. */
  _disable_pollers_with_full_conf();

  /* Then we process the diff. */

  /* Disabling removed hosts */
  _disable_hosts();

  /* Adding new hosts */
  if (_stream->supports_bulk_prepared_statements())
    _add_hosts_mariadb();
  else
    _add_hosts_mysql();
}

/**
 * @brief Disable hosts, services in the hosts, services and resources tables
 * for the pollers whose configuration is fully received. This is needed because
 * we don't know which hosts and services have been removed.
 */
void database_configurator::_disable_pollers_with_full_conf() {
  for (uint64_t instance_id : _diff.full_conf_poller_id())
    _stream->clean_tables(instance_id);

  // Removed hosts are disabled in the hosts table.
  std::string query(
      fmt::format("UPDATE hosts SET enabled=0 WHERE host_id IN ({})",
                  fmt::join(_diff.hosts().removed(), ",")));
  _stream->get_mysql().run_query(query, database::mysql_error::disable_hosts,
                                 0);

  // Services of removed hosts are disabled in the services table.
  query = fmt::format("UPDATE services SET enabled=0 WHERE host_id IN ({})",
                      fmt::join(_diff.hosts().removed(), ","));
  _stream->get_mysql().run_query(query, database::mysql_error::disable_hosts,
                                 0);

  // Same thing with resources table.
  query = fmt::format(
      "UPDATE resources SET enabled=0 WHERE parent_id IN ({0}) OR (parent_id = "
      "0 AND id IN ({0}))",
      fmt::join(_diff.hosts().removed(), ","));
  _stream->get_mysql().run_query(query, database::mysql_error::disable_hosts,
                                 0);
}

/**
 * @brief Disable hosts, services in the hosts, services and resources tables
 * corresponding to the removed hosts in the diff state.
 */
void database_configurator::_disable_hosts() {
  // Removed hosts are disabled in the hosts table.
  std::string query(
      fmt::format("UPDATE hosts SET enabled=0 WHERE host_id IN ({})",
                  fmt::join(_diff.hosts().removed(), ",")));
  _stream->get_mysql().run_query(query, database::mysql_error::disable_hosts,
                                 0);

  // Services of removed hosts are disabled in the services table.
  query = fmt::format("UPDATE services SET enabled=0 WHERE host_id IN ({})",
                      fmt::join(_diff.hosts().removed(), ","));
  _stream->get_mysql().run_query(query, database::mysql_error::disable_hosts,
                                 0);

  // Same thing with resources table.
  query = fmt::format(
      "UPDATE resources SET enabled=0 WHERE parent_id IN ({0}) OR (parent_id = "
      "0 AND id IN ({0}))",
      fmt::join(_diff.hosts().removed(), ","));
  _stream->get_mysql().run_query(query, database::mysql_error::disable_hosts,
                                 0);
}

constexpr std::string_view add_hosts_query(
    "INSERT INTO hosts (host_id, name, instance_id, action_url, enabled, "
    "active_checks, address, alias, check_freshness) VALUES");
constexpr std::string_view add_hosts_query_suffix(
    "ON DUPLICATE KEY UPDATE "
    "name=VALUES(name), instance_id=VALUES(instance_id), action_url="
    "VALUES(action_url), active_checks=VALUES(active_checks), enabled="
    "VALUES(enabled), address=VALUES(address), alias=VALUES(alias), "
    "check_freshness=VALUES(check_freshness)");
void database_configurator::_add_hosts_mariadb() {
  mysql& mysql = _stream->get_mysql();
  std::string query(fmt::format("{} {} {}", add_hosts_query,
                                "(?, ?, ?, ?, ?, ?, ?, ?)",
                                add_hosts_query_suffix));
  mysql_bulk_stmt stmt(query);
  mysql.prepare_statement(stmt);
  auto bind = stmt.create_bind();

  for (const auto& host : _diff.hosts().added()) {
    bind->set_value_as_i32(0, host.host_id());
    bind->set_value_as_str(
        1, common::truncate_utf8(host.host_name(),
                                 get_centreon_storage_hosts_col_size(
                                     centreon_storage_hosts_name)));
    bind->set_value_as_i32(2, host.poller_id());
    bind->set_value_as_str(
        3, common::truncate_utf8(host.action_url(),
                                 get_centreon_storage_hosts_col_size(
                                     centreon_storage_hosts_action_url)));
    bind->set_value_as_bool(4, host.checks_active());
    bind->set_value_as_bool(5, 1);
    bind->set_value_as_str(
        6, common::truncate_utf8(host.address(),
                                 get_centreon_storage_hosts_col_size(
                                     centreon_storage_hosts_address)));
    bind->set_value_as_str(
        7,
        common::truncate_utf8(host.alias(), get_centreon_storage_hosts_col_size(
                                                centreon_storage_hosts_alias)));
    bind->set_value_as_bool(8, host.check_freshness());
    bind->next_row();
  }
  stmt.set_bind(std::move(bind));
  mysql.run_statement(stmt);
}

void database_configurator::_add_hosts_mysql() {
  mysql& mysql = _stream->get_mysql();

  std::vector<std::string> values;
  for (const auto& host : _diff.hosts().added()) {
    std::string value(
        fmt::format(
            "({},'{}',{},'{}',{},{},'{}','{}',{})", host.host_id(),
            misc::string::escape(host.host_name(),
                                 get_centreon_storage_hosts_col_size(
                                     centreon_storage_hosts_name)),
            host.poller_id(),
            misc::string::escape(host.action_url(),
                                 get_centreon_storage_hosts_col_size(
                                     centreon_storage_hosts_action_url)),
            host.checks_active(), 1,
            misc::string::escape(host.address(),
                                 get_centreon_storage_hosts_col_size(
                                     centreon_storage_hosts_address)),
            misc::string::escape(host.alias(),
                                 get_centreon_storage_hosts_col_size(
                                     centreon_storage_hosts_alias))),
        host.check_freshness());
    values.emplace_back(value);
  }
  std::string query(fmt::format("{} {} {}", add_hosts_query,
                                fmt::join(values, ","),
                                add_hosts_query_suffix));
  mysql.run_query(query);
}

}  // namespace com::centreon::broker::unified_sql

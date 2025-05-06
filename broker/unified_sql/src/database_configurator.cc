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
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/sql/table_max_size.hh"
#include "com/centreon/broker/unified_sql/database_configurator.hh"
#include "com/centreon/common/utf8.hh"
#include "common/engine_conf/state.pb.h"

using namespace com::centreon::broker::database;
using namespace com::centreon::broker::misc;

using com::centreon::engine::configuration::ActionHostOn;
using com::centreon::engine::configuration::ActionServiceOn;

namespace com::centreon::broker::unified_sql {

void database_configurator::process() {
  /* We start by disabling pollers with full conf. */
  _disable_pollers_with_full_conf();

  /* Then we process the diff. */

  /* Disabling removed hosts */
  _disable_hosts();

  /* Adding new hosts */
  if (_stream->supports_bulk_prepared_statements()) {
    auto cache_severities = _add_severities_mariadb(_diff.severities().added());
    auto cache_tags = _add_tags_mariadb(_diff.tags().added());
    _add_hosts_mariadb(_diff.hosts().added());
    auto cache_host_resources =
        _add_host_resources_mariadb(_diff.hosts().added());
  } else {
    auto cache_severities = _add_severities_mysql(_diff.severities().added());
    auto cache_tags = _add_tags_mysql(_diff.tags().added());
    _add_hosts_mysql(_diff.hosts().added());
    auto cache_host_resources =
        _add_host_resources_mysql(_diff.hosts().added());
  }
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

/** Database configuration
 * Query: INSERT ON DUPLICATE KEY UPDATE
 * Method: _add_severities
 * Return: absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t>
 * Key: {key::id, key::type}
 * Protobuf message: engine::configuration::Severity
 * Description: Add severities into the database.
 * Table: severities
 * Data:
 *  FIELD                 & TYPE   & COL NAME    & C_TYPE & OPTIONS
 *  ---------------------------------------------------------------
 *  ${0}                  & uint64 & severity_id & uint64 & AU
 *  key::id               & uint64 & id          & uint64 &
 *  key::type             & uint32 & type        & uint32 &
 *  severity_name         & string & name        & string &
 *  level                 & uint32 & level       & uint32 &
 *  icon_id               & uint64 & icon_id     & uint64 &
 *
 */
/**
 * @brief Add severities into the database. (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t> database_configurator::_add_severities_mariadb(const ::google::protobuf::RepeatedPtrField<engine::configuration::Severity>& lst) {
  absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t> retval;
  uint64_t offset = 0;
  std::string query("INSERT INTO severities (id,type,name,level,icon_id) VALUES (?,?,?,?,?) ON DUPLICATE KEY UPDATE id=VALUES(id),type=VALUES(type),name=VALUES(name),level=VALUES(level),icon_id=VALUES(icon_id)");
  mysql_bulk_stmt stmt(query);
  mysql& mysql = _stream->get_mysql();
  if (!stmt.prepared())
    mysql.prepare_statement(stmt);

  auto bind = stmt.create_bind();

  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.key().id(), msg.key().type());
    retval.emplace(key, offset);
    offset++;

    bind->set_value_as_u64(0, msg.key().id());
    bind->set_value_as_u32(1, msg.key().type());
    bind->set_value_as_str(2, common::truncate_utf8(msg.severity_name(), get_centreon_storage_severities_col_size(centreon_storage_severities_name)));
    bind->set_value_as_u32(3, msg.level());
    bind->set_value_as_u64(4, msg.icon_id());
    bind->next_row();
  }
  stmt.set_bind(std::move(bind));
  
  try {
    std::promise<uint64_t> promise;
    std::future<uint64_t> future = promise.get_future();
    mysql.run_statement_and_get_int<uint64_t>(stmt, std::move(promise), mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& [k, v] : retval)
      v += first_id;
  } catch (const std::exception& e) {
      _logger->error("Error while executing <<{{}}>>: {{}}", query, e.what());
  }
  return retval;
}



/**
 * @brief Add severities into the database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t> database_configurator::_add_severities_mysql(const ::google::protobuf::RepeatedPtrField<engine::configuration::Severity>& lst) {
  mysql& mysql = _stream->get_mysql();
  absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t> retval;
  uint32_t offset = 0;

  std::vector<std::string> values;
  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.key().id(), msg.key().type());
    retval.emplace(key, offset);
    offset++;

    std::string value(
        fmt::format("({},{},'{}',{},{})", msg.key().id(), msg.key().type(), misc::string::escape(msg.severity_name(), get_centreon_storage_severities_col_size(centreon_storage_severities_name)), msg.level(), msg.icon_id()));
    values.emplace_back(value);
  }
  std::string query(fmt::format("INSERT INTO severities VALUES {} ON DUPLICATE KEY UPDATE id=VALUES(id),type=VALUES(type),name=VALUES(name),level=VALUES(level),icon_id=VALUES(icon_id)", fmt::join(values, ",")));
  
  try {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();
    mysql.run_query_and_get_int(query, std::move(promise), mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& [k, v] : retval)
      v += first_id;
  } catch (const std::exception& e) {
    _logger->error("Error while executing <<{{}}>>: {{}}", query, e.what());
  }
  return retval;

}


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
/**
 * @brief Add tags into the database. (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t> database_configurator::_add_tags_mariadb(const ::google::protobuf::RepeatedPtrField<engine::configuration::Tag>& lst) {
  absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t> retval;
  uint64_t offset = 0;
  std::string query("INSERT INTO tags (id,type,name) VALUES (?,?,?) ON DUPLICATE KEY UPDATE id=VALUES(id),type=VALUES(type),name=VALUES(name)");
  mysql_bulk_stmt stmt(query);
  mysql& mysql = _stream->get_mysql();
  if (!stmt.prepared())
    mysql.prepare_statement(stmt);

  auto bind = stmt.create_bind();

  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.key().id(), msg.key().type());
    retval.emplace(key, offset);
    offset++;

    bind->set_value_as_u64(0, msg.key().id());
    bind->set_value_as_u32(1, msg.key().type());
    bind->set_value_as_str(2, common::truncate_utf8(msg.tag_name(), get_centreon_storage_tags_col_size(centreon_storage_tags_name)));
    bind->next_row();
  }
  stmt.set_bind(std::move(bind));
  
  try {
    std::promise<uint64_t> promise;
    std::future<uint64_t> future = promise.get_future();
    mysql.run_statement_and_get_int<uint64_t>(stmt, std::move(promise), mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& [k, v] : retval)
      v += first_id;
  } catch (const std::exception& e) {
      _logger->error("Error while executing <<{{}}>>: {{}}", query, e.what());
  }
  return retval;
}



/**
 * @brief Add tags into the database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t> database_configurator::_add_tags_mysql(const ::google::protobuf::RepeatedPtrField<engine::configuration::Tag>& lst) {
  mysql& mysql = _stream->get_mysql();
  absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t> retval;
  uint32_t offset = 0;

  std::vector<std::string> values;
  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.key().id(), msg.key().type());
    retval.emplace(key, offset);
    offset++;

    std::string value(
        fmt::format("({},{},'{}')", msg.key().id(), msg.key().type(), misc::string::escape(msg.tag_name(), get_centreon_storage_tags_col_size(centreon_storage_tags_name))));
    values.emplace_back(value);
  }
  std::string query(fmt::format("INSERT INTO tags VALUES {} ON DUPLICATE KEY UPDATE id=VALUES(id),type=VALUES(type),name=VALUES(name)", fmt::join(values, ",")));
  
  try {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();
    mysql.run_query_and_get_int(query, std::move(promise), mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& [k, v] : retval)
      v += first_id;
  } catch (const std::exception& e) {
    _logger->error("Error while executing <<{{}}>>: {{}}", query, e.what());
  }
  return retval;

}


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
/**
 * @brief Add hosts into the database. (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_hosts_mariadb(const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>& lst) {
  
  std::string query("INSERT INTO hosts (host_id,name,instance_id,action_url,active_checks,address,alias,check_command,check_freshness,check_interval,check_period,default_active_checks,default_event_handler_enabled,default_flap_detection,default_notify,default_passive_checks,default_process_perfdata,display_name,enabled,event_handler,event_handler_enabled,first_notification_delay,flap_detection,flap_detection_on_down,flap_detection_on_unreachable,flap_detection_on_up,freshness_threshold,high_flap_threshold,icon_image,icon_image_alt,low_flap_threshold,max_check_attempts,notes,notes_url,notification_interval,notify,notify_on_down,notify_on_downtime,notify_on_flapping,notify_on_recovery,notify_on_unreachable,obsess_over_host,passive_checks,process_perfdata,retain_nonstatus_information,retain_status_information,retry_interval,stalk_on_down,stalk_on_unreachable,stalk_on_up,statusmap_image,timezone) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE name=VALUES(name),instance_id=VALUES(instance_id),action_url=VALUES(action_url),active_checks=VALUES(active_checks),address=VALUES(address),alias=VALUES(alias),check_command=VALUES(check_command),check_freshness=VALUES(check_freshness),check_interval=VALUES(check_interval),check_period=VALUES(check_period),default_active_checks=VALUES(default_active_checks),default_event_handler_enabled=VALUES(default_event_handler_enabled),default_flap_detection=VALUES(default_flap_detection),default_notify=VALUES(default_notify),default_passive_checks=VALUES(default_passive_checks),default_process_perfdata=VALUES(default_process_perfdata),display_name=VALUES(display_name),enabled=VALUES(enabled),event_handler=VALUES(event_handler),event_handler_enabled=VALUES(event_handler_enabled),first_notification_delay=VALUES(first_notification_delay),flap_detection=VALUES(flap_detection),flap_detection_on_down=VALUES(flap_detection_on_down),flap_detection_on_unreachable=VALUES(flap_detection_on_unreachable),flap_detection_on_up=VALUES(flap_detection_on_up),freshness_threshold=VALUES(freshness_threshold),high_flap_threshold=VALUES(high_flap_threshold),icon_image=VALUES(icon_image),icon_image_alt=VALUES(icon_image_alt),low_flap_threshold=VALUES(low_flap_threshold),max_check_attempts=VALUES(max_check_attempts),notes=VALUES(notes),notes_url=VALUES(notes_url),notification_interval=VALUES(notification_interval),notify=VALUES(notify),notify_on_down=VALUES(notify_on_down),notify_on_downtime=VALUES(notify_on_downtime),notify_on_flapping=VALUES(notify_on_flapping),notify_on_recovery=VALUES(notify_on_recovery),notify_on_unreachable=VALUES(notify_on_unreachable),obsess_over_host=VALUES(obsess_over_host),passive_checks=VALUES(passive_checks),process_perfdata=VALUES(process_perfdata),retain_nonstatus_information=VALUES(retain_nonstatus_information),retain_status_information=VALUES(retain_status_information),retry_interval=VALUES(retry_interval),stalk_on_down=VALUES(stalk_on_down),stalk_on_unreachable=VALUES(stalk_on_unreachable),stalk_on_up=VALUES(stalk_on_up),statusmap_image=VALUES(statusmap_image),timezone=VALUES(timezone)");
  mysql_bulk_stmt stmt(query);
  mysql& mysql = _stream->get_mysql();
  if (!stmt.prepared())
    mysql.prepare_statement(stmt);

  auto bind = stmt.create_bind();

  for (const auto& msg : lst) {

    bind->set_value_as_i32(0, msg.host_id());
    bind->set_value_as_str(1, common::truncate_utf8(msg.host_name(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_name)));
    bind->set_value_as_i32(2, msg.poller_id());
    bind->set_value_as_str(3, common::truncate_utf8(msg.action_url(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_action_url)));
    bind->set_value_as_bool(4, msg.checks_active());
    bind->set_value_as_str(5, common::truncate_utf8(msg.address(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_address)));
    bind->set_value_as_str(6, common::truncate_utf8(msg.alias(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_alias)));
    bind->set_value_as_str(7, common::truncate_utf8(msg.check_command(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_check_command)));
    bind->set_value_as_bool(8, msg.check_freshness());
    bind->set_value_as_f64(9, msg.check_interval());
    bind->set_value_as_str(10, common::truncate_utf8(msg.check_period(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_check_period)));
    bind->set_value_as_bool(11, msg.checks_active());
    bind->set_value_as_bool(12, msg.event_handler_enabled());
    bind->set_value_as_bool(13, msg.flap_detection_enabled());
    bind->set_value_as_bool(14, msg.notifications_enabled());
    bind->set_value_as_bool(15, msg.checks_passive());
    bind->set_value_as_bool(16, msg.process_perf_data());
    bind->set_value_as_str(17, common::truncate_utf8(msg.display_name(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_display_name)));
    bind->set_value_as_bool(18, true);
    bind->set_value_as_str(19, common::truncate_utf8(msg.event_handler(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_event_handler)));
    bind->set_value_as_bool(20, msg.event_handler_enabled());
    bind->set_value_as_f64(21, msg.first_notification_delay());
    bind->set_value_as_bool(22, msg.flap_detection_enabled());
    bind->set_value_as_bool(23, msg.flap_detection_options() & ActionHostOn::action_hst_down);
    bind->set_value_as_bool(24, msg.flap_detection_options() & ActionHostOn::action_hst_unreachable);
    bind->set_value_as_bool(25, msg.flap_detection_options() & ActionHostOn::action_hst_up);
    bind->set_value_as_f64(26, msg.freshness_threshold());
    bind->set_value_as_f64(27, msg.high_flap_threshold());
    bind->set_value_as_str(28, common::truncate_utf8(msg.icon_image(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_icon_image)));
    bind->set_value_as_str(29, common::truncate_utf8(msg.icon_image_alt(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_icon_image_alt)));
    bind->set_value_as_f64(30, msg.low_flap_threshold());
    bind->set_value_as_i32(31, msg.max_check_attempts());
    bind->set_value_as_str(32, common::truncate_utf8(msg.notes(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_notes)));
    bind->set_value_as_str(33, common::truncate_utf8(msg.notes_url(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_notes_url)));
    bind->set_value_as_f64(34, msg.notification_interval());
    bind->set_value_as_bool(35, msg.notifications_enabled());
    bind->set_value_as_bool(36, msg.notification_options() & ActionHostOn::action_hst_down);
    bind->set_value_as_bool(37, msg.notification_options() & ActionHostOn::action_hst_downtime);
    bind->set_value_as_bool(38, msg.notification_options() & ActionHostOn::action_hst_flapping);
    bind->set_value_as_bool(39, msg.notification_options() & ActionHostOn::action_hst_up);
    bind->set_value_as_bool(40, msg.notification_options() & ActionHostOn::action_hst_unreachable);
    bind->set_value_as_bool(41, msg.obsess_over_host());
    bind->set_value_as_bool(42, msg.checks_passive());
    bind->set_value_as_bool(43, msg.process_perf_data());
    bind->set_value_as_bool(44, msg.retain_nonstatus_information());
    bind->set_value_as_bool(45, msg.retain_status_information());
    bind->set_value_as_f64(46, msg.retry_interval());
    bind->set_value_as_bool(47, msg.stalking_options() & ActionHostOn::action_hst_down);
    bind->set_value_as_bool(48, msg.stalking_options() & ActionHostOn::action_hst_unreachable);
    bind->set_value_as_bool(49, msg.stalking_options() & ActionHostOn::action_hst_up);
    bind->set_value_as_str(50, common::truncate_utf8(msg.statusmap_image(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_statusmap_image)));
    if (msg.has_timezone())
          bind->set_value_as_str(51, common::truncate_utf8(msg.timezone(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_timezone)));
    else
      bind->set_null_str(51);
    bind->next_row();
  }
  stmt.set_bind(std::move(bind));
  mysql.run_statement(stmt);
}



/**
 * @brief Add hosts into the database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_hosts_mysql(const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>& lst) {
  mysql& mysql = _stream->get_mysql();
  

  std::vector<std::string> values;
  for (const auto& msg : lst) {
    
    std::string value(
        fmt::format("({},'{}',{},'{}',{},'{}','{}','{}',{},{},'{}',{},{},{},{},{},{},'{}',1,'{}',{},{},{},{},{},{},{},{},'{}','{}',{},{},'{}','{}',{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},'{}','{}')", msg.host_id(), misc::string::escape(msg.host_name(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_name)), msg.poller_id(), misc::string::escape(msg.action_url(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_action_url)), msg.checks_active(), misc::string::escape(msg.address(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_address)), misc::string::escape(msg.alias(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_alias)), misc::string::escape(msg.check_command(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_check_command)), msg.check_freshness(), msg.check_interval(), misc::string::escape(msg.check_period(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_check_period)), msg.checks_active(), msg.event_handler_enabled(), msg.flap_detection_enabled(), msg.notifications_enabled(), msg.checks_passive(), msg.process_perf_data(), misc::string::escape(msg.display_name(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_display_name)), misc::string::escape(msg.event_handler(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_event_handler)), msg.event_handler_enabled(), msg.first_notification_delay(), msg.flap_detection_enabled(), msg.flap_detection_options() & ActionHostOn::action_hst_down ? 1 : 0, msg.flap_detection_options() & ActionHostOn::action_hst_unreachable ? 1 : 0, msg.flap_detection_options() & ActionHostOn::action_hst_up ? 1 : 0, msg.freshness_threshold(), msg.high_flap_threshold(), misc::string::escape(msg.icon_image(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_icon_image)), misc::string::escape(msg.icon_image_alt(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_icon_image_alt)), msg.low_flap_threshold(), msg.max_check_attempts(), misc::string::escape(msg.notes(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_notes)), misc::string::escape(msg.notes_url(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_notes_url)), msg.notification_interval(), msg.notifications_enabled(), msg.notification_options() & ActionHostOn::action_hst_down ? 1 : 0, msg.notification_options() & ActionHostOn::action_hst_downtime ? 1 : 0, msg.notification_options() & ActionHostOn::action_hst_flapping ? 1 : 0, msg.notification_options() & ActionHostOn::action_hst_up ? 1 : 0, msg.notification_options() & ActionHostOn::action_hst_unreachable ? 1 : 0, msg.obsess_over_host(), msg.checks_passive(), msg.process_perf_data(), msg.retain_nonstatus_information(), msg.retain_status_information(), msg.retry_interval(), msg.stalking_options() & ActionHostOn::action_hst_down ? 1 : 0, msg.stalking_options() & ActionHostOn::action_hst_unreachable ? 1 : 0, msg.stalking_options() & ActionHostOn::action_hst_up ? 1 : 0, misc::string::escape(msg.statusmap_image(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_statusmap_image)), msg.has_timezone() ? misc::string::escape(msg.timezone(), get_centreon_storage_hosts_col_size(centreon_storage_hosts_timezone)) : NULL));
    values.emplace_back(value);
  }
  std::string query(fmt::format("INSERT INTO hosts VALUES {} ON DUPLICATE KEY UPDATE name=VALUES(name),instance_id=VALUES(instance_id),action_url=VALUES(action_url),active_checks=VALUES(active_checks),address=VALUES(address),alias=VALUES(alias),check_command=VALUES(check_command),check_freshness=VALUES(check_freshness),check_interval=VALUES(check_interval),check_period=VALUES(check_period),default_active_checks=VALUES(default_active_checks),default_event_handler_enabled=VALUES(default_event_handler_enabled),default_flap_detection=VALUES(default_flap_detection),default_notify=VALUES(default_notify),default_passive_checks=VALUES(default_passive_checks),default_process_perfdata=VALUES(default_process_perfdata),display_name=VALUES(display_name),enabled=VALUES(enabled),event_handler=VALUES(event_handler),event_handler_enabled=VALUES(event_handler_enabled),first_notification_delay=VALUES(first_notification_delay),flap_detection=VALUES(flap_detection),flap_detection_on_down=VALUES(flap_detection_on_down),flap_detection_on_unreachable=VALUES(flap_detection_on_unreachable),flap_detection_on_up=VALUES(flap_detection_on_up),freshness_threshold=VALUES(freshness_threshold),high_flap_threshold=VALUES(high_flap_threshold),icon_image=VALUES(icon_image),icon_image_alt=VALUES(icon_image_alt),low_flap_threshold=VALUES(low_flap_threshold),max_check_attempts=VALUES(max_check_attempts),notes=VALUES(notes),notes_url=VALUES(notes_url),notification_interval=VALUES(notification_interval),notify=VALUES(notify),notify_on_down=VALUES(notify_on_down),notify_on_downtime=VALUES(notify_on_downtime),notify_on_flapping=VALUES(notify_on_flapping),notify_on_recovery=VALUES(notify_on_recovery),notify_on_unreachable=VALUES(notify_on_unreachable),obsess_over_host=VALUES(obsess_over_host),passive_checks=VALUES(passive_checks),process_perfdata=VALUES(process_perfdata),retain_nonstatus_information=VALUES(retain_nonstatus_information),retain_status_information=VALUES(retain_status_information),retry_interval=VALUES(retry_interval),stalk_on_down=VALUES(stalk_on_down),stalk_on_unreachable=VALUES(stalk_on_unreachable),stalk_on_up=VALUES(stalk_on_up),statusmap_image=VALUES(statusmap_image),timezone=VALUES(timezone)", fmt::join(values, ",")));
  mysql.run_query(query);
}


/** Database configuration
 * Query: INSERT ON DUPLICATE KEY UPDATE
 * Method: _add_host_resources
 * Return: absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t>
 * Key: {host_id, ${0}}
 * Protobuf message: engine::configuration::Host
 * Description: Add hosts into the resources database.
 * Table: resources
 * Data:
 *   FIELD                 & TYPE   & COL NAME               & C_TYPE & OPTIONS
 *   --------------------------------------------------------------------------
 *   ${0}                  & uint64 & resource_id            & uint64 & AU
 *   host_id               & uint64 & id                     & uint64 & U
 *   ${0}                  & uint64 & parent_id              & uint64 &
 *   ${NULL}               & uint64 & internal_id            & uint64 &
 *   ${1}                  & uint32 & type                   & uint32 &
 *   max_check_attempts    & uint32 & max_check_attempts     & uint32 &
 *   poller_id             & uint64 & poller_id              & uint64 &
 *   severity_id           & uint64 & severity_id            & uint64 & O
 *   host_name             & string & name                   & string &
 *   alias                 & string & alias                  & string &
 *   address               & string & address                & string &
 *   ${NULL}               & string & parent_name            & string & O
 *   icon_id               & uint64 & icon_id                & uint64 & O
 *   notes_url             & string & notes_url              & string &
 *   notes                 & string & notes                  & string &
 *   action_url            & string & action_url             & string &
 *   notifications_enabled & bool   & notifications_enabled  & bool   &
 *   checks_passive        & bool   & passive_checks_enabled & bool   &
 *   checks_active         & bool   & active_checks_enabled  & bool   &
 *   ${true}               & bool   & enabled                & bool   &
 */
/**
 * @brief Add hosts into the resources database. (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t> database_configurator::_add_host_resources_mariadb(const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>& lst) {
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t> retval;
  uint64_t offset = 0;
  std::string query("INSERT INTO resources (id,parent_id,internal_id,type,max_check_attempts,poller_id,severity_id,name,alias,address,parent_name,icon_id,notes_url,notes,action_url,notifications_enabled,passive_checks_enabled,active_checks_enabled,enabled) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE parent_id=VALUES(parent_id),internal_id=VALUES(internal_id),type=VALUES(type),max_check_attempts=VALUES(max_check_attempts),poller_id=VALUES(poller_id),severity_id=VALUES(severity_id),name=VALUES(name),alias=VALUES(alias),address=VALUES(address),parent_name=VALUES(parent_name),icon_id=VALUES(icon_id),notes_url=VALUES(notes_url),notes=VALUES(notes),action_url=VALUES(action_url),notifications_enabled=VALUES(notifications_enabled),passive_checks_enabled=VALUES(passive_checks_enabled),active_checks_enabled=VALUES(active_checks_enabled),enabled=VALUES(enabled)");
  mysql_bulk_stmt stmt(query);
  mysql& mysql = _stream->get_mysql();
  if (!stmt.prepared())
    mysql.prepare_statement(stmt);

  auto bind = stmt.create_bind();

  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.host_id(), 0);
    retval.emplace(key, offset);
    offset++;

    bind->set_value_as_u64(0, msg.host_id());
    bind->set_value_as_u64(1, 0);
    bind->set_null_u64(2);
    bind->set_value_as_u32(3, 1);
    bind->set_value_as_u32(4, msg.max_check_attempts());
    bind->set_value_as_u64(5, msg.poller_id());
    if (msg.has_severity_id())
      bind->set_value_as_u64(6, msg.severity_id());
    else
      bind->set_null_u64(6);
    bind->set_value_as_str(7, common::truncate_utf8(msg.host_name(), get_centreon_storage_resources_col_size(centreon_storage_resources_name)));
    bind->set_value_as_str(8, common::truncate_utf8(msg.alias(), get_centreon_storage_resources_col_size(centreon_storage_resources_alias)));
    bind->set_value_as_str(9, common::truncate_utf8(msg.address(), get_centreon_storage_resources_col_size(centreon_storage_resources_address)));
    bind->set_null_str(10);
    if (msg.has_icon_id())
      bind->set_value_as_u64(11, msg.icon_id());
    else
      bind->set_null_u64(11);
    bind->set_value_as_str(12, common::truncate_utf8(msg.notes_url(), get_centreon_storage_resources_col_size(centreon_storage_resources_notes_url)));
    bind->set_value_as_str(13, common::truncate_utf8(msg.notes(), get_centreon_storage_resources_col_size(centreon_storage_resources_notes)));
    bind->set_value_as_str(14, common::truncate_utf8(msg.action_url(), get_centreon_storage_resources_col_size(centreon_storage_resources_action_url)));
    bind->set_value_as_bool(15, msg.notifications_enabled());
    bind->set_value_as_bool(16, msg.checks_passive());
    bind->set_value_as_bool(17, msg.checks_active());
    bind->set_value_as_bool(18, true);
    bind->next_row();
  }
  stmt.set_bind(std::move(bind));
  
  try {
    std::promise<uint64_t> promise;
    std::future<uint64_t> future = promise.get_future();
    mysql.run_statement_and_get_int<uint64_t>(stmt, std::move(promise), mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& [k, v] : retval)
      v += first_id;
  } catch (const std::exception& e) {
      _logger->error("Error while executing <<{{}}>>: {{}}", query, e.what());
  }
  return retval;
}



/**
 * @brief Add hosts into the resources database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t> database_configurator::_add_host_resources_mysql(const ::google::protobuf::RepeatedPtrField<engine::configuration::Host>& lst) {
  mysql& mysql = _stream->get_mysql();
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t> retval;
  uint32_t offset = 0;

  std::vector<std::string> values;
  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.host_id(), 0);
    retval.emplace(key, offset);
    offset++;

    std::string value(
        fmt::format("({},{},NULL,{},{},{},{},'{}','{}','{}',NULL,{},'{}','{}','{}',{},{},{},1)", msg.host_id(), 0, 1, msg.max_check_attempts(), msg.poller_id(), msg.severity_id(), misc::string::escape(msg.host_name(), get_centreon_storage_resources_col_size(centreon_storage_resources_name)), misc::string::escape(msg.alias(), get_centreon_storage_resources_col_size(centreon_storage_resources_alias)), misc::string::escape(msg.address(), get_centreon_storage_resources_col_size(centreon_storage_resources_address)), msg.icon_id(), misc::string::escape(msg.notes_url(), get_centreon_storage_resources_col_size(centreon_storage_resources_notes_url)), misc::string::escape(msg.notes(), get_centreon_storage_resources_col_size(centreon_storage_resources_notes)), misc::string::escape(msg.action_url(), get_centreon_storage_resources_col_size(centreon_storage_resources_action_url)), msg.notifications_enabled(), msg.checks_passive(), msg.checks_active()));
    values.emplace_back(value);
  }
  std::string query(fmt::format("INSERT INTO resources VALUES {} ON DUPLICATE KEY UPDATE parent_id=VALUES(parent_id),internal_id=VALUES(internal_id),type=VALUES(type),max_check_attempts=VALUES(max_check_attempts),poller_id=VALUES(poller_id),severity_id=VALUES(severity_id),name=VALUES(name),alias=VALUES(alias),address=VALUES(address),parent_name=VALUES(parent_name),icon_id=VALUES(icon_id),notes_url=VALUES(notes_url),notes=VALUES(notes),action_url=VALUES(action_url),notifications_enabled=VALUES(notifications_enabled),passive_checks_enabled=VALUES(passive_checks_enabled),active_checks_enabled=VALUES(active_checks_enabled),enabled=VALUES(enabled)", fmt::join(values, ",")));
  
  try {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();
    mysql.run_query_and_get_int(query, std::move(promise), mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& [k, v] : retval)
      v += first_id;
  } catch (const std::exception& e) {
    _logger->error("Error while executing <<{{}}>>: {{}}", query, e.what());
  }
  return retval;

}


/** Database configuration
 * Query: INSERT ON DUPLICATE KEY UPDATE
 * Method: _add_services
 * Protobuf message: engine::configuration::Service
 * Description: Add services into the database.
 * Table: services
 * Data:
 *  FIELD                                                               & TYPE   & COL NAME                      & C_TYPE & OPTIONS
 *  ----------------------------------------------------------------------------------------------------------------------------------
 *  host_id                  & uint64_t & host_id                       & int32  & U
 *  service_description      & string   & description                   & string &
 *  service_id               & uint64   & service_id                    & int32  & U
 *  action_url               & string   & action_url                    & string &
 *  checks_active            & bool     & active_checks                 & bool   &
 *  check_command            & string   & check_command                 & string &
 *  check_freshness          & bool     & check_freshness               & bool   &
 *  check_interval           & uint32   & check_interval                & double &
 *  check_period             & string   & check_period                  & string &
 *  checks_active            & bool     & default_active_checks         & bool   &
 *  event_handler_enabled    & bool     & default_event_handler_enabled & bool   &
 *  flap_detection_enabled   & bool     & default_flap_detection        & bool   &
 *  notifications_enabled    & bool     & default_notify                & bool   &
 *  checks_passive           & bool     & default_passive_checks        & bool   &
 *  process_perf_data        & bool     & default_process_perfdata      & bool   &
 *  display_name             & string   & display_name                  & string &
 *  ${true}                  & bool     & enabled                       & bool   &
 *  event_handler            & string   & event_handler                 & string &
 *  event_handler_enabled    & bool     & event_handler_enabled         & bool   &
 *  first_notification_delay & uint32   & first_notification_delay      & double &
 *  flap_detection_enabled   & bool     & flap_detection                & bool   &
 *  ${msg.flap_detection_options() & ActionServiceOn::action_svc_critical} & bool & flap_detection_on_critical & bool &
 *  ${msg.flap_detection_options() & ActionServiceOn::action_svc_ok}       & bool & flap_detection_on_ok       & bool &
 *  ${msg.flap_detection_options() & ActionServiceOn::action_svc_unknown}  & bool & flap_detection_on_unknown  & bool &
 *  ${msg.flap_detection_options() & ActionServiceOn::action_svc_warning}  & bool & flap_detection_on_warning  & bool &
 *  freshness_threshold   & uint32 & freshness_threshold   & double &
 *  high_flap_threshold   & uint32 & high_flap_threshold   & double &
 *  icon_image            & string & icon_image            & string &
 *  icon_image_alt        & string & icon_image_alt        & string &
 *  low_flap_threshold    & uint32 & low_flap_threshold    & double &
 *  max_check_attempts    & uint32 & max_check_attempts    & int32  &
 *  notes                 & string & notes                 & string &
 *  notes_url             & string & notes_url             & string &
 *  notification_interval & uint32 & notification_interval & double &
 *  notification_period   & string & notification_period   & string & O
 *  notifications_enabled & bool   & notify                & bool   &
 *  ${msg.notification_options() & ActionServiceOn::action_svc_critical} & bool & notify_on_critical & bool &
 *  ${msg.notification_options() & ActionServiceOn::action_svc_downtime} & bool & notify_on_downtime & bool &
 *  ${msg.notification_options() & ActionServiceOn::action_svc_flapping} & bool & notify_on_flapping & bool &
 *  ${msg.notification_options() & ActionServiceOn::action_svc_ok} & bool & notify_on_recovery & bool &
 *  ${msg.notification_options() & ActionServiceOn::action_svc_unknown} & bool & notify_on_unknown & bool &
 *  ${msg.notification_options() & ActionServiceOn::action_svc_warning} & bool & notify_on_warning & bool &
 *  obsess_over_service          & bool   & obsess_over_service          & bool   &
 *  checks_passive               & bool   & passive_checks               & bool   &
 *  process_perf_data            & bool   & process_perfdata             & bool   &
 *  retain_nonstatus_information & bool   & retain_nonstatus_information & bool   &
 *  retain_status_information    & bool   & retain_status_information    & bool   &
 *  retry_interval               & uint32 & retry_interval               & double &
 *  ${msg.stalking_options() & ActionServiceOn::action_svc_critical} & bool & stalk_on_critical & bool &
 *  ${msg.stalking_options() & ActionServiceOn::action_svc_ok}       & bool & stalk_on_ok       & bool &
 *  ${msg.stalking_options() & ActionServiceOn::action_svc_unknown}  & bool & stalk_on_unknown  & bool &
 *  ${msg.stalking_options() & ActionServiceOn::action_svc_warning}  & bool & stalk_on_warning  & bool &
 */
/**
 * @brief Add services into the database. (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_services_mariadb(const ::google::protobuf::RepeatedPtrField<engine::configuration::Service>& lst) {
  
  std::string query("INSERT INTO services (host_id,description,service_id,action_url,active_checks,check_command,check_freshness,check_interval,check_period,default_active_checks,default_event_handler_enabled,default_flap_detection,default_notify,default_passive_checks,default_process_perfdata,display_name,enabled,event_handler,event_handler_enabled,first_notification_delay,flap_detection,flap_detection_on_critical,flap_detection_on_ok,flap_detection_on_unknown,flap_detection_on_warning,freshness_threshold,high_flap_threshold,icon_image,icon_image_alt,low_flap_threshold,max_check_attempts,notes,notes_url,notification_interval,notification_period,notify,notify_on_critical,notify_on_downtime,notify_on_flapping,notify_on_recovery,notify_on_unknown,notify_on_warning,obsess_over_service,passive_checks,process_perfdata,retain_nonstatus_information,retain_status_information,retry_interval,stalk_on_critical,stalk_on_ok,stalk_on_unknown,stalk_on_warning) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE description=VALUES(description),action_url=VALUES(action_url),active_checks=VALUES(active_checks),check_command=VALUES(check_command),check_freshness=VALUES(check_freshness),check_interval=VALUES(check_interval),check_period=VALUES(check_period),default_active_checks=VALUES(default_active_checks),default_event_handler_enabled=VALUES(default_event_handler_enabled),default_flap_detection=VALUES(default_flap_detection),default_notify=VALUES(default_notify),default_passive_checks=VALUES(default_passive_checks),default_process_perfdata=VALUES(default_process_perfdata),display_name=VALUES(display_name),enabled=VALUES(enabled),event_handler=VALUES(event_handler),event_handler_enabled=VALUES(event_handler_enabled),first_notification_delay=VALUES(first_notification_delay),flap_detection=VALUES(flap_detection),flap_detection_on_critical=VALUES(flap_detection_on_critical),flap_detection_on_ok=VALUES(flap_detection_on_ok),flap_detection_on_unknown=VALUES(flap_detection_on_unknown),flap_detection_on_warning=VALUES(flap_detection_on_warning),freshness_threshold=VALUES(freshness_threshold),high_flap_threshold=VALUES(high_flap_threshold),icon_image=VALUES(icon_image),icon_image_alt=VALUES(icon_image_alt),low_flap_threshold=VALUES(low_flap_threshold),max_check_attempts=VALUES(max_check_attempts),notes=VALUES(notes),notes_url=VALUES(notes_url),notification_interval=VALUES(notification_interval),notification_period=VALUES(notification_period),notify=VALUES(notify),notify_on_critical=VALUES(notify_on_critical),notify_on_downtime=VALUES(notify_on_downtime),notify_on_flapping=VALUES(notify_on_flapping),notify_on_recovery=VALUES(notify_on_recovery),notify_on_unknown=VALUES(notify_on_unknown),notify_on_warning=VALUES(notify_on_warning),obsess_over_service=VALUES(obsess_over_service),passive_checks=VALUES(passive_checks),process_perfdata=VALUES(process_perfdata),retain_nonstatus_information=VALUES(retain_nonstatus_information),retain_status_information=VALUES(retain_status_information),retry_interval=VALUES(retry_interval),stalk_on_critical=VALUES(stalk_on_critical),stalk_on_ok=VALUES(stalk_on_ok),stalk_on_unknown=VALUES(stalk_on_unknown),stalk_on_warning=VALUES(stalk_on_warning)");
  mysql_bulk_stmt stmt(query);
  mysql& mysql = _stream->get_mysql();
  if (!stmt.prepared())
    mysql.prepare_statement(stmt);

  auto bind = stmt.create_bind();

  for (const auto& msg : lst) {

    bind->set_value_as_i32(0, msg.host_id());
    bind->set_value_as_str(1, common::truncate_utf8(msg.service_description(), get_centreon_storage_services_col_size(centreon_storage_services_description)));
    bind->set_value_as_i32(2, msg.service_id());
    bind->set_value_as_str(3, common::truncate_utf8(msg.action_url(), get_centreon_storage_services_col_size(centreon_storage_services_action_url)));
    bind->set_value_as_bool(4, msg.checks_active());
    bind->set_value_as_str(5, common::truncate_utf8(msg.check_command(), get_centreon_storage_services_col_size(centreon_storage_services_check_command)));
    bind->set_value_as_bool(6, msg.check_freshness());
    bind->set_value_as_f64(7, msg.check_interval());
    bind->set_value_as_str(8, common::truncate_utf8(msg.check_period(), get_centreon_storage_services_col_size(centreon_storage_services_check_period)));
    bind->set_value_as_bool(9, msg.checks_active());
    bind->set_value_as_bool(10, msg.event_handler_enabled());
    bind->set_value_as_bool(11, msg.flap_detection_enabled());
    bind->set_value_as_bool(12, msg.notifications_enabled());
    bind->set_value_as_bool(13, msg.checks_passive());
    bind->set_value_as_bool(14, msg.process_perf_data());
    bind->set_value_as_str(15, common::truncate_utf8(msg.display_name(), get_centreon_storage_services_col_size(centreon_storage_services_display_name)));
    bind->set_value_as_bool(16, true);
    bind->set_value_as_str(17, common::truncate_utf8(msg.event_handler(), get_centreon_storage_services_col_size(centreon_storage_services_event_handler)));
    bind->set_value_as_bool(18, msg.event_handler_enabled());
    bind->set_value_as_f64(19, msg.first_notification_delay());
    bind->set_value_as_bool(20, msg.flap_detection_enabled());
    bind->set_value_as_bool(21, msg.flap_detection_options() & ActionServiceOn::action_svc_critical);
    bind->set_value_as_bool(22, msg.flap_detection_options() & ActionServiceOn::action_svc_ok);
    bind->set_value_as_bool(23, msg.flap_detection_options() & ActionServiceOn::action_svc_unknown);
    bind->set_value_as_bool(24, msg.flap_detection_options() & ActionServiceOn::action_svc_warning);
    bind->set_value_as_f64(25, msg.freshness_threshold());
    bind->set_value_as_f64(26, msg.high_flap_threshold());
    bind->set_value_as_str(27, common::truncate_utf8(msg.icon_image(), get_centreon_storage_services_col_size(centreon_storage_services_icon_image)));
    bind->set_value_as_str(28, common::truncate_utf8(msg.icon_image_alt(), get_centreon_storage_services_col_size(centreon_storage_services_icon_image_alt)));
    bind->set_value_as_f64(29, msg.low_flap_threshold());
    bind->set_value_as_i32(30, msg.max_check_attempts());
    bind->set_value_as_str(31, common::truncate_utf8(msg.notes(), get_centreon_storage_services_col_size(centreon_storage_services_notes)));
    bind->set_value_as_str(32, common::truncate_utf8(msg.notes_url(), get_centreon_storage_services_col_size(centreon_storage_services_notes_url)));
    bind->set_value_as_f64(33, msg.notification_interval());
    if (msg.has_notification_period())
          bind->set_value_as_str(34, common::truncate_utf8(msg.notification_period(), get_centreon_storage_services_col_size(centreon_storage_services_notification_period)));
    else
      bind->set_null_str(34);
    bind->set_value_as_bool(35, msg.notifications_enabled());
    bind->set_value_as_bool(36, msg.notification_options() & ActionServiceOn::action_svc_critical);
    bind->set_value_as_bool(37, msg.notification_options() & ActionServiceOn::action_svc_downtime);
    bind->set_value_as_bool(38, msg.notification_options() & ActionServiceOn::action_svc_flapping);
    bind->set_value_as_bool(39, msg.notification_options() & ActionServiceOn::action_svc_ok);
    bind->set_value_as_bool(40, msg.notification_options() & ActionServiceOn::action_svc_unknown);
    bind->set_value_as_bool(41, msg.notification_options() & ActionServiceOn::action_svc_warning);
    bind->set_value_as_bool(42, msg.obsess_over_service());
    bind->set_value_as_bool(43, msg.checks_passive());
    bind->set_value_as_bool(44, msg.process_perf_data());
    bind->set_value_as_bool(45, msg.retain_nonstatus_information());
    bind->set_value_as_bool(46, msg.retain_status_information());
    bind->set_value_as_f64(47, msg.retry_interval());
    bind->set_value_as_bool(48, msg.stalking_options() & ActionServiceOn::action_svc_critical);
    bind->set_value_as_bool(49, msg.stalking_options() & ActionServiceOn::action_svc_ok);
    bind->set_value_as_bool(50, msg.stalking_options() & ActionServiceOn::action_svc_unknown);
    bind->set_value_as_bool(51, msg.stalking_options() & ActionServiceOn::action_svc_warning);
    bind->next_row();
  }
  stmt.set_bind(std::move(bind));
  mysql.run_statement(stmt);
}



/**
 * @brief Add services into the database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_services_mysql(const ::google::protobuf::RepeatedPtrField<engine::configuration::Service>& lst) {
  mysql& mysql = _stream->get_mysql();
  

  std::vector<std::string> values;
  for (const auto& msg : lst) {
    
    std::string value(
        fmt::format("({},'{}',{},'{}',{},'{}',{},{},'{}',{},{},{},{},{},{},'{}',1,'{}',{},{},{},{},{},{},{},{},{},'{}','{}',{},{},'{}','{}',{},'{}',{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{})", msg.host_id(), misc::string::escape(msg.service_description(), get_centreon_storage_services_col_size(centreon_storage_services_description)), msg.service_id(), misc::string::escape(msg.action_url(), get_centreon_storage_services_col_size(centreon_storage_services_action_url)), msg.checks_active(), misc::string::escape(msg.check_command(), get_centreon_storage_services_col_size(centreon_storage_services_check_command)), msg.check_freshness(), msg.check_interval(), misc::string::escape(msg.check_period(), get_centreon_storage_services_col_size(centreon_storage_services_check_period)), msg.checks_active(), msg.event_handler_enabled(), msg.flap_detection_enabled(), msg.notifications_enabled(), msg.checks_passive(), msg.process_perf_data(), misc::string::escape(msg.display_name(), get_centreon_storage_services_col_size(centreon_storage_services_display_name)), misc::string::escape(msg.event_handler(), get_centreon_storage_services_col_size(centreon_storage_services_event_handler)), msg.event_handler_enabled(), msg.first_notification_delay(), msg.flap_detection_enabled(), msg.flap_detection_options() & ActionServiceOn::action_svc_critical ? 1 : 0, msg.flap_detection_options() & ActionServiceOn::action_svc_ok ? 1 : 0, msg.flap_detection_options() & ActionServiceOn::action_svc_unknown ? 1 : 0, msg.flap_detection_options() & ActionServiceOn::action_svc_warning ? 1 : 0, msg.freshness_threshold(), msg.high_flap_threshold(), misc::string::escape(msg.icon_image(), get_centreon_storage_services_col_size(centreon_storage_services_icon_image)), misc::string::escape(msg.icon_image_alt(), get_centreon_storage_services_col_size(centreon_storage_services_icon_image_alt)), msg.low_flap_threshold(), msg.max_check_attempts(), misc::string::escape(msg.notes(), get_centreon_storage_services_col_size(centreon_storage_services_notes)), misc::string::escape(msg.notes_url(), get_centreon_storage_services_col_size(centreon_storage_services_notes_url)), msg.notification_interval(), msg.has_notification_period() ? misc::string::escape(msg.notification_period(), get_centreon_storage_services_col_size(centreon_storage_services_notification_period)) : NULL, msg.notifications_enabled(), msg.notification_options() & ActionServiceOn::action_svc_critical ? 1 : 0, msg.notification_options() & ActionServiceOn::action_svc_downtime ? 1 : 0, msg.notification_options() & ActionServiceOn::action_svc_flapping ? 1 : 0, msg.notification_options() & ActionServiceOn::action_svc_ok ? 1 : 0, msg.notification_options() & ActionServiceOn::action_svc_unknown ? 1 : 0, msg.notification_options() & ActionServiceOn::action_svc_warning ? 1 : 0, msg.obsess_over_service(), msg.checks_passive(), msg.process_perf_data(), msg.retain_nonstatus_information(), msg.retain_status_information(), msg.retry_interval(), msg.stalking_options() & ActionServiceOn::action_svc_critical ? 1 : 0, msg.stalking_options() & ActionServiceOn::action_svc_ok ? 1 : 0, msg.stalking_options() & ActionServiceOn::action_svc_unknown ? 1 : 0, msg.stalking_options() & ActionServiceOn::action_svc_warning ? 1 : 0));
    values.emplace_back(value);
  }
  std::string query(fmt::format("INSERT INTO services VALUES {} ON DUPLICATE KEY UPDATE description=VALUES(description),action_url=VALUES(action_url),active_checks=VALUES(active_checks),check_command=VALUES(check_command),check_freshness=VALUES(check_freshness),check_interval=VALUES(check_interval),check_period=VALUES(check_period),default_active_checks=VALUES(default_active_checks),default_event_handler_enabled=VALUES(default_event_handler_enabled),default_flap_detection=VALUES(default_flap_detection),default_notify=VALUES(default_notify),default_passive_checks=VALUES(default_passive_checks),default_process_perfdata=VALUES(default_process_perfdata),display_name=VALUES(display_name),enabled=VALUES(enabled),event_handler=VALUES(event_handler),event_handler_enabled=VALUES(event_handler_enabled),first_notification_delay=VALUES(first_notification_delay),flap_detection=VALUES(flap_detection),flap_detection_on_critical=VALUES(flap_detection_on_critical),flap_detection_on_ok=VALUES(flap_detection_on_ok),flap_detection_on_unknown=VALUES(flap_detection_on_unknown),flap_detection_on_warning=VALUES(flap_detection_on_warning),freshness_threshold=VALUES(freshness_threshold),high_flap_threshold=VALUES(high_flap_threshold),icon_image=VALUES(icon_image),icon_image_alt=VALUES(icon_image_alt),low_flap_threshold=VALUES(low_flap_threshold),max_check_attempts=VALUES(max_check_attempts),notes=VALUES(notes),notes_url=VALUES(notes_url),notification_interval=VALUES(notification_interval),notification_period=VALUES(notification_period),notify=VALUES(notify),notify_on_critical=VALUES(notify_on_critical),notify_on_downtime=VALUES(notify_on_downtime),notify_on_flapping=VALUES(notify_on_flapping),notify_on_recovery=VALUES(notify_on_recovery),notify_on_unknown=VALUES(notify_on_unknown),notify_on_warning=VALUES(notify_on_warning),obsess_over_service=VALUES(obsess_over_service),passive_checks=VALUES(passive_checks),process_perfdata=VALUES(process_perfdata),retain_nonstatus_information=VALUES(retain_nonstatus_information),retain_status_information=VALUES(retain_status_information),retry_interval=VALUES(retry_interval),stalk_on_critical=VALUES(stalk_on_critical),stalk_on_ok=VALUES(stalk_on_ok),stalk_on_unknown=VALUES(stalk_on_unknown),stalk_on_warning=VALUES(stalk_on_warning)", fmt::join(values, ",")));
  mysql.run_query(query);
}


/** Database configuration
 * Query: INSERT ON DUPLICATE KEY UPDATE
 * Method: _add_service_resources
 * Return: absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t>
 * Key: {host_id, service_id}
 * Protobuf message: engine::configuration::Service
 * Description: Add services into the resources database.
 * Table: resources
 * Data:
 *  FIELD                 & TYPE   & COL NAME               & C_TYPE & OPTIONS
 *  --------------------------------------------------------------------------
 * ${0} & uint64 & resource_id & uint64_t & AU
 * service_id & uint64 & id & uint64 & U
 * host_id & uint64 & parent_id & uint64 & U
 * ${NULL} & uint64 & internal_id & uint64 &
 * ${0 // Maybe others values if meta-service, ba, ... } & uint32 & type & uint32 &
 * max_check_attempts & uint32 & max_check_attempts & uint32 &
 * ${poller_id} & uint64 & poller_id & uint64 &
 * severity_id & uint64 & severity_id & uint64 & O
 * service_description & string & name & string &
 * ${NULL} & string & alias & string &
 * host_name & string & parent_name & string &
 * notes_url & string & notes_url & string &
 * notes & string & notes & string &
 * action_url & string & action_url & string &
 * notifications_enabled & bool & notifications_enabled & bool &
 * checks_passive & bool & passive_checks_enabled & bool &
 * checks_active & bool & active_checks_enabled & bool &
 * ${true} & bool & enabled & bool &
 */
/**
 * @brief Add services into the resources database. (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t> database_configurator::_add_service_resources_mariadb(const ::google::protobuf::RepeatedPtrField<engine::configuration::Service>& lst) {
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t> retval;
  uint64_t offset = 0;
  std::string query("INSERT INTO resources (id,parent_id,internal_id,type,max_check_attempts,poller_id,severity_id,name,alias,parent_name,notes_url,notes,action_url,notifications_enabled,passive_checks_enabled,active_checks_enabled,enabled) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE internal_id=VALUES(internal_id),type=VALUES(type),max_check_attempts=VALUES(max_check_attempts),poller_id=VALUES(poller_id),severity_id=VALUES(severity_id),name=VALUES(name),alias=VALUES(alias),parent_name=VALUES(parent_name),notes_url=VALUES(notes_url),notes=VALUES(notes),action_url=VALUES(action_url),notifications_enabled=VALUES(notifications_enabled),passive_checks_enabled=VALUES(passive_checks_enabled),active_checks_enabled=VALUES(active_checks_enabled),enabled=VALUES(enabled)");
  mysql_bulk_stmt stmt(query);
  mysql& mysql = _stream->get_mysql();
  if (!stmt.prepared())
    mysql.prepare_statement(stmt);

  auto bind = stmt.create_bind();

  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.host_id(), msg.service_id());
    retval.emplace(key, offset);
    offset++;

    bind->set_value_as_u64(0, msg.service_id());
    bind->set_value_as_u64(1, msg.host_id());
    bind->set_null_u64(2);
    bind->set_value_as_u32(3, 0 /* Maybe others values if meta-service, ba, ... */);
    bind->set_value_as_u32(4, msg.max_check_attempts());
    bind->set_value_as_u64(5, poller_id);
    if (msg.has_severity_id())
      bind->set_value_as_u64(6, msg.severity_id());
    else
      bind->set_null_u64(6);
    bind->set_value_as_str(7, common::truncate_utf8(msg.service_description(), get_centreon_storage_resources_col_size(centreon_storage_resources_name)));
    bind->set_null_str(8);
    bind->set_value_as_str(9, common::truncate_utf8(msg.host_name(), get_centreon_storage_resources_col_size(centreon_storage_resources_parent_name)));
    bind->set_value_as_str(10, common::truncate_utf8(msg.notes_url(), get_centreon_storage_resources_col_size(centreon_storage_resources_notes_url)));
    bind->set_value_as_str(11, common::truncate_utf8(msg.notes(), get_centreon_storage_resources_col_size(centreon_storage_resources_notes)));
    bind->set_value_as_str(12, common::truncate_utf8(msg.action_url(), get_centreon_storage_resources_col_size(centreon_storage_resources_action_url)));
    bind->set_value_as_bool(13, msg.notifications_enabled());
    bind->set_value_as_bool(14, msg.checks_passive());
    bind->set_value_as_bool(15, msg.checks_active());
    bind->set_value_as_bool(16, true);
    bind->next_row();
  }
  stmt.set_bind(std::move(bind));
  
  try {
    std::promise<uint64_t> promise;
    std::future<uint64_t> future = promise.get_future();
    mysql.run_statement_and_get_int<uint64_t>(stmt, std::move(promise), mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& [k, v] : retval)
      v += first_id;
  } catch (const std::exception& e) {
      _logger->error("Error while executing <<{{}}>>: {{}}", query, e.what());
  }
  return retval;
}



/**
 * @brief Add services into the resources database. (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t> database_configurator::_add_service_resources_mysql(const ::google::protobuf::RepeatedPtrField<engine::configuration::Service>& lst) {
  mysql& mysql = _stream->get_mysql();
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t> retval;
  uint32_t offset = 0;

  std::vector<std::string> values;
  for (const auto& msg : lst) {
    auto key = std::make_pair(msg.host_id(), msg.service_id());
    retval.emplace(key, offset);
    offset++;

    std::string value(
        fmt::format("({},{},NULL,{},{},{},{},'{}',NULL,'{}','{}','{}','{}',{},{},{},1)", msg.service_id(), msg.host_id(), 0 /* Maybe others values if meta-service, ba, ... */, msg.max_check_attempts(), poller_id, msg.severity_id(), misc::string::escape(msg.service_description(), get_centreon_storage_resources_col_size(centreon_storage_resources_name)), misc::string::escape(msg.host_name(), get_centreon_storage_resources_col_size(centreon_storage_resources_parent_name)), misc::string::escape(msg.notes_url(), get_centreon_storage_resources_col_size(centreon_storage_resources_notes_url)), misc::string::escape(msg.notes(), get_centreon_storage_resources_col_size(centreon_storage_resources_notes)), misc::string::escape(msg.action_url(), get_centreon_storage_resources_col_size(centreon_storage_resources_action_url)), msg.notifications_enabled(), msg.checks_passive(), msg.checks_active()));
    values.emplace_back(value);
  }
  std::string query(fmt::format("INSERT INTO resources VALUES {} ON DUPLICATE KEY UPDATE internal_id=VALUES(internal_id),type=VALUES(type),max_check_attempts=VALUES(max_check_attempts),poller_id=VALUES(poller_id),severity_id=VALUES(severity_id),name=VALUES(name),alias=VALUES(alias),parent_name=VALUES(parent_name),notes_url=VALUES(notes_url),notes=VALUES(notes),action_url=VALUES(action_url),notifications_enabled=VALUES(notifications_enabled),passive_checks_enabled=VALUES(passive_checks_enabled),active_checks_enabled=VALUES(active_checks_enabled),enabled=VALUES(enabled)", fmt::join(values, ",")));
  
  try {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();
    mysql.run_query_and_get_int(query, std::move(promise), mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& [k, v] : retval)
      v += first_id;
  } catch (const std::exception& e) {
    _logger->error("Error while executing <<{{}}>>: {{}}", query, e.what());
  }
  return retval;

}


}  // namespace com::centreon::broker::unified_sql

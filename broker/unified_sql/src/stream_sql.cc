/*
** Copyright 2021-2023 Centreon
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
#include <fmt/format.h>

#include "bbdo/storage/index_mapping.hh"
#include "com/centreon/broker/database/mysql_result.hh"
#include "com/centreon/broker/database/table_max_size.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/broker/query_preparator.hh"
#include "com/centreon/broker/unified_sql/stream.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::database;
using namespace com::centreon::broker::unified_sql;

static inline bool is_not_zero(const int64_t& value) {
  return value != 0;
}

/**
 *  @brief Clean tables with data associated to the instance.
 *
 *  Rather than delete appropriate entries in tables, they are instead
 *  deactivated using a specific flag.
 *
 *  @param[in] instance_id Instance ID to remove.
 */
void stream::_clean_tables(uint32_t instance_id) {
  // no hostgroup and servicegroup clean during this function
  {
    std::lock_guard<std::mutex> l(_group_clean_timer_m);
    _group_clean_timer.cancel();
  }

  /* Database version. */

  int32_t conn;

  _finish_action(-1, -1);
  if (_store_in_resources) {
    log_v2::sql()->debug(
        "unified sql: remove tags memberships (instance_id: {})", instance_id);
    conn = special_conn::tag % _mysql.connections_count();
    _mysql.run_query(
        fmt::format("DELETE rt FROM resources_tags rt LEFT JOIN resources r ON "
                    "rt.resource_id=r.resource_id WHERE r.poller_id={}",
                    instance_id),
        database::mysql_error::clean_resources_tags, false, conn);
    _mysql.commit(conn);
  }

  conn = _mysql.choose_connection_by_instance(instance_id);
  _mysql.run_query(
      fmt::format("UPDATE resources SET enabled=0 WHERE poller_id={}",
                  instance_id),
      database::mysql_error::clean_resources, false, conn);
  _add_action(conn, actions::resources);
  log_v2::sql()->debug(
      "unified sql: disable hosts and services (instance_id: {})", instance_id);
  /* Disable hosts and services. */
  std::string query(fmt::format(
      "UPDATE hosts AS h LEFT JOIN services AS s ON h.host_id = s.host_id "
      "SET h.enabled=0, s.enabled=0 WHERE h.instance_id={}",
      instance_id));
  _mysql.run_query(query, database::mysql_error::clean_hosts_services, false,
                   conn);
  _add_action(conn, actions::hosts);

  /* Remove host group memberships. */
  log_v2::sql()->debug(
      "unified sql: remove host group memberships (instance_id: {})",
      instance_id);
  query = fmt::format(
      "DELETE hosts_hostgroups FROM hosts_hostgroups LEFT JOIN hosts ON "
      "hosts_hostgroups.host_id=hosts.host_id WHERE hosts.instance_id={}",
      instance_id);
  _mysql.run_query(query, database::mysql_error::clean_hostgroup_members, false,
                   conn);
  _add_action(conn, actions::hostgroups);

  /* Remove service group memberships */
  log_v2::sql()->debug(
      "unified sql: remove service group memberships (instance_id: {})",
      instance_id);
  query = fmt::format(
      "DELETE services_servicegroups FROM services_servicegroups LEFT JOIN "
      "hosts ON services_servicegroups.host_id=hosts.host_id WHERE "
      "hosts.instance_id={}",
      instance_id);
  _mysql.run_query(query, database::mysql_error::clean_servicegroup_members,
                   false, conn);
  _add_action(conn, actions::servicegroups);

  /* Remove host dependencies. */
  log_v2::sql()->debug(
      "unified sql: remove host dependencies (instance_id: {})", instance_id);
  query = fmt::format(
      "DELETE hhd FROM hosts_hosts_dependencies AS hhd INNER JOIN hosts as "
      "h ON hhd.host_id=h.host_id OR hhd.dependent_host_id=h.host_id WHERE "
      "h.instance_id={}",
      instance_id);
  _mysql.run_query(query, database::mysql_error::clean_host_dependencies, false,
                   conn);
  _add_action(conn, actions::host_dependencies);

  /* Remove host parents. */
  log_v2::sql()->debug("unified sql: remove host parents (instance_id: {})",
                       instance_id);
  query = fmt::format(
      "DELETE hhp FROM hosts_hosts_parents AS hhp INNER JOIN hosts as h ON "
      "hhp.child_id=h.host_id OR hhp.parent_id=h.host_id WHERE "
      "h.instance_id={}",
      instance_id);
  _mysql.run_query(query, database::mysql_error::clean_host_parents, false,
                   conn);
  _add_action(conn, actions::host_parents);

  /* Remove service dependencies. */
  log_v2::sql()->debug(
      "unified sql: remove service dependencies (instance_id: {})",
      instance_id);
  query = fmt::format(
      "DELETE ssd FROM services_services_dependencies AS ssd"
      " INNER JOIN services as s"
      " ON ssd.service_id=s.service_id OR "
      "ssd.dependent_service_id=s.service_id"
      " INNER JOIN hosts as h"
      " ON s.host_id=h.host_id"
      " WHERE h.instance_id={}",
      instance_id);
  _mysql.run_query(query, database::mysql_error::clean_service_dependencies,
                   false, conn);
  _add_action(conn, actions::service_dependencies);

  /* Remove list of modules. */
  log_v2::sql()->debug("SQL: remove list of modules (instance_id: {})",
                       instance_id);
  query = fmt::format("DELETE FROM modules WHERE instance_id={}", instance_id);
  _mysql.run_query(query, database::mysql_error::clean_modules, false, conn);
  _add_action(conn, actions::modules);

  // Cancellation of downtimes.
  log_v2::sql()->debug("SQL: Cancellation of downtimes (instance_id: {})",
                       instance_id);
  query = fmt::format(
      "UPDATE downtimes SET cancelled=1 WHERE actual_end_time IS NULL AND "
      "cancelled=0 "
      "AND instance_id={}",
      instance_id);

  _mysql.run_query(query, database::mysql_error::clean_downtimes, false, conn);
  _add_action(conn, actions::downtimes);

  // Remove comments.
  log_v2::sql()->debug("unified sql: remove comments (instance_id: {})",
                       instance_id);

  query = fmt::format(
      "UPDATE comments SET deletion_time={} WHERE instance_id={} AND "
      "persistent=0 AND "
      "(deletion_time IS NULL OR deletion_time=0)",
      time(nullptr), instance_id);

  _mysql.run_query(query, database::mysql_error::clean_comments, false, conn);
  _add_action(conn, actions::comments);

  // Remove custom variables. No need to choose the good instance, there are
  // no constraint between custom variables and instances.
  log_v2::sql()->debug("Removing custom variables (instance_id: {})",
                       instance_id);
  query = fmt::format(
      "DELETE cv FROM customvariables AS cv INNER JOIN hosts AS h ON "
      "cv.host_id = h.host_id WHERE h.instance_id={}",
      instance_id);

  _finish_action(conn, actions::custom_variables | actions::hosts);
  _mysql.run_query(query, database::mysql_error::clean_customvariables, false,
                   conn);
  _add_action(conn, actions::custom_variables);

  std::lock_guard<std::mutex> l(_group_clean_timer_m);
  _group_clean_timer.expires_after(std::chrono::minutes(1));
  _group_clean_timer.async_wait([this](const asio::error_code& err) {
    if (!err) {
      _clean_group_table();
    }
  });
}

void stream::_clean_group_table() {
  int32_t conn = _mysql.choose_best_connection(-1);
  /* Remove host groups. */
  log_v2::sql()->debug("unified sql: remove empty host groups");
  _mysql.run_query(
      "DELETE hg FROM hostgroups AS hg LEFT JOIN hosts_hostgroups AS hhg ON "
      "hg.hostgroup_id=hhg.hostgroup_id WHERE hhg.hostgroup_id IS NULL",
      database::mysql_error::clean_empty_hostgroups, false, conn);
  _add_action(conn, actions::hostgroups);

  /* Remove service groups. */
  log_v2::sql()->debug("unified sql: remove empty service groups");

  _mysql.run_query(
      "DELETE sg FROM servicegroups AS sg LEFT JOIN services_servicegroups as "
      "ssg ON sg.servicegroup_id=ssg.servicegroup_id WHERE ssg.servicegroup_id "
      "IS NULL",
      database::mysql_error::clean_empty_servicegroups, false, conn);
  _add_action(conn, actions::servicegroups);
}

/**
 *  Update all the hosts and services of unresponsive instances.
 */
void stream::_update_hosts_and_services_of_unresponsive_instances() {
  log_v2::sql()->debug("unified sql: checking for outdated instances");

  /* Don't do anything if timeout is deactivated. */
  if (_instance_timeout == 0)
    return;

  if (_stored_timestamps.size() == 0 ||
      std::difftime(std::time(nullptr), _oldest_timestamp) <= _instance_timeout)
    return;

  /* Update unresponsive instances which were responsive */
  for (std::unordered_map<uint32_t, stored_timestamp>::iterator
           it = _stored_timestamps.begin(),
           end = _stored_timestamps.end();
       it != end; ++it) {
    if (it->second.get_state() == stored_timestamp::responsive &&
        it->second.timestamp_outdated(_instance_timeout)) {
      it->second.set_state(stored_timestamp::unresponsive);
      _update_hosts_and_services_of_instance(it->second.get_id(), false);
    }
  }

  // Update new oldest timestamp
  _oldest_timestamp = timestamp(std::numeric_limits<time_t>::max());
  for (std::unordered_map<uint32_t, stored_timestamp>::iterator
           it = _stored_timestamps.begin(),
           end = _stored_timestamps.end();
       it != end; ++it) {
    if (it->second.get_state() == stored_timestamp::responsive &&
        _oldest_timestamp > it->second.get_timestamp())
      _oldest_timestamp = it->second.get_timestamp();
  }
}

/**
 *  Update the hosts and services of one instance.
 *
 *  @param[in] id         The instance id.
 *  @param[in] responsive True if the instance is responsive, false otherwise.
 */
void stream::_update_hosts_and_services_of_instance(uint32_t id,
                                                    bool responsive) {
  int32_t conn = _mysql.choose_connection_by_instance(id);
  _finish_action(conn, actions::hosts);
  _finish_action(-1, actions::acknowledgements | actions::modules |
                         actions::downtimes | actions::comments);

  std::string query;
  if (responsive) {
    query = fmt::format(
        "UPDATE instances SET outdated=FALSE WHERE instance_id={}", id);
    _mysql.run_query(query, database::mysql_error::restore_instances, false,
                     conn);
    _add_action(conn, actions::instances);
    query = fmt::format(
        "UPDATE hosts AS h LEFT JOIN services AS s ON h.host_id=s.host_id "
        "SET h.state=h.real_state,s.state=s.real_state WHERE h.instance_id={}",
        id);
    _mysql.run_query(query, database::mysql_error::restore_instances, false,
                     conn);
    _add_action(conn, actions::hosts);
  } else {
    query = fmt::format(
        "UPDATE instances SET outdated=TRUE WHERE instance_id={}", id);
    _mysql.run_query(query, database::mysql_error::restore_instances, false,
                     conn);
    _add_action(conn, actions::instances);
    query = fmt::format(
        "UPDATE hosts AS h LEFT JOIN services AS s ON h.host_id=s.host_id "
        "SET h.real_state=h.state,s.real_state=s.state,h.state={},s.state={} "
        "WHERE h.instance_id={}",
        com::centreon::engine::host::state_unreachable,
        com::centreon::engine::service::state_unknown, id);
    _mysql.run_query(query, database::mysql_error::restore_instances, false,
                     conn);
    _add_action(conn, actions::hosts);
  }
  std::shared_ptr<neb::responsive_instance> ri =
      std::make_shared<neb::responsive_instance>();
  ri->poller_id = id;
  ri->responsive = responsive;
  multiplexing::publisher().write(ri);
}

/**
 *  Update the store of living instance timestamps.
 *
 *  @param instance_id The id of the instance to have its timestamp updated.
 */
void stream::_update_timestamp(uint32_t instance_id) {
  stored_timestamp::state_type s{stored_timestamp::responsive};

  // Find the state of an existing timestamp if it exists.
  std::unordered_map<uint32_t, stored_timestamp>::iterator found =
      _stored_timestamps.find(instance_id);
  if (found != _stored_timestamps.end()) {
    s = found->second.get_state();

    // Update a suddenly alive instance
    if (s == stored_timestamp::unresponsive) {
      _update_hosts_and_services_of_instance(instance_id, true);
      s = stored_timestamp::responsive;
    }
  }

  // Insert the timestamp and its state in the store.
  stored_timestamp& timestamp = _stored_timestamps[instance_id];
  timestamp = stored_timestamp(instance_id, s);
  if (_oldest_timestamp > timestamp.get_timestamp())
    _oldest_timestamp = timestamp.get_timestamp();
}

bool stream::_is_valid_poller(uint32_t instance_id) {
  /* Check if the poller of id instance_id is deleted. */
  bool deleted = false;
  if (_cache_deleted_instance_id.find(instance_id) !=
      _cache_deleted_instance_id.end()) {
    log_v2::sql()->info(
        "unified sql: discarding some event related to a deleted poller "
        "({})",
        instance_id);
    deleted = true;
  } else
    /* Update poller timestamp. */
    _update_timestamp(instance_id);

  return !deleted;
}

void stream::_prepare_hg_insupdate_statement() {
  if (!_host_group_insupdate.prepared()) {
    query_preparator::event_unique unique;
    unique.insert("hostgroup_id");
    query_preparator qp(neb::host_group::static_type(), unique);
    _host_group_insupdate = qp.prepare_insert_or_update(_mysql);
  }
}

void stream::_prepare_sg_insupdate_statement() {
  if (!_service_group_insupdate.prepared()) {
    query_preparator::event_unique unique;
    unique.insert("servicegroup_id");
    query_preparator qp(neb::service_group::static_type(), unique);
    _service_group_insupdate = qp.prepare_insert_or_update(_mysql);
  }
}

/**
 *  Process an acknowledgement event.
 *
 *  @param[in] e Uncasted acknowledgement.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_acknowledgement(const std::shared_ptr<io::data>& d) {
  // Cast object.
  neb::acknowledgement const& ack =
      *static_cast<neb::acknowledgement const*>(d.get());

  // Log message.
  log_v2::sql()->info(
      "processing acknowledgement event (poller: {}, host: {}, service: {}, "
      "entry time: {}, deletion time: {})",
      ack.poller_id, ack.host_id, ack.service_id, ack.entry_time,
      ack.deletion_time);

  // Processing.
  if (_is_valid_poller(ack.poller_id)) {
    // Prepare queries.
    if (!_acknowledgement_insupdate.prepared()) {
      query_preparator::event_unique unique;
      unique.insert("entry_time");
      unique.insert("host_id");
      unique.insert("service_id");
      query_preparator qp(neb::acknowledgement::static_type(), unique);
      _acknowledgement_insupdate = qp.prepare_insert_or_update(_mysql);
    }

    int32_t conn = _mysql.choose_connection_by_instance(ack.poller_id);
    // Process object.
    _acknowledgement_insupdate << ack;
    _mysql.run_statement(_acknowledgement_insupdate,
                         database::mysql_error::store_acknowledgement, false,
                         conn);
  }
}

/**
 *  Process a comment event.
 *
 *  @param[in] e  Uncasted comment.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_comment(const std::shared_ptr<io::data>& d) {
  _finish_action(-1, actions::hosts | actions::instances |
                         actions::host_parents | actions::host_dependencies |
                         actions::service_dependencies | actions::comments);

  // Cast object.
  neb::comment const& cmmnt{*static_cast<neb::comment const*>(d.get())};

  int32_t conn = _mysql.choose_connection_by_instance(cmmnt.poller_id);

  // Log message.
  log_v2::sql()->info("SQL: processing comment of poller {} on ({}, {})",
                      cmmnt.poller_id, cmmnt.host_id, cmmnt.service_id);

  // Prepare queries.
  if (!_comment_insupdate.prepared()) {
    query_preparator::event_unique unique;
    unique.insert("host_id");
    unique.insert("service_id");
    unique.insert("entry_time");
    unique.insert("instance_id");
    unique.insert("internal_id");
    query_preparator qp(neb::comment::static_type(), unique);
    _comment_insupdate = qp.prepare_insert_or_update(_mysql);
  }

  // Processing.
  _comment_insupdate << cmmnt;
  _mysql.run_statement(_comment_insupdate, database::mysql_error::store_comment,
                       false, conn);
  _add_action(conn, actions::comments);
}

/**
 *  Process a custom variable event.
 *
 *  @param[in] e Uncasted custom variable.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_custom_variable(const std::shared_ptr<io::data>& d) {
  // Cast object.
  neb::custom_variable const& cv{
      *static_cast<neb::custom_variable const*>(d.get())};

  // Prepare queries.
  if (!_custom_variable_delete.prepared()) {
    query_preparator::event_unique unique;
    unique.insert("host_id");
    unique.insert("name");
    unique.insert("service_id");
    query_preparator qp(neb::custom_variable::static_type(), unique);
    _custom_variable_delete = qp.prepare_delete(_mysql);
  }

  // Processing.
  if (cv.enabled) {
    _cv.push_query(fmt::format(
        "('{}',{},{},'{}',{},{},{},'{}')",
        misc::string::escape(
            cv.name, get_customvariables_col_size(customvariables_name)),
        cv.host_id, cv.service_id,
        misc::string::escape(
            cv.default_value,
            get_customvariables_col_size(customvariables_default_value)),
        cv.modified ? 1 : 0, cv.var_type, cv.update_time,
        misc::string::escape(
            cv.value, get_customvariables_col_size(customvariables_value))));
  } else {
    int conn = special_conn::custom_variable % _mysql.connections_count();
    _finish_action(-1, actions::custom_variables);

    log_v2::sql()->info("SQL: disabling custom variable '{}' of ({}, {})",
                        cv.name, cv.host_id, cv.service_id);
    _custom_variable_delete.bind_value_as_i32_k(":host_id", cv.host_id);
    _custom_variable_delete.bind_value_as_i32_k(":service_id", cv.service_id);
    _custom_variable_delete.bind_value_as_str_k(":name", cv.name);

    _mysql.run_statement(_custom_variable_delete,
                         database::mysql_error::remove_customvariable, false,
                         conn);
    _add_action(conn, actions::custom_variables);
  }
}

/**
 *  Process a custom variable status event.
 *
 *  @param[in] e Uncasted custom variable status.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_custom_variable_status(
    const std::shared_ptr<io::data>& d) {
  // Cast object.
  neb::custom_variable_status const& cv{
      *static_cast<neb::custom_variable_status const*>(d.get())};

  _cvs.push_query(fmt::format(
      "('{}',{},{},{},{},'{}')",
      misc::string::escape(cv.name,
                           get_customvariables_col_size(customvariables_name)),
      cv.host_id, cv.service_id, cv.modified ? 1 : 0, cv.update_time,
      misc::string::escape(
          cv.value, get_customvariables_col_size(customvariables_value))));

  log_v2::sql()->info("SQL: updating custom variable '{}' of ({}, {})", cv.name,
                      cv.host_id, cv.service_id);
}

/**
 *  Process a downtime event.
 *
 *  @param[in] e Uncasted downtime.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_downtime(const std::shared_ptr<io::data>& d) {
  // Cast object.
  neb::downtime const& dd = *static_cast<neb::downtime const*>(d.get());

  // Log message.
  log_v2::sql()->info(
      "SQL: processing downtime event (poller: {}"
      ", host: {}, service: {}, start time: {}, end_time: {}"
      ", actual start time: {}"
      ", actual end time: {}"
      ", duration: {}, entry time: {}"
      ", deletion time: {})",
      dd.poller_id, dd.host_id, dd.service_id, dd.start_time, dd.end_time,
      dd.actual_start_time, dd.actual_end_time, dd.duration, dd.entry_time,
      dd.deletion_time);

  // Check if poller is valid.
  if (_is_valid_poller(dd.poller_id)) {
    _downtimes.push_query(fmt::format(
        "({},{},'{}',{},{},{},{},{},{},{},{},{},{},{},{},{},{},'{}')",
        dd.actual_end_time.is_null() ? "NULL"
                                     : fmt::format("{}", dd.actual_end_time),
        dd.actual_start_time.is_null()
            ? "NULL"
            : fmt::format("{}", dd.actual_start_time),
        misc::string::escape(dd.author,
                             get_downtimes_col_size(downtimes_author)),
        dd.downtime_type,
        dd.deletion_time.is_null() ? "NULL"
                                   : fmt::format("{}", dd.deletion_time),
        dd.duration,
        dd.end_time.is_null() ? "NULL" : fmt::format("{}", dd.end_time),
        dd.entry_time.is_null() ? "NULL" : fmt::format("{}", dd.entry_time),
        dd.fixed, dd.host_id, dd.poller_id, dd.internal_id, dd.service_id,
        dd.start_time.is_null() ? "NULL" : fmt::format("{}", dd.start_time),
        dd.triggered_by == 0 ? "NULL" : fmt::format("{}", dd.triggered_by),
        dd.was_cancelled, dd.was_started,
        misc::string::escape(dd.comment,
                             get_downtimes_col_size(downtimes_comment_data))));
  }
}

/**
 *  Process an event handler event.
 *
 *  @param[in] e Uncasted event handler.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_event_handler(const std::shared_ptr<io::data>& d) {
  // Cast object.
  neb::event_handler const& eh =
      *static_cast<neb::event_handler const*>(d.get());

  // Log message.
  log_v2::sql()->info(
      "SQL: processing event handler event (host: {}"
      ", service: {}, start time {})",
      eh.host_id, eh.service_id, eh.start_time);

  // Prepare queries.
  if (!_event_handler_insupdate.prepared()) {
    query_preparator::event_unique unique;
    unique.insert("host_id");
    unique.insert("service_id");
    unique.insert("start_time");
    query_preparator qp(neb::event_handler::static_type(), unique);
    _event_handler_insupdate = qp.prepare_insert_or_update(_mysql);
  }

  // Processing.
  _event_handler_insupdate << eh;
  _mysql.run_statement(
      _event_handler_insupdate, database::mysql_error::store_eventhandler,
      false,
      _mysql.choose_connection_by_instance(_cache_host_instance[eh.host_id]));
}

/**
 *  Process a flapping status event.
 *
 *  @param[in] e Uncasted flapping status.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_flapping_status(const std::shared_ptr<io::data>& d) {
  // Cast object.
  neb::flapping_status const& fs(
      *static_cast<neb::flapping_status const*>(d.get()));

  // Log message.
  log_v2::sql()->info(
      "SQL: processing flapping status event (host: {}, service: {}, entry "
      "time: {})",
      fs.host_id, fs.service_id, fs.event_time);

  // Prepare queries.
  if (!_flapping_status_insupdate.prepared()) {
    query_preparator::event_unique unique;
    unique.insert("host_id");
    unique.insert("service_id");
    unique.insert("event_time");
    query_preparator qp(neb::flapping_status::static_type(), unique);
    _flapping_status_insupdate = qp.prepare_insert_or_update(_mysql);
  }

  // Processing.
  _flapping_status_insupdate << fs;
  int32_t conn =
      _mysql.choose_connection_by_instance(_cache_host_instance[fs.host_id]);
  _mysql.run_statement(_flapping_status_insupdate,
                       database::mysql_error::store_flapping, false, conn);
  _add_action(conn, actions::hosts);
}

/**
 *  Process an host check event.
 *
 *  @param[in] e Uncasted host check.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_host_check(const std::shared_ptr<io::data>& d) {
  _finish_action(-1, actions::instances | actions::downtimes |
                         actions::comments | actions::host_dependencies |
                         actions::host_parents | actions::service_dependencies);

  // Cast object.
  neb::host_check const& hc = *static_cast<neb::host_check const*>(d.get());

  time_t now = time(nullptr);
  if (hc.check_type ||                  // - passive result
      !hc.active_checks_enabled ||      // - active checks are disabled,
                                        //   status might not be updated
      hc.next_check >= now - 5 * 60 ||  // - normal case
      !hc.next_check) {                 // - initial state
    // Apply to DB.
    log_v2::sql()->info(
        "SQL: processing host check event (host: {}, command: {}", hc.host_id,
        hc.command_line);

    // Prepare queries.
    if (!_host_check_update.prepared()) {
      query_preparator::event_unique unique;
      unique.insert("host_id");
      query_preparator qp(neb::host_check::static_type(), unique);
      _host_check_update = qp.prepare_update(_mysql);
    }

    // Processing.
    bool store;
    size_t str_hash = std::hash<std::string>{}(hc.command_line);
    // Did the command changed since last time?
    if (_cache_hst_cmd[hc.host_id] != str_hash) {
      store = true;
      _cache_hst_cmd[hc.host_id] = str_hash;
    } else
      store = false;

    if (store) {
      int32_t conn = _mysql.choose_connection_by_instance(
          _cache_host_instance[hc.host_id]);

      _host_check_update << hc;
      std::promise<int> promise;
      _mysql.run_statement(_host_check_update,
                           database::mysql_error::store_host_check, false,
                           conn);
      _add_action(conn, actions::hosts);
    }
  } else
    // Do nothing.
    log_v2::sql()->info(
        "SQL: not processing host check event (host: {}, command: {}, check "
        "type: {}, next check: {}, now: {})",
        hc.host_id, hc.command_line, hc.check_type, hc.next_check, now);
}

/**
 *  Process a host dependency event.
 *
 *  @param[in] e Uncasted host dependency.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_host_dependency(const std::shared_ptr<io::data>& d) {
  int32_t conn = special_conn::host_dependency % _mysql.connections_count();
  _finish_action(-1, actions::hosts | actions::host_parents |
                         actions::comments | actions::downtimes |
                         actions::host_dependencies |
                         actions::service_dependencies);

  // Cast object.
  neb::host_dependency const& hd =
      *static_cast<neb::host_dependency const*>(d.get());

  // Insert/Update.
  if (hd.enabled) {
    log_v2::sql()->info("SQL: enabling host dependency of {} on {}",
                        hd.dependent_host_id, hd.host_id);

    // Prepare queries.
    if (!_host_dependency_insupdate.prepared()) {
      query_preparator::event_unique unique;
      unique.insert("host_id");
      unique.insert("dependent_host_id");
      query_preparator qp(neb::host_dependency::static_type(), unique);
      _host_dependency_insupdate = qp.prepare_insert_or_update(_mysql);
    }

    // Process object.
    _host_dependency_insupdate << hd;
    _mysql.run_statement(_host_dependency_insupdate,
                         database::mysql_error::store_host_dependency, false,
                         conn);
    _add_action(conn, actions::host_dependencies);
  }
  // Delete.
  else {
    log_v2::sql()->info("SQL: removing host dependency of {} on {}",
                        hd.dependent_host_id, hd.host_id);
    std::string query(fmt::format(
        "DELETE FROM hosts_hosts_dependencies WHERE dependent_host_id={}"
        " AND host_id={}",
        hd.dependent_host_id, hd.host_id));
    _mysql.run_query(query, database::mysql_error::empty, false, conn);
    _add_action(conn, actions::host_dependencies);
  }
}

/**
 *  Process a host group event.
 *
 *  @param[in] e Uncasted host group.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_host_group(const std::shared_ptr<io::data>& d) {
  int32_t conn = special_conn::host_group % _mysql.connections_count();

  // Cast object.
  const neb::host_group& hg{*static_cast<const neb::host_group*>(d.get())};

  if (hg.enabled) {
    log_v2::sql()->info("SQL: enabling host group {} ('{}' on instance {})",
                        hg.id, hg.name, hg.poller_id);
    _prepare_hg_insupdate_statement();

    _host_group_insupdate << hg;
    _mysql.run_statement(_host_group_insupdate,
                         database::mysql_error::store_host_group, false, conn);
    _hostgroup_cache.insert(hg.id);
  }
  // Delete group.
  else {
    log_v2::sql()->info("SQL: disabling host group {} ('{}' on instance {})",
                        hg.id, hg.name, hg.poller_id);

    // Delete group members.
    {
      _finish_action(-1, actions::hosts);
      std::string query(fmt::format(
          "DELETE hosts_hostgroups FROM hosts_hostgroups LEFT JOIN hosts"
          " ON hosts_hostgroups.host_id=hosts.host_id"
          " WHERE hosts_hostgroups.hostgroup_id={} AND hosts.instance_id={}",
          hg.id, hg.poller_id));
      _mysql.run_query(query, database::mysql_error::empty, false, conn);
      _hostgroup_cache.erase(hg.id);
    }
  }
  _add_action(conn, actions::hostgroups);
}

/**
 *  Process a host group member event.
 *
 *  @param[in] e Uncasted host group member.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_host_group_member(const std::shared_ptr<io::data>& d) {
  int32_t conn = special_conn::host_group % _mysql.connections_count();
  _finish_action(-1, actions::hosts);

  // Cast object.
  const neb::host_group_member& hgm{
      *static_cast<const neb::host_group_member*>(d.get())};

  if (hgm.enabled) {
    // Log message.
    log_v2::sql()->info(
        "SQL: enabling membership of host {} to host group {} on instance {}",
        hgm.host_id, hgm.group_id, hgm.poller_id);

    // We only need to try to insert in this table as the
    // host_id/hostgroup_id should be UNIQUE.
    if (!_host_group_member_insert.prepared()) {
      query_preparator::event_unique unique;
      unique.insert("hostgroup_id");
      unique.insert("host_id");
      query_preparator qp(neb::host_group_member::static_type(), unique);
      _host_group_member_insert = qp.prepare_insert(_mysql);
    }

    /* If the group does not exist, we create it. */
    if (_cache_host_instance[hgm.host_id]) {
      if (_hostgroup_cache.find(hgm.group_id) == _hostgroup_cache.end()) {
        log_v2::sql()->error(
            "SQL: host group {} does not exist - insertion before insertion of "
            "members",
            hgm.group_id);
        _prepare_hg_insupdate_statement();

        neb::host_group hg;
        hg.id = hgm.group_id;
        hg.name = hgm.group_name;
        hg.enabled = true;
        hg.poller_id = _cache_host_instance[hgm.host_id];

        _host_group_insupdate << hg;
        _mysql.run_statement(_host_group_insupdate,
                             database::mysql_error::store_host_group, false,
                             conn);
        _hostgroup_cache.insert(hgm.group_id);
      }

      _host_group_member_insert << hgm;
      _mysql.run_statement(_host_group_member_insert,
                           database::mysql_error::store_host_group_member,
                           false, conn);
      _add_action(conn, actions::hostgroups);
    } else
      log_v2::sql()->error(
          "SQL: host with host_id = {} does not exist - unable to store "
          "unexisting host in a hostgroup. You should restart centengine.",
          hgm.host_id);
  }
  // Delete.
  else {
    // Log message.
    log_v2::sql()->info(
        "SQL: disabling membership of host {} to host group {} on instance {}",
        hgm.host_id, hgm.group_id, hgm.poller_id);

    if (!_host_group_member_delete.prepared()) {
      query_preparator::event_unique unique;
      unique.insert("hostgroup_id");
      unique.insert("host_id");
      query_preparator qp(neb::host_group_member::static_type(), unique);
      _host_group_member_delete = qp.prepare_delete(_mysql);
    }
    _host_group_member_delete << hgm;
    _mysql.run_statement(_host_group_member_delete,
                         database::mysql_error::delete_host_group_member, false,
                         conn);
    _add_action(conn, actions::hostgroups);
  }
}

/**
 *  Process an host event.
 *
 *  @param[in] e Uncasted host.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_host(const std::shared_ptr<io::data>& d) {
  _finish_action(-1, actions::instances | actions::hostgroups |
                         actions::host_dependencies | actions::host_parents |
                         actions::custom_variables | actions::downtimes |
                         actions::comments | actions::service_dependencies);
  neb::host& h = *static_cast<neb::host*>(d.get());

  // Log message.
  log_v2::sql()->info(
      "SQL: processing host event (poller: {}, host: {}, name: {})",
      h.poller_id, h.host_id, h.host_name);

  // Processing
  if (_is_valid_poller(h.poller_id)) {
    // FixMe BAM Generate fake host, this host
    // does not contains a display_name
    // We should not store them in db
    if (h.host_id && !h.alias.empty()) {
      int32_t conn = _mysql.choose_connection_by_instance(h.poller_id);

      // Prepare queries.
      if (!_host_insupdate.prepared()) {
        query_preparator::event_unique unique;
        unique.insert("host_id");
        query_preparator qp(neb::host::static_type(), unique);
        _host_insupdate = qp.prepare_insert_or_update(_mysql);
      }

      // Process object.
      _host_insupdate << h;
      _mysql.run_statement(_host_insupdate, database::mysql_error::store_host,
                           false, conn);
      _add_action(conn, actions::hosts);

      // Fill the cache...
      if (h.enabled)
        _cache_host_instance[h.host_id] = h.poller_id;
      else
        _cache_host_instance.erase(h.host_id);
    } else
      log_v2::sql()->trace(
          "SQL: host '{}' of poller {} has no ID nor alias, probably bam "
          "fake host",
          h.host_name, h.poller_id);
  }
}

/**
 *  Process a host parent event.
 *
 *  @param[in] e Uncasted host parent.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_host_parent(const std::shared_ptr<io::data>& d) {
  int32_t conn = special_conn::host_parent % _mysql.connections_count();
  _finish_action(-1, actions::hosts | actions::host_dependencies |
                         actions::comments | actions::downtimes);

  neb::host_parent const& hp(*static_cast<neb::host_parent const*>(d.get()));

  // Enable parenting.
  if (hp.enabled) {
    // Log message.
    log_v2::sql()->info("SQL: host {} is parent of host {}", hp.parent_id,
                        hp.host_id);

    // Prepare queries.
    if (!_host_parent_insert.prepared()) {
      query_preparator qp(neb::host_parent::static_type());
      _host_parent_insert = qp.prepare_insert(_mysql, true);
    }

    // Insert.
    _host_parent_insert << hp;
    _mysql.run_statement(_host_parent_insert,
                         database::mysql_error::store_host_parentship, false,
                         conn);
    _add_action(conn, actions::host_parents);
  }
  // Disable parenting.
  else {
    log_v2::sql()->info("SQL: host {} is not parent of host {} anymore",
                        hp.parent_id, hp.host_id);

    // Prepare queries.
    if (!_host_parent_delete.prepared()) {
      query_preparator::event_unique unique;
      unique.insert("child_id");
      unique.insert("parent_id");
      query_preparator qp(neb::host_parent::static_type(), unique);
      _host_parent_delete = qp.prepare_delete(_mysql);
    }

    // Delete.
    _host_parent_delete << hp;
    _mysql.run_statement(_host_parent_delete, database::mysql_error::empty,
                         false, conn);
    _add_action(conn, actions::host_parents);
  }
}

/**
 *  Process a host status event.
 *
 *  @param[in] e Uncasted host status.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_host_status(const std::shared_ptr<io::data>& d) {
  if (!_store_in_hosts_services)
    return;

  _finish_action(-1, actions::instances | actions::downtimes |
                         actions::comments | actions::custom_variables |
                         actions::hostgroups | actions::host_dependencies |
                         actions::host_parents);

  // Processed object.
  neb::host_status const& hs(*static_cast<neb::host_status const*>(d.get()));

  time_t now = time(nullptr);
  if (hs.check_type ||                  // - passive result
      !hs.active_checks_enabled ||      // - active checks are disabled,
                                        //   status might not be updated
      hs.next_check >= now - 5 * 60 ||  // - normal case
      !hs.next_check) {                 // - initial state
    // Apply to DB.
    log_v2::sql()->info(
        "processing host status event (host: {}, last check: {}, state ({}, "
        "{}))",
        hs.host_id, hs.last_check, hs.current_state, hs.state_type);

    // Prepare queries.
    if (!_host_status_update.prepared()) {
      query_preparator::event_unique unique;
      unique.insert("host_id");
      query_preparator qp(neb::host_status::static_type(), unique);
      _host_status_update = qp.prepare_update(_mysql);
    }

    // Processing.
    _host_status_update << hs;
    int32_t conn =
        _mysql.choose_connection_by_instance(_cache_host_instance[hs.host_id]);
    _mysql.run_statement(_host_status_update,
                         database::mysql_error::store_host_status, false, conn);
    _add_action(conn, actions::hosts);
  } else
    // Do nothing.
    log_v2::sql()->info(
        "SQL: not processing host status event (id: {}, check type: {}, last "
        "check: {}, next check: {}, now: {}, state: ({}, {}))",
        hs.host_id, hs.check_type, hs.last_check, hs.next_check, now,
        hs.current_state, hs.state_type);
}

/**
 *  Process a host status protobuf event.
 *
 *  @param[in] e Uncasted host status.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_pb_host(const std::shared_ptr<io::data>& d) {
  _finish_action(-1, actions::instances | actions::hostgroups |
                         actions::host_dependencies | actions::host_parents |
                         actions::custom_variables | actions::downtimes |
                         actions::comments | actions::service_dependencies |
                         actions::severities);
  auto hst{static_cast<const neb::pb_host*>(d.get())};
  auto& h = hst->obj();

  // Log message.
  log_v2::sql()->info(
      "SQL: processing pb host event (poller: {}, host: {}, name: {})",
      h.instance_id(), h.host_id(), h.name());

  // Processing
  if (_is_valid_poller(h.instance_id())) {
    // FixMe BAM Generate fake host, this host
    // does not contain a display_name
    // We should not store them in db
    if (h.host_id() && !h.alias().empty()) {
      int32_t conn = _mysql.choose_connection_by_instance(h.instance_id());

      // Prepare queries.
      if (!_pb_host_insupdate.prepared()) {
        query_preparator::event_pb_unique unique{
            {1, "host_id", io::protobuf_base::invalid_on_zero, 0}};
        query_preparator qp(neb::pb_host::static_type(), unique);
        _pb_host_insupdate = qp.prepare_insert_or_update_table(
            _mysql, "hosts",
            {
                {1, "host_id", io::protobuf_base::invalid_on_zero, 0},
                {2, "acknowledged", 0, 0},
                {3, "acknowledgement_type", 0, 0},
                {4, "active_checks", 0, 0},
                {5, "enabled", 0, 0},
                {6, "scheduled_downtime_depth", 0, 0},
                {7, "check_command", 0,
                 get_hosts_col_size(hosts_check_command)},
                {8, "check_interval", 0, 0},
                {9, "check_period", 0, get_hosts_col_size(hosts_check_period)},
                {10, "check_type", 0, 0},
                {11, "check_attempt", 0, 0},
                {12, "state", 0, 0},
                {13, "event_handler_enabled", 0, 0},
                {14, "event_handler", 0,
                 get_hosts_col_size(hosts_event_handler)},
                {15, "execution_time", 0, 0},
                {16, "flap_detection", 0, 0},
                {17, "checked", 0, 0},
                {18, "flapping", 0, 0},
                {19, "last_check", io::protobuf_base::invalid_on_zero, 0},
                {20, "last_hard_state", 0, 0},
                {21, "last_hard_state_change",
                 io::protobuf_base::invalid_on_zero, 0},
                {22, "last_notification", io::protobuf_base::invalid_on_zero,
                 0},
                {23, "notification_number", 0, 0},
                {24, "last_state_change", io::protobuf_base::invalid_on_zero,
                 0},
                {25, "last_time_down", io::protobuf_base::invalid_on_zero, 0},
                {26, "last_time_unreachable",
                 io::protobuf_base::invalid_on_zero, 0},
                {27, "last_time_up", io::protobuf_base::invalid_on_zero, 0},
                {28, "last_update", io::protobuf_base::invalid_on_zero, 0},
                {29, "latency", 0, 0},
                {30, "max_check_attempts", 0, 0},
                {31, "next_check", io::protobuf_base::invalid_on_zero, 0},
                {32, "next_host_notification",
                 io::protobuf_base::invalid_on_zero, 0},
                {33, "no_more_notifications", 0, 0},
                {34, "notify", 0, 0},
                {35, "output", 0, get_hosts_col_size(hosts_output)},
                {36, "passive_checks", 0, 0},
                {37, "percent_state_change", 0, 0},
                {38, "perfdata", 0, get_hosts_col_size(hosts_perfdata)},
                {39, "retry_interval", 0, 0},
                {40, "should_be_scheduled", 0, 0},
                {41, "obsess_over_host", 0, 0},
                {42, "state_type", 0, 0},
                {43, "action_url", 0, get_hosts_col_size(hosts_action_url)},
                {44, "address", 0, get_hosts_col_size(hosts_address)},
                {45, "alias", 0, get_hosts_col_size(hosts_alias)},
                {46, "check_freshness", 0, 0},
                {47, "default_active_checks", 0, 0},
                {48, "default_event_handler_enabled", 0, 0},
                {49, "default_flap_detection", 0, 0},
                {50, "default_notify", 0, 0},
                {51, "default_passive_checks", 0, 0},
                {52, "display_name", 0, get_hosts_col_size(hosts_display_name)},
                {53, "first_notification_delay", 0, 0},
                {54, "flap_detection_on_down", 0, 0},
                {55, "flap_detection_on_unreachable", 0, 0},
                {56, "flap_detection_on_up", 0, 0},
                {57, "freshness_threshold", 0, 0},
                {58, "high_flap_threshold", 0, 0},
                {59, "name", 0, get_hosts_col_size(hosts_name)},
                {60, "icon_image", 0, get_hosts_col_size(hosts_icon_image)},
                {61, "icon_image_alt", 0,
                 get_hosts_col_size(hosts_icon_image_alt)},
                {62, "instance_id", mapping::entry::invalid_on_zero, 0},
                {63, "low_flap_threshold", 0, 0},
                {64, "notes", 0, get_hosts_col_size(hosts_notes)},
                {65, "notes_url", 0, get_hosts_col_size(hosts_notes_url)},
                {66, "notification_interval", 0, 0},
                {67, "notification_period", 0,
                 get_hosts_col_size(hosts_notification_period)},
                {68, "notify_on_down", 0, 0},
                {69, "notify_on_downtime", 0, 0},
                {70, "notify_on_flapping", 0, 0},
                {71, "notify_on_recovery", 0, 0},
                {72, "notify_on_unreachable", 0, 0},
                {73, "stalk_on_down", 0, 0},
                {74, "stalk_on_unreachable", 0, 0},
                {75, "stalk_on_up", 0, 0},
                {76, "statusmap_image", 0,
                 get_hosts_col_size(hosts_statusmap_image)},
                {77, "retain_nonstatus_information", 0, 0},
                {78, "retain_status_information", 0, 0},
                {79, "timezone", 0, get_hosts_col_size(hosts_timezone)},
            });
        if (_store_in_resources) {
          _resources_host_insert = _mysql.prepare_query(
              "INSERT INTO resources "
              "(id,parent_id,type,status,status_ordered,last_"
              "status_change,"
              "in_downtime,acknowledged,"
              "status_confirmed,check_attempts,max_check_attempts,poller_id,"
              "severity_id,name,address,alias,parent_name,notes_url,notes,"
              "action_url,"
              "notifications_enabled,passive_checks_enabled,"
              "active_checks_enabled,enabled,icon_id) "
              "VALUES(?,0,1,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,1,?)");
          _resources_host_update = _mysql.prepare_query(
              "UPDATE resources SET "
              "type=1,status=?,status_ordered=?,last_status_change=?,"
              "in_downtime=?,acknowledged=?,"
              "status_confirmed=?,check_attempts=?,max_check_attempts=?,"
              "poller_id=?,severity_id=?,name=?,address=?,alias=?,"
              "parent_name=?,notes_url=?,notes=?,action_url=?,"
              "notifications_enabled=?,passive_checks_enabled=?,"
              "active_checks_enabled=?,icon_id=?,enabled=1 WHERE "
              "resource_id=?");
          if (!_resources_tags_remove.prepared())
            _resources_tags_remove = _mysql.prepare_query(
                "DELETE FROM resources_tags WHERE resource_id=?");
          if (!_resources_disable.prepared()) {
            _resources_disable = _mysql.prepare_query(
                "UPDATE resources SET enabled=0 WHERE resource_id=?");
          }
        }
      }

      // Process object.
      _pb_host_insupdate << *hst;
      _mysql.run_statement(_pb_host_insupdate,
                           database::mysql_error::store_host, false, conn);
      _add_action(conn, actions::hosts);

      // Fill the cache...
      if (h.enabled())
        _cache_host_instance[h.host_id()] = h.instance_id();
      else
        _cache_host_instance.erase(h.host_id());

      if (_store_in_resources) {
        uint64_t res_id = 0;
        auto found = _resource_cache.find({h.host_id(), 0});

        if (h.enabled()) {
          uint64_t sid = 0;
          fmt::string_view name{misc::string::truncate(
              h.name(), get_resources_col_size(resources_name))};
          fmt::string_view address{misc::string::truncate(
              h.address(), get_resources_col_size(resources_address))};
          fmt::string_view alias{misc::string::truncate(
              h.alias(), get_resources_col_size(resources_alias))};
          fmt::string_view parent_name{misc::string::truncate(
              h.name(), get_resources_col_size(resources_parent_name))};
          fmt::string_view notes_url{misc::string::truncate(
              h.notes_url(), get_resources_col_size(resources_notes_url))};
          fmt::string_view notes{misc::string::truncate(
              h.notes(), get_resources_col_size(resources_notes))};
          fmt::string_view action_url{misc::string::truncate(
              h.action_url(), get_resources_col_size(resources_action_url))};

          // INSERT
          if (found == _resource_cache.end()) {
            _resources_host_insert.bind_value_as_u64(0, h.host_id());
            _resources_host_insert.bind_value_as_u32(1, h.state());
            _resources_host_insert.bind_value_as_u32(
                2, hst_ordered_status[h.state()]);
            _resources_host_insert.bind_value_as_u64(3, h.last_state_change());
            _resources_host_insert.bind_value_as_bool(
                4, h.scheduled_downtime_depth() > 0);
            _resources_host_insert.bind_value_as_bool(
                5, h.acknowledgement_type() != Host_AckType_NONE);
            _resources_host_insert.bind_value_as_bool(
                6, h.state_type() == Host_StateType_HARD);
            _resources_host_insert.bind_value_as_u32(7, h.check_attempt());
            _resources_host_insert.bind_value_as_u32(8, h.max_check_attempts());
            _resources_host_insert.bind_value_as_u64(
                9, _cache_host_instance[h.host_id()]);
            if (h.severity_id()) {
              sid = _severity_cache[{h.severity_id(), 1}];
              log_v2::sql()->debug("host {} with severity_id {} => uid = {}",
                                   h.host_id(), h.severity_id(), sid);
            } else
              log_v2::sql()->info("no host severity found in cache for host {}",
                                  h.host_id());
            if (sid)
              _resources_host_insert.bind_value_as_u64(10, sid);
            else
              _resources_host_insert.bind_null_u64(10);
            _resources_host_insert.bind_value_as_str(11, name);
            _resources_host_insert.bind_value_as_str(12, address);
            _resources_host_insert.bind_value_as_str(13, alias);
            _resources_host_insert.bind_value_as_str(14, parent_name);
            _resources_host_insert.bind_value_as_str(15, notes_url);
            _resources_host_insert.bind_value_as_str(16, notes);
            _resources_host_insert.bind_value_as_str(17, action_url);
            _resources_host_insert.bind_value_as_bool(18, h.notify());
            _resources_host_insert.bind_value_as_bool(19, h.passive_checks());
            _resources_host_insert.bind_value_as_bool(20, h.active_checks());
            _resources_host_insert.bind_value_as_u64(21, h.icon_id());

            std::promise<uint64_t> p;
            std::future<uint64_t> future = p.get_future();
            _mysql.run_statement_and_get_int<uint64_t>(
                _resources_host_insert, std::move(p),
                database::mysql_task::LAST_INSERT_ID, conn);
            _add_action(conn, actions::resources);
            try {
              res_id = future.get();
              _resource_cache.insert({{h.host_id(), 0}, res_id});
            } catch (const std::exception& e) {
              log_v2::sql()->critical(
                  "SQL: unable to insert new host resource {}: {}", h.host_id(),
                  e.what());

              std::promise<mysql_result> promise_resource;
              std::future<mysql_result> future_resource =
                  promise_resource.get_future();
              _mysql.run_query_and_get_result(
                  fmt::format("SELECT resource_id FROM resources WHERE "
                              "parent_id=0 AND id={}",
                              h.host_id()),
                  std::move(promise_resource));
              try {
                mysql_result res{future_resource.get()};
                if (_mysql.fetch_row(res)) {
                  auto r = _resource_cache.insert(
                      {{h.host_id(), 0}, res.value_as_u64(0)});
                  found = r.first;
                  log_v2::sql()->debug(
                      "Host resource (host {}) found in database with id {}",
                      h.host_id(), found->second);
                }
              } catch (const std::exception& e) {
                log_v2::sql()->critical(
                    "No host resource in database with id {}: {}", h.host_id(),
                    e.what());
                return;
              }
            }
          }
          if (res_id == 0) {
            res_id = found->second;
            // UPDATE
            _resources_host_update.bind_value_as_u32(0, h.state());
            _resources_host_update.bind_value_as_u32(
                1, hst_ordered_status[h.state()]);
            _resources_host_update.bind_value_as_u64(2, h.last_state_change());
            _resources_host_update.bind_value_as_bool(
                3, h.scheduled_downtime_depth() > 0);
            _resources_host_update.bind_value_as_bool(
                4, h.acknowledgement_type() != Host_AckType_NONE);
            _resources_host_update.bind_value_as_bool(
                5, h.state_type() == Host_StateType_HARD);
            _resources_host_update.bind_value_as_u32(6, h.check_attempt());
            _resources_host_update.bind_value_as_u32(7, h.max_check_attempts());
            _resources_host_update.bind_value_as_u64(
                8, _cache_host_instance[h.host_id()]);
            if (h.severity_id()) {
              sid = _severity_cache[{h.severity_id(), 1}];
              log_v2::sql()->debug("host {} with severity_id {} => uid = {}",
                                   h.host_id(), h.severity_id(), sid);
            } else
              log_v2::sql()->info("no host severity found in cache for host {}",
                                  h.host_id());
            if (sid)
              _resources_host_update.bind_value_as_u64(9, sid);
            else
              _resources_host_update.bind_null_u64(9);
            _resources_host_update.bind_value_as_str(10, name);
            _resources_host_update.bind_value_as_str(11, address);
            _resources_host_update.bind_value_as_str(12, alias);
            _resources_host_update.bind_value_as_str(13, parent_name);
            _resources_host_update.bind_value_as_str(14, notes_url);
            _resources_host_update.bind_value_as_str(15, notes);
            _resources_host_update.bind_value_as_str(16, action_url);
            _resources_host_update.bind_value_as_bool(17, h.notify());
            _resources_host_update.bind_value_as_bool(18, h.passive_checks());
            _resources_host_update.bind_value_as_bool(19, h.active_checks());
            _resources_host_update.bind_value_as_u64(20, h.icon_id());
            _resources_host_update.bind_value_as_u64(21, res_id);

            _mysql.run_statement(_resources_host_update,
                                 database::mysql_error::store_host_resources,
                                 false, conn);
            _add_action(conn, actions::resources);
          }

          if (!_resources_tags_insert.prepared()) {
            _resources_tags_insert = _mysql.prepare_query(
                "INSERT INTO resources_tags (tag_id,resource_id) VALUES(?,?)");
          }
          if (!_resources_tags_remove.prepared())
            _resources_tags_remove = _mysql.prepare_query(
                "DELETE FROM resources_tags WHERE resource_id=?");
          _finish_action(-1, actions::tags);
          _resources_tags_remove.bind_value_as_u64(0, res_id);
          _mysql.run_statement(_resources_tags_remove,
                               database::mysql_error::delete_resources_tags,
                               false, conn);
          for (auto& tag : h.tags()) {
            auto it_tags_cache = _tags_cache.find({tag.id(), tag.type()});

            if (it_tags_cache == _tags_cache.end()) {
              log_v2::sql()->error(
                  "SQL: could not find in cache the tag ({}, {}) for host "
                  "'{}': "
                  "trying to add it.",
                  tag.id(), tag.type(), h.host_id());
              if (!_tag_insert.prepared())
                _tag_insert = _mysql.prepare_query(
                    "INSERT INTO tags (id,type,name) VALUES(?,?,?)");
              _tag_insert.bind_value_as_u64(0, tag.id());
              _tag_insert.bind_value_as_u32(1, tag.type());
              _tag_insert.bind_value_as_str(2, "(unknown)");
              std::promise<uint64_t> p;
              std::future<uint64_t> future = p.get_future();

              _mysql.run_statement_and_get_int<uint64_t>(
                  _tag_insert, std::move(p),
                  database::mysql_task::LAST_INSERT_ID, conn);
              try {
                uint64_t tag_id = future.get();
                it_tags_cache =
                    _tags_cache.insert({{tag.id(), tag.type()}, tag_id}).first;
              } catch (const std::exception& e) {
                log_v2::sql()->error(
                    "SQL: unable to insert new tag ({},{}): {}", tag.id(),
                    tag.type(), e.what());
              }
            }

            if (it_tags_cache != _tags_cache.end()) {
              _resources_tags_insert.bind_value_as_u64(0,
                                                       it_tags_cache->second);
              _resources_tags_insert.bind_value_as_u64(1, res_id);
              log_v2::sql()->debug(
                  "SQL: new relation between host (resource_id: {}, host_id: "
                  "{}) "
                  "and tag ({},{})",
                  res_id, h.host_id(), tag.id(), tag.type());
              _mysql.run_statement(
                  _resources_tags_insert,
                  database::mysql_error::store_tags_resources_tags, false,
                  conn);
              _add_action(conn, actions::resources_tags);
            }
          }
        } else {
          if (found != _resource_cache.end()) {
            _resources_disable.bind_value_as_u64(0, found->second);

            _mysql.run_statement(_resources_disable,
                                 database::mysql_error::clean_resources, false,
                                 conn);
            _resource_cache.erase(found);
            _add_action(conn, actions::resources);
          } else {
            log_v2::sql()->info(
                "SQL: no need to remove host {}, it is not in database",
                h.host_id());
          }
        }
      }
    } else
      log_v2::sql()->trace(
          "SQL: host '{}' of poller {} has no ID nor alias, probably bam "
          "fake host",
          h.name(), h.instance_id());
  }
}

/**
 *  Process an adaptive host event.
 *
 *  @param[in] e Uncasted host.
 *
 */
void stream::_process_pb_adaptive_host(const std::shared_ptr<io::data>& d) {
  log_v2::sql()->info("SQL: processing pb adaptive host");
  _finish_action(-1, actions::host_parents | actions::comments |
                         actions::downtimes | actions::host_dependencies |
                         actions::service_dependencies);
  // Processed object.
  auto h{static_cast<const neb::pb_adaptive_host*>(d.get())};
  auto& ah = h->obj();
  if (_cache_host_instance[ah.host_id()]) {
    int32_t conn = _mysql.choose_connection_by_instance(
        _cache_host_instance[static_cast<uint32_t>(ah.host_id())]);

    constexpr const char* buf = "UPDATE hosts SET";
    constexpr size_t size = strlen(buf);
    std::string query{buf};
    if (ah.has_notify())
      query += fmt::format(" notify='{}',", ah.notify() ? 1 : 0);
    if (ah.has_active_checks())
      query += fmt::format(" active_checks='{}',", ah.active_checks() ? 1 : 0);
    if (ah.has_should_be_scheduled())
      query += fmt::format(" should_be_scheduled='{}',",
                           ah.should_be_scheduled() ? 1 : 0);
    if (ah.has_passive_checks())
      query +=
          fmt::format(" passive_checks='{}',", ah.passive_checks() ? 1 : 0);
    if (ah.has_event_handler_enabled())
      query += fmt::format(" event_handler_enabled='{}',",
                           ah.event_handler_enabled() ? 1 : 0);
    if (ah.has_flap_detection())
      query +=
          fmt::format(" flap_detection='{}',", ah.flap_detection() ? 1 : 0);
    if (ah.has_obsess_over_host())
      query +=
          fmt::format(" obsess_over_host='{}',", ah.obsess_over_host() ? 1 : 0);
    if (ah.has_event_handler())
      query += fmt::format(
          " event_handler='{}',",
          misc::string::escape(ah.event_handler(),
                               get_hosts_col_size(hosts_event_handler)));
    if (ah.has_check_command())
      query += fmt::format(
          " check_command='{}',",
          misc::string::escape(ah.check_command(),
                               get_hosts_col_size(hosts_check_command)));
    if (ah.has_check_interval())
      query += fmt::format(" check_interval={},", ah.check_interval());
    if (ah.has_retry_interval())
      query += fmt::format(" retry_interval={},", ah.retry_interval());
    if (ah.has_max_check_attempts())
      query += fmt::format(" max_check_attempts={},", ah.max_check_attempts());
    if (ah.has_check_freshness())
      query +=
          fmt::format(" check_freshness='{}',", ah.check_freshness() ? 1 : 0);
    if (ah.has_check_period())
      query += fmt::format(
          " check_period='{}',",
          misc::string::escape(ah.check_period(),
                               get_hosts_col_size(hosts_check_period)));
    if (ah.has_notification_period())
      query +=
          fmt::format(" notification_period='{}',",
                      misc::string::escape(
                          ah.notification_period(),
                          get_services_col_size(services_notification_period)));

    // If nothing was added to query, we can exit immediately.
    if (query.size() > size) {
      query.resize(query.size() - 1);
      query += fmt::format(" WHERE host_id={}", ah.host_id());
      log_v2::sql()->trace("SQL: query <<{}>>", query);
      _mysql.run_query(query, database::mysql_error::store_host, false, conn);
      _add_action(conn, actions::hosts);

      if (_store_in_resources) {
        constexpr const char* res_buf = "UPDATE resources SET";
        constexpr size_t res_size = strlen(res_buf);
        std::string res_query{res_buf};
        if (ah.has_notify())
          res_query +=
              fmt::format(" notifications_enabled='{}',", ah.notify() ? 1 : 0);
        if (ah.has_active_checks())
          res_query += fmt::format(" active_checks_enabled='{}',",
                                   ah.active_checks() ? 1 : 0);
        if (ah.has_passive_checks())
          res_query += fmt::format(" passive_checks_enabled='{}',",
                                   ah.passive_checks() ? 1 : 0);
        if (ah.has_max_check_attempts())
          res_query +=
              fmt::format(" max_check_attempts={},", ah.max_check_attempts());

        if (res_query.size() > res_size) {
          res_query.resize(res_query.size() - 1);
          res_query +=
              fmt::format(" WHERE parent_id=0 AND id={}", ah.host_id());
          log_v2::sql()->trace("SQL: query <<{}>>", res_query);
          _mysql.run_query(res_query, database::mysql_error::update_resources,
                           false, conn);
          _add_action(conn, actions::resources);
        }
      }
    }
  } else
    log_v2::sql()->error(
        "SQL: host with host_id = {} does not exist - unable to store service "
        "of that host. You should restart centengine",
        ah.host_id());
}

/**
 *  Process a host status check result event.
 *
 *  @param[in] e Uncasted service status.
 *
 */
void stream::_process_pb_host_status(const std::shared_ptr<io::data>& d) {
  _finish_action(-1, actions::host_parents | actions::comments |
                         actions::downtimes | actions::host_dependencies);
  // Processed object.
  auto h{static_cast<const neb::pb_host_status*>(d.get())};
  auto& hscr = h->obj();

  log_v2::sql()->debug("SQL: pb host status check result output: <<{}>>",
                       hscr.output());
  log_v2::sql()->debug("SQL: pb host status check result perfdata: <<{}>>",
                       hscr.perfdata());

  time_t now = time(nullptr);
  if (hscr.check_type() == HostStatus_CheckType_PASSIVE ||
      hscr.next_check() >= now - 5 * 60 ||  // usual case
      hscr.next_check() == 0) {             // initial state
    // Apply to DB.
    log_v2::sql()->info(
        "SQL: processing host status check result event proto (host: {}, "
        "last check: {}, state ({}, {}))",
        hscr.host_id(), hscr.last_check(), hscr.state(), hscr.state_type());

    // Processing.
    if (_store_in_hosts_services) {
      int32_t conn = _mysql.choose_connection_by_instance(
          _cache_host_instance[static_cast<uint32_t>(hscr.host_id())]);
      if (_bulk_prepared_statement) {
        if (!_hscr_bind->bind(conn))
          _hscr_bind->init_from_stmt(conn);
        auto* b = _hscr_bind->bind(conn).get();
        b->set_value_as_bool(0, hscr.checked());
        b->set_value_as_i32(1, hscr.check_type());
        b->set_value_as_i32(2, hscr.state());
        b->set_value_as_i32(3, hscr.state_type());
        b->set_value_as_i64(4, hscr.last_state_change());
        b->set_value_as_i32(5, hscr.last_hard_state());
        b->set_value_as_i64(6, hscr.last_hard_state_change());
        b->set_value_as_i64(7, hscr.last_time_up());
        b->set_value_as_i64(8, hscr.last_time_down());
        b->set_value_as_i64(9, hscr.last_time_unreachable());
        std::string full_output{
            fmt::format("{}\n{}", hscr.output(), hscr.long_output())};
        size_t size = misc::string::adjust_size_utf8(
            full_output, get_hosts_col_size(hosts_output));
        b->set_value_as_str(10, fmt::string_view(full_output.data(), size));
        size = misc::string::adjust_size_utf8(
            hscr.perfdata(), get_hosts_col_size(hosts_perfdata));
        b->set_value_as_str(11, fmt::string_view(hscr.perfdata().data(), size));
        b->set_value_as_bool(12, hscr.flapping());
        b->set_value_as_f64(13, hscr.percent_state_change());
        b->set_value_as_f64(14, hscr.latency());
        b->set_value_as_f64(15, hscr.execution_time());
        if (hscr.last_check() == 0)
          b->set_null_i64(16);
        else
          b->set_value_as_i64(16, hscr.last_check());
        b->set_value_as_i64(17, hscr.next_check());
        b->set_value_as_bool(18, hscr.should_be_scheduled());
        b->set_value_as_i32(19, hscr.check_attempt());
        b->set_value_as_i32(20, hscr.notification_number());
        b->set_value_as_bool(21, hscr.no_more_notifications());
        b->set_value_as_i64(22, hscr.last_notification());
        b->set_value_as_i64(23, hscr.next_host_notification());
        b->set_value_as_bool(
            24, hscr.acknowledgement_type() != HostStatus_AckType_NONE);
        b->set_value_as_i32(25, hscr.acknowledgement_type());
        b->set_value_as_i32(26, hscr.scheduled_downtime_depth());
        b->set_value_as_i32(27, hscr.host_id());
        b->next_row();
        log_v2::sql()->trace("{} waiting updates for host status in hosts",
                             b->current_row());
      } else {
        _hscr_update->bind_value_as_bool(0, hscr.checked());
        _hscr_update->bind_value_as_i32(1, hscr.check_type());
        _hscr_update->bind_value_as_i32(2, hscr.state());
        _hscr_update->bind_value_as_i32(3, hscr.state_type());
        _hscr_update->bind_value_as_i64(4, hscr.last_state_change());
        _hscr_update->bind_value_as_i32(5, hscr.last_hard_state());
        _hscr_update->bind_value_as_i64(6, hscr.last_hard_state_change());
        _hscr_update->bind_value_as_i64(7, hscr.last_time_up());
        _hscr_update->bind_value_as_i64(8, hscr.last_time_down());
        _hscr_update->bind_value_as_i64(9, hscr.last_time_unreachable());
        std::string full_output{
            fmt::format("{}\n{}", hscr.output(), hscr.long_output())};
        size_t size = misc::string::adjust_size_utf8(
            full_output, get_hosts_col_size(hosts_output));
        _hscr_update->bind_value_as_str(
            10, fmt::string_view(full_output.data(), size));
        size = misc::string::adjust_size_utf8(
            hscr.perfdata(), get_hosts_col_size(hosts_perfdata));
        _hscr_update->bind_value_as_str(
            11, fmt::string_view(hscr.perfdata().data(), size));
        _hscr_update->bind_value_as_bool(12, hscr.flapping());
        _hscr_update->bind_value_as_f64(13, hscr.percent_state_change());
        _hscr_update->bind_value_as_f64(14, hscr.latency());
        _hscr_update->bind_value_as_f64(15, hscr.execution_time());
        _hscr_update->bind_value_as_i64(16, hscr.last_check(), is_not_zero);
        _hscr_update->bind_value_as_i64(17, hscr.next_check());
        _hscr_update->bind_value_as_bool(18, hscr.should_be_scheduled());
        _hscr_update->bind_value_as_i32(19, hscr.check_attempt());
        _hscr_update->bind_value_as_i32(20, hscr.notification_number());
        _hscr_update->bind_value_as_bool(21, hscr.no_more_notifications());
        _hscr_update->bind_value_as_i64(22, hscr.last_notification());
        _hscr_update->bind_value_as_i64(23, hscr.next_host_notification());
        _hscr_update->bind_value_as_bool(
            24, hscr.acknowledgement_type() != HostStatus_AckType_NONE);
        _hscr_update->bind_value_as_i32(25, hscr.acknowledgement_type());
        _hscr_update->bind_value_as_i32(26, hscr.scheduled_downtime_depth());
        _hscr_update->bind_value_as_i32(27, hscr.host_id());

        _mysql.run_statement(*_hscr_update,
                             database::mysql_error::store_host_status, false,
                             conn);

        _add_action(conn, actions::hosts);
      }
    }

    if (_store_in_resources) {
      int32_t conn = _mysql.choose_connection_by_instance(
          _cache_host_instance[static_cast<uint32_t>(hscr.host_id())]);
      if (_bulk_prepared_statement) {
        if (!_hscr_resources_bind->bind(conn))
          _hscr_resources_bind->init_from_stmt(conn);
        auto* b = _hscr_resources_bind->bind(conn).get();
        b->set_value_as_i32(0, hscr.state());
        b->set_value_as_i32(1, hst_ordered_status[hscr.state()]);
        b->set_value_as_u64(2, hscr.last_state_change());
        b->set_value_as_bool(3, hscr.scheduled_downtime_depth() > 0);
        b->set_value_as_bool(
            4, hscr.acknowledgement_type() != HostStatus_AckType_NONE);
        b->set_value_as_bool(5, hscr.state_type() == HostStatus_StateType_HARD);
        b->set_value_as_u32(6, hscr.check_attempt());
        b->set_value_as_bool(7, hscr.perfdata() != "");
        b->set_value_as_u32(8, hscr.check_type());
        if (hscr.last_check() == 0)
          b->set_null_u64(9);
        else
          b->set_value_as_u64(9, hscr.last_check());
        b->set_value_as_str(10, hscr.output());
        b->set_value_as_u64(11, hscr.host_id());
        b->next_row();
      } else {
        _hscr_resources_update->bind_value_as_i32(0, hscr.state());
        _hscr_resources_update->bind_value_as_i32(
            1, hst_ordered_status[hscr.state()]);
        _hscr_resources_update->bind_value_as_u64(2, hscr.last_state_change());
        _hscr_resources_update->bind_value_as_bool(
            3, hscr.scheduled_downtime_depth() > 0);
        _hscr_resources_update->bind_value_as_bool(
            4, hscr.acknowledgement_type() != HostStatus_AckType_NONE);
        _hscr_resources_update->bind_value_as_bool(
            5, hscr.state_type() == HostStatus_StateType_HARD);
        _hscr_resources_update->bind_value_as_u32(6, hscr.check_attempt());
        _hscr_resources_update->bind_value_as_bool(7, hscr.perfdata() != "");
        _hscr_resources_update->bind_value_as_u32(8, hscr.check_type());
        _hscr_resources_update->bind_value_as_u64(9, hscr.last_check(),
                                                  is_not_zero);
        _hscr_resources_update->bind_value_as_str(10, hscr.output());
        _hscr_resources_update->bind_value_as_u64(11, hscr.host_id());

        _mysql.run_statement(*_hscr_resources_update,
                             database::mysql_error::store_host_status, false,
                             conn);

        _add_action(conn, actions::resources);
      }
    }
  } else
    // Do nothing.
    log_v2::sql()->info(
        "SQL: not processing pb host status check result event (host: {}, "
        "check type: {}, last check: {}, next check: {}, now: {}, state ({}, "
        "{}))",
        hscr.host_id(), hscr.check_type(), hscr.last_check(), hscr.next_check(),
        now, hscr.state(), hscr.state_type());
}

/**
 *  Process an instance event. The thread executing the command is controlled
 *  so that queries depending on this one will be made by the same thread.
 *
 *  @param[in] e Uncasted instance.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_instance(const std::shared_ptr<io::data>& d) {
  neb::instance& i(*static_cast<neb::instance*>(d.get()));
  int32_t conn = _mysql.choose_connection_by_instance(i.poller_id);
  _finish_action(-1, actions::hosts | actions::acknowledgements |
                         actions::modules | actions::downtimes |
                         actions::comments | actions::servicegroups |
                         actions::hostgroups | actions::service_dependencies |
                         actions::host_dependencies);

  // Log message.
  log_v2::sql()->info(
      "SQL: processing poller event (id: {}, name: {}, running: {})",
      i.poller_id, i.name, i.is_running ? "yes" : "no");

  // Clean tables.
  _clean_tables(i.poller_id);

  // Processing.
  if (_is_valid_poller(i.poller_id)) {
    // Prepare queries.
    if (!_instance_insupdate.prepared()) {
      query_preparator::event_unique unique;
      unique.insert("instance_id");
      query_preparator qp(neb::instance::static_type(), unique);
      _instance_insupdate = qp.prepare_insert_or_update(_mysql);
    }

    // Process object.
    _instance_insupdate << i;
    _mysql.run_statement(_instance_insupdate,
                         database::mysql_error::store_poller, false, conn);
    _add_action(conn, actions::instances);
  }
}

/**
 *  Process an instance status event. To work on an instance status, we must
 *  be sure the instance already exists in the database. So this query must
 *  be done by the same thread as the one that created the instance.
 *
 *  @param[in] e Uncasted instance status.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_instance_status(const std::shared_ptr<io::data>& d) {
  neb::instance_status& is = *static_cast<neb::instance_status*>(d.get());
  int32_t conn = _mysql.choose_connection_by_instance(is.poller_id);

  _finish_action(-1, actions::hosts | actions::acknowledgements |
                         actions::modules | actions::downtimes |
                         actions::comments);

  // Log message.
  log_v2::sql()->info(
      "SQL: processing poller status event (id: {}, last alive: {})",
      is.poller_id, is.last_alive);

  // Processing.
  if (_is_valid_poller(is.poller_id)) {
    // Prepare queries.
    if (!_instance_status_insupdate.prepared()) {
      query_preparator::event_unique unique;
      unique.insert("instance_id");
      query_preparator qp(neb::instance_status::static_type(), unique);
      _instance_status_insupdate = qp.prepare_insert_or_update(_mysql);
    }

    // Process object.
    _instance_status_insupdate << is;
    _mysql.run_statement(_instance_status_insupdate,
                         database::mysql_error::update_poller, false, conn);
    _add_action(conn, actions::instances);
  }
}

/**
 *  Process a log event.
 *
 *  @param[in] e Uncasted log.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_log(const std::shared_ptr<io::data>& d) {
  // Fetch proper structure.
  neb::log_entry const& le(*static_cast<neb::log_entry const*>(d.get()));

  // Log message.
  log_v2::sql()->info(
      "SQL: processing log of poller '{}' generated at {} (type {})",
      le.poller_name, le.c_time, le.msg_type);

  // Push query.
  _logs.push_query(fmt::format(
      "({},{},{},'{}','{}',{},{},'{}','{}',{},'{}',{},'{}')", le.c_time,
      le.host_id, le.service_id,
      misc::string::escape(le.host_name, get_logs_col_size(logs_host_name)),
      misc::string::escape(le.poller_name,
                           get_logs_col_size(logs_instance_name)),
      le.log_type, le.msg_type,
      misc::string::escape(le.notification_cmd,
                           get_logs_col_size(logs_notification_cmd)),
      misc::string::escape(le.notification_contact,
                           get_logs_col_size(logs_notification_contact)),
      le.retry,
      misc::string::escape(le.service_description,
                           get_logs_col_size(logs_service_description)),
      le.status,
      misc::string::escape(le.output, get_logs_col_size(logs_output))));
}

/**
 *  Process a module event. We must take care of the thread id sending the
 *  query because the modules table has a constraint on instances.instance_id
 *
 *  @param[in] e Uncasted module.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_module(const std::shared_ptr<io::data>& d) {
  // Cast object.
  neb::module const& m = *static_cast<neb::module const*>(d.get());
  int32_t conn = _mysql.choose_connection_by_instance(m.poller_id);

  // Log message.
  log_v2::sql()->info(
      "SQL: processing module event (poller: {}, filename: {}, loaded: {})",
      m.poller_id, m.filename, m.loaded ? "yes" : "no");

  // Processing.
  if (_is_valid_poller(m.poller_id)) {
    // Prepare queries.
    if (!_module_insert.prepared()) {
      query_preparator qp(neb::module::static_type());
      _module_insert = qp.prepare_insert(_mysql);
    }

    // Process object.
    if (m.enabled) {
      _module_insert << m;
      _mysql.run_statement(_module_insert, database::mysql_error::store_module,
                           false, conn);
      _add_action(conn, actions::modules);
    } else {
      const std::string* ptr_filename;
      if (m.filename.size() > get_modules_col_size(modules_filename)) {
        std::string trunc_filename = m.filename;
        misc::string::truncate(trunc_filename,
                               get_modules_col_size(modules_filename));
        ptr_filename = &trunc_filename;
      } else
        ptr_filename = &m.filename;

      std::string query(fmt::format(
          "DELETE FROM modules WHERE instance_id={} AND filename='{}'",
          m.poller_id, *ptr_filename));
      _mysql.run_query(query, database::mysql_error::empty, false, conn);
      _add_action(conn, actions::modules);
    }
  }
}

/**
 *  Process a service check event.
 *
 *  @param[in] e Uncasted service check.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_service_check(const std::shared_ptr<io::data>& d) {
  _finish_action(-1, actions::downtimes | actions::comments |
                         actions::host_dependencies | actions::host_parents |
                         actions::service_dependencies);

  // Cast object.
  neb::service_check const& sc(
      *static_cast<neb::service_check const*>(d.get()));

  time_t now{time(nullptr)};
  if (sc.check_type                 // - passive result
      || !sc.active_checks_enabled  // - active checks are disabled,
                                    //   status might not be updated
                                    // - normal case
      || (sc.next_check >= now - 5 * 60) ||
      !sc.next_check) {  // - initial state
    // Apply to DB.
    log_v2::sql()->info(
        "SQL: processing service check event (host: {}, service: {}, command: "
        "{})",
        sc.host_id, sc.service_id, sc.command_line);

    // Prepare queries.
    if (!_service_check_update.prepared()) {
      query_preparator::event_unique unique;
      unique.insert("host_id");
      unique.insert("service_id");
      query_preparator qp(neb::service_check::static_type(), unique);
      _service_check_update = qp.prepare_update(_mysql);
    }

    // Processing.
    bool store = true;
    size_t str_hash = std::hash<std::string>{}(sc.command_line);
    // Did the command changed since last time?
    if (_cache_svc_cmd[{sc.host_id, sc.service_id}] != str_hash)
      _cache_svc_cmd[std::make_pair(sc.host_id, sc.service_id)] = str_hash;
    else
      store = false;

    if (store) {
      if (_cache_host_instance[sc.host_id]) {
        _service_check_update << sc;
        std::promise<int> promise;
        int32_t conn = _mysql.choose_connection_by_instance(
            _cache_host_instance[sc.host_id]);
        _mysql.run_statement(_service_check_update,
                             database::mysql_error::store_service_check_command,
                             false, conn);
      } else
        log_v2::sql()->error(
            "SQL: host with host_id = {} does not exist - unable to store "
            "service command check of that host. You should restart centengine",
            sc.host_id);
    }
  } else
    // Do nothing.
    log_v2::sql()->info(
        "SQL: not processing service check event (host: {}, service: {}, "
        "command: {}, check_type: {}, next_check: {}, now: {})",
        sc.host_id, sc.service_id, sc.command_line, sc.check_type,
        sc.next_check, now);
}

/**
 *  Process a service dependency event.
 *
 *  @param[in] e Uncasted service dependency.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_service_dependency(const std::shared_ptr<io::data>& d) {
  int32_t conn = special_conn::service_dependency % _mysql.connections_count();
  _finish_action(-1, actions::hosts | actions::host_parents |
                         actions::downtimes | actions::comments |
                         actions::host_dependencies |
                         actions::service_dependencies);

  // Cast object.
  neb::service_dependency const& sd(
      *static_cast<neb::service_dependency const*>(d.get()));

  // Insert/Update.
  if (sd.enabled) {
    log_v2::sql()->info(
        "SQL: enabling service dependency of ({}, {}) on ({}, {})",
        sd.dependent_host_id, sd.dependent_service_id, sd.host_id,
        sd.service_id);

    // Prepare queries.
    if (!_service_dependency_insupdate.prepared()) {
      query_preparator::event_unique unique;
      unique.insert("dependent_host_id");
      unique.insert("dependent_service_id");
      unique.insert("host_id");
      unique.insert("service_id");
      query_preparator qp(neb::service_dependency::static_type(), unique);
      _service_dependency_insupdate = qp.prepare_insert_or_update(_mysql);
    }

    // Process object.
    _service_dependency_insupdate << sd;
    _mysql.run_statement(_service_dependency_insupdate,
                         database::mysql_error::store_service_dependency, false,
                         conn);
    _add_action(conn, actions::service_dependencies);
  }
  // Delete.
  else {
    log_v2::sql()->info(
        "SQL: removing service dependency of ({}, {}) on ({}, {})",
        sd.dependent_host_id, sd.dependent_service_id, sd.host_id,
        sd.service_id);
    std::string query(fmt::format(
        "DELETE FROM serivces_services_dependencies WHERE dependent_host_id={} "
        "AND dependent_service_id={} AND host_id={} AND service_id={}",
        sd.dependent_host_id, sd.dependent_service_id, sd.host_id,
        sd.service_id));
    _mysql.run_query(query, database::mysql_error::empty, false, conn);
    _add_action(conn, actions::service_dependencies);
  }
}

/**
 *  Process a service group event.
 *
 *  @param[in] e Uncasted service group.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_service_group(const std::shared_ptr<io::data>& d) {
  int32_t conn = special_conn::service_group % _mysql.connections_count();

  // Cast object.
  const neb::service_group& sg{
      *static_cast<const neb::service_group*>(d.get())};

  // Insert/update group.
  if (sg.enabled) {
    log_v2::sql()->info("SQL: enabling service group {} ('{}' on instance {})",
                        sg.id, sg.name, sg.poller_id);
    _prepare_sg_insupdate_statement();

    _service_group_insupdate << sg;
    _mysql.run_statement(_service_group_insupdate,
                         database::mysql_error::store_service_group, false,
                         conn);
    _servicegroup_cache.insert(sg.id);
  }
  // Delete group.
  else {
    log_v2::sql()->info("SQL: disabling service group {} ('{}' on instance {})",
                        sg.id, sg.name, sg.poller_id);

    // Delete group members.
    {
      _finish_action(-1, actions::services);
      std::string query(fmt::format(
          "DELETE services_servicegroups FROM services_servicegroups LEFT "
          "JOIN hosts ON services_servicegroups.host_id=hosts.host_id WHERE "
          "services_servicegroups.servicegroup_id={} AND "
          "hosts.instance_id={}",
          sg.id, sg.poller_id));
      _mysql.run_query(query, database::mysql_error::empty, false, conn);
      _servicegroup_cache.erase(sg.id);
    }
  }
  _add_action(conn, actions::servicegroups);
}

/**
 *  Process a service group member event.
 *
 *  @param[in] e Uncasted service group member.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_service_group_member(const std::shared_ptr<io::data>& d) {
  int32_t conn = special_conn::service_group % _mysql.connections_count();
  _finish_action(-1, actions::services);

  // Cast object.
  const neb::service_group_member& sgm{
      *static_cast<const neb::service_group_member*>(d.get())};

  if (sgm.enabled) {
    // Log message.
    log_v2::sql()->info(
        "SQL: enabling membership of service ({}, {}) to service group {} on "
        "instance {}",
        sgm.host_id, sgm.service_id, sgm.group_id, sgm.poller_id);

    // We only need to try to insert in this table as the
    // host_id/service_id/servicegroup_id combo should be UNIQUE.
    if (!_service_group_member_insert.prepared()) {
      query_preparator::event_unique unique;
      unique.insert("servicegroup_id");
      unique.insert("host_id");
      unique.insert("service_id");
      query_preparator qp(neb::service_group_member::static_type(), unique);
      _service_group_member_insert = qp.prepare_insert(_mysql);
    }

    /* If the group does not exist, we create it. */
    if (_servicegroup_cache.find(sgm.group_id) == _servicegroup_cache.end()) {
      log_v2::sql()->error(
          "SQL: service group {} does not exist - insertion before insertion "
          "of members",
          sgm.group_id);
      _prepare_sg_insupdate_statement();

      neb::service_group sg;
      sg.id = sgm.group_id;
      sg.name = sgm.group_name;
      sg.enabled = true;
      sg.poller_id = sgm.poller_id;

      _service_group_insupdate << sg;
      _mysql.run_statement(_service_group_insupdate,
                           database::mysql_error::store_service_group, false,
                           conn);
      _servicegroup_cache.insert(sgm.group_id);
    }

    _service_group_member_insert << sgm;
    _mysql.run_statement(_service_group_member_insert,
                         database::mysql_error::store_service_group_member,
                         false, conn);
    _add_action(conn, actions::servicegroups);
  }
  // Delete.
  else {
    // Log message.
    log_v2::sql()->info(
        "SQL: disabling membership of service ({}, {}) to service group {} on "
        "instance {}",
        sgm.host_id, sgm.service_id, sgm.group_id, sgm.poller_id);

    if (!_service_group_member_delete.prepared()) {
      query_preparator::event_unique unique;
      unique.insert("servicegroup_id");
      unique.insert("host_id");
      unique.insert("service_id");
      query_preparator qp(neb::service_group_member::static_type(), unique);
      _service_group_member_delete = qp.prepare_delete(_mysql);
    }
    _service_group_member_delete << sgm;
    _mysql.run_statement(_service_group_member_delete,
                         database::mysql_error::delete_service_group_member,
                         false, conn);
    _add_action(conn, actions::servicegroups);
  }
}

/**
 *  Process a service event.
 *
 *  @param[in] e Uncasted service.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_service(const std::shared_ptr<io::data>& d) {
  _finish_action(-1, actions::host_parents | actions::comments |
                         actions::downtimes | actions::host_dependencies |
                         actions::service_dependencies);

  // Processed object.
  const neb::service& s(*static_cast<neb::service const*>(d.get()));
  if (_cache_host_instance[s.host_id]) {
    int32_t conn =
        _mysql.choose_connection_by_instance(_cache_host_instance[s.host_id]);

    // Log message.
    log_v2::sql()->info(
        "SQL: processing service event (host: {}, service: {}, "
        "description: {})",
        s.host_id, s.service_id, s.service_description);

    if (s.host_id && s.service_id) {
      // Prepare queries.
      if (!_service_insupdate.prepared()) {
        query_preparator::event_unique unique;
        unique.insert("host_id");
        unique.insert("service_id");
        query_preparator qp(neb::service::static_type(), unique);
        _service_insupdate = qp.prepare_insert_or_update(_mysql);
      }

      _service_insupdate << s;
      _mysql.run_statement(_service_insupdate,
                           database::mysql_error::store_service, false, conn);
      _add_action(conn, actions::services);
    } else
      log_v2::sql()->trace(
          "SQL: service '{}' has no host ID, service ID nor hostname, probably "
          "bam fake service",
          s.service_description);
  } else
    log_v2::sql()->error(
        "SQL: host with host_id = {} does not exist - unable to store service "
        "of that host. You should restart centengine",
        s.host_id);
}

/**
 *  Process a service event.
 *
 *  @param[in] e Uncasted service.
 *
 */
void stream::_process_pb_service(const std::shared_ptr<io::data>& d) {
  _finish_action(-1, actions::host_parents | actions::comments |
                         actions::downtimes | actions::host_dependencies |
                         actions::service_dependencies | actions::severities);
  // Processed object.
  auto svc{static_cast<neb::pb_service const*>(d.get())};
  auto& s = svc->obj();
  log_v2::sql()->debug("SQL: processing pb service ({}, {})", s.host_id(),
                       s.service_id());
  log_v2::sql()->trace("SQL: pb service output: <<{}>>", s.output());
  // Processed object.
  if (_cache_host_instance[s.host_id()]) {
    int32_t conn =
        _mysql.choose_connection_by_instance(_cache_host_instance[s.host_id()]);

    // Log message.
    log_v2::sql()->info(
        "SQL: processing pb service event (host: {}, service: {}, "
        "description: {})",
        s.host_id(), s.service_id(), s.description());

    if (s.host_id() && s.service_id()) {
      // Prepare queries.
      if (!_pb_service_insupdate.prepared()) {
        query_preparator::event_pb_unique unique{
            {1, "host_id", io::protobuf_base::invalid_on_zero, 0},
            {2, "service_id", io::protobuf_base::invalid_on_zero, 0},
        };
        query_preparator qp(neb::pb_service::static_type(), unique);

        _pb_service_insupdate = qp.prepare_insert_or_update_table(
            _mysql, "services",
            {
                {1, "host_id", io::protobuf_base::invalid_on_zero, 0},
                {2, "service_id", io::protobuf_base::invalid_on_zero, 0},
                {3, "acknowledged", 0, 0},
                {4, "acknowledgement_type", 0, 0},
                {5, "active_checks", 0, 0},
                {6, "enabled", 0, 0},
                {7, "scheduled_downtime_depth", 0, 0},
                {8, "check_command", 0,
                 get_services_col_size(services_check_command)},
                {9, "check_interval", 0, 0},
                {10, "check_period", 0,
                 get_services_col_size(services_check_period)},
                {11, "check_type", 0, 0},
                {12, "check_attempt", 0, 0},
                {13, "state", 0, 0},
                {14, "event_handler_enabled", 0, 0},
                {15, "event_handler", 0,
                 get_services_col_size(services_event_handler)},
                {16, "execution_time", 0, 0},
                {17, "flap_detection", 0, 0},
                {18, "checked", 0, 0},
                {19, "flapping", 0, 0},
                {20, "last_check", io::protobuf_base::invalid_on_zero, 0},
                {21, "last_hard_state", 0, 0},
                {22, "last_hard_state_change",
                 io::protobuf_base::invalid_on_zero, 0},
                {23, "last_notification", io::protobuf_base::invalid_on_zero,
                 0},
                {24, "notification_number", 0, 0},
                {25, "last_state_change", io::protobuf_base::invalid_on_zero,
                 0},
                {26, "last_time_ok", io::protobuf_base::invalid_on_zero, 0},
                {27, "last_time_warning", io::protobuf_base::invalid_on_zero,
                 0},
                {28, "last_time_critical", io::protobuf_base::invalid_on_zero,
                 0},
                {29, "last_time_unknown", io::protobuf_base::invalid_on_zero,
                 0},
                {30, "last_update", io::protobuf_base::invalid_on_zero, 0},
                {31, "latency", 0, 0},
                {32, "max_check_attempts", 0, 0},
                {33, "next_check", io::protobuf_base::invalid_on_zero, 0},
                {34, "next_notification", io::protobuf_base::invalid_on_zero,
                 0},
                {35, "no_more_notifications", 0, 0},
                {36, "notify", 0, 0},
                {37, "output", 0, get_services_col_size(services_output)},

                {39, "passive_checks", 0, 0},
                {40, "percent_state_change", 0, 0},
                {41, "perfdata", 0, get_services_col_size(services_perfdata)},
                {42, "retry_interval", 0, 0},

                {44, "description", 0,
                 get_services_col_size(services_description)},
                {45, "should_be_scheduled", 0, 0},
                {46, "obsess_over_service", 0, 0},
                {47, "state_type", 0, 0},
                {48, "action_url", 0,
                 get_services_col_size(services_action_url)},
                {49, "check_freshness", 0, 0},
                {50, "default_active_checks", 0, 0},
                {51, "default_event_handler_enabled", 0, 0},
                {52, "default_flap_detection", 0, 0},
                {53, "default_notify", 0, 0},
                {54, "default_passive_checks", 0, 0},
                {55, "display_name", 0,
                 get_services_col_size(services_display_name)},
                {56, "first_notification_delay", 0, 0},
                {57, "flap_detection_on_critical", 0, 0},
                {58, "flap_detection_on_ok", 0, 0},
                {59, "flap_detection_on_unknown", 0, 0},
                {60, "flap_detection_on_warning", 0, 0},
                {61, "freshness_threshold", 0, 0},
                {62, "high_flap_threshold", 0, 0},
                {63, "icon_image", 0,
                 get_services_col_size(services_icon_image)},
                {64, "icon_image_alt", 0,
                 get_services_col_size(services_icon_image_alt)},
                {65, "volatile", 0, 0},
                {66, "low_flap_threshold", 0, 0},
                {67, "notes", 0, get_services_col_size(services_notes)},
                {68, "notes_url", 0, get_services_col_size(services_notes_url)},
                {69, "notification_interval", 0, 0},
                {70, "notification_period", 0,
                 get_services_col_size(services_notification_period)},
                {71, "notify_on_critical", 0, 0},
                {72, "notify_on_downtime", 0, 0},
                {73, "notify_on_flapping", 0, 0},
                {74, "notify_on_recovery", 0, 0},
                {75, "notify_on_unknown", 0, 0},
                {76, "notify_on_warning", 0, 0},
                {77, "stalk_on_critical", 0, 0},
                {78, "stalk_on_ok", 0, 0},
                {79, "stalk_on_unknown", 0, 0},
                {80, "stalk_on_warning", 0, 0},
                {81, "retain_nonstatus_information", 0, 0},
                {82, "retain_status_information", 0, 0},
            });
        if (_store_in_resources) {
          _resources_service_insert = _mysql.prepare_query(
              "INSERT INTO resources "
              "(id,parent_id,type,internal_id,status,status_"
              "ordered,last_"
              "status_change,in_downtime,acknowledged,"
              "status_confirmed,check_attempts,max_check_attempts,poller_id,"
              "severity_id,name,parent_name,notes_url,notes,action_url,"
              "notifications_enabled,passive_checks_enabled,active_checks_"
              "enabled,enabled,icon_id) "
              "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,1,?)");
          _resources_service_update = _mysql.prepare_query(
              "UPDATE resources SET "
              "type=?,internal_id=?,status=?,status_ordered=?,last_status_"
              "change=?,"
              "in_downtime=?,acknowledged=?,"
              "status_confirmed=?,check_attempts=?,max_check_attempts=?,"
              "poller_id=?,severity_id=?,name=?,parent_name=?,notes_url=?,"
              "notes=?,action_url=?,notifications_enabled=?,"
              "passive_checks_enabled=?,active_checks_enabled=?,icon_id=?,"
              "enabled=1 WHERE resource_id=?");
          if (!_resources_disable.prepared()) {
            _resources_disable = _mysql.prepare_query(
                "UPDATE resources SET enabled=0 WHERE resource_id=?");
          }
        }
      }

      // Process object.
      _pb_service_insupdate << *svc;
      _mysql.run_statement(_pb_service_insupdate,
                           database::mysql_error::store_service, false, conn);
      _add_action(conn, actions::services);

      _check_and_update_index_cache(s);

      if (_store_in_resources) {
        uint64_t res_id = 0;
        auto found = _resource_cache.find({s.service_id(), s.host_id()});

        if (s.enabled()) {
          uint64_t sid = 0;
          fmt::string_view name{misc::string::truncate(
              s.display_name(), get_resources_col_size(resources_name))};
          fmt::string_view parent_name{misc::string::truncate(
              s.host_name(), get_resources_col_size(resources_parent_name))};
          fmt::string_view notes_url{misc::string::truncate(
              s.notes_url(), get_resources_col_size(resources_notes_url))};
          fmt::string_view notes{misc::string::truncate(
              s.notes(), get_resources_col_size(resources_notes))};
          fmt::string_view action_url{misc::string::truncate(
              s.action_url(), get_resources_col_size(resources_action_url))};

          // INSERT
          if (found == _resource_cache.end()) {
            _resources_service_insert.bind_value_as_u64(0, s.service_id());
            _resources_service_insert.bind_value_as_u64(1, s.host_id());
            _resources_service_insert.bind_value_as_u32(2, s.type());
            if (s.internal_id())
              _resources_service_insert.bind_value_as_u64(3, s.internal_id());
            else
              _resources_service_insert.bind_null_u64(3);
            _resources_service_insert.bind_value_as_u32(4, s.state());
            _resources_service_insert.bind_value_as_u32(
                5, svc_ordered_status[s.state()]);
            _resources_service_insert.bind_value_as_u64(6,
                                                        s.last_state_change());
            _resources_service_insert.bind_value_as_bool(
                7, s.scheduled_downtime_depth() > 0);
            _resources_service_insert.bind_value_as_bool(
                8, s.acknowledgement_type() != Service_AckType_NONE);
            _resources_service_insert.bind_value_as_bool(
                9, s.state_type() == Service_StateType_HARD);
            _resources_service_insert.bind_value_as_u32(10, s.check_attempt());
            _resources_service_insert.bind_value_as_u32(11,
                                                        s.max_check_attempts());
            _resources_service_insert.bind_value_as_u64(
                12, _cache_host_instance[s.host_id()]);
            if (s.severity_id() > 0) {
              sid = _severity_cache[{s.severity_id(), 0}];
              log_v2::sql()->debug(
                  "service ({}, {}) with severity_id {} => uid = {}",
                  s.host_id(), s.service_id(), s.severity_id(), sid);
            }
            if (sid)
              _resources_service_insert.bind_value_as_u64(13, sid);
            else
              _resources_service_insert.bind_null_u64(13);
            _resources_service_insert.bind_value_as_str(14, name);
            _resources_service_insert.bind_value_as_str(15, parent_name);
            _resources_service_insert.bind_value_as_str(16, notes_url);
            _resources_service_insert.bind_value_as_str(17, notes);
            _resources_service_insert.bind_value_as_str(18, action_url);
            _resources_service_insert.bind_value_as_bool(19, s.notify());
            _resources_service_insert.bind_value_as_bool(20,
                                                         s.passive_checks());
            _resources_service_insert.bind_value_as_bool(21, s.active_checks());
            _resources_service_insert.bind_value_as_u64(22, s.icon_id());

            std::promise<uint64_t> p;
            std::future<uint64_t> future = p.get_future();
            _mysql.run_statement_and_get_int<uint64_t>(
                _resources_service_insert, std::move(p),
                database::mysql_task::LAST_INSERT_ID, conn);
            _add_action(conn, actions::resources);
            try {
              res_id = future.get();
              _resource_cache.insert({{s.service_id(), s.host_id()}, res_id});
            } catch (const std::exception& e) {
              log_v2::sql()->critical(
                  "SQL: unable to insert new service resource ({}, {}): {}",
                  s.host_id(), s.service_id(), e.what());

              std::promise<mysql_result> promise_resource;
              std::future<mysql_result> future_resource =
                  promise_resource.get_future();
              _mysql.run_query_and_get_result(
                  fmt::format("SELECT resource_id FROM resources WHERE "
                              "parent_id={} AND id={}",
                              s.host_id(), s.service_id()),
                  std::move(promise_resource));
              try {
                mysql_result res{future_resource.get()};
                if (_mysql.fetch_row(res)) {
                  auto r = _resource_cache.insert(
                      {{s.service_id(), s.host_id()}, res.value_as_u64(0)});
                  found = r.first;
                  log_v2::sql()->debug(
                      "Service resource ({}, {}) found in database with id {}",
                      s.host_id(), s.service_id(), found->second);
                }
              } catch (const std::exception& e) {
                log_v2::sql()->critical(
                    "No service resource in database with id ({}, {}): {}",
                    s.host_id(), s.service_id(), e.what());
                return;
              }
            }
          }
          if (res_id == 0) {
            res_id = found->second;
            // UPDATE
            _resources_service_update.bind_value_as_u32(0, s.type());
            if (s.internal_id())
              _resources_service_update.bind_value_as_u64(1, s.internal_id());
            else
              _resources_service_update.bind_null_u64(1);
            _resources_service_update.bind_value_as_u32(2, s.state());
            _resources_service_update.bind_value_as_u32(
                3, svc_ordered_status[s.state()]);
            _resources_service_update.bind_value_as_u64(4,
                                                        s.last_state_change());
            _resources_service_update.bind_value_as_bool(
                5, s.scheduled_downtime_depth() > 0);
            _resources_service_update.bind_value_as_bool(
                6, s.acknowledgement_type() != Service_AckType_NONE);
            _resources_service_update.bind_value_as_bool(
                7, s.state_type() == Service_StateType_HARD);
            _resources_service_update.bind_value_as_u32(8, s.check_attempt());
            _resources_service_update.bind_value_as_u32(9,
                                                        s.max_check_attempts());
            _resources_service_update.bind_value_as_u64(
                10, _cache_host_instance[s.host_id()]);
            if (s.severity_id() > 0) {
              sid = _severity_cache[{s.severity_id(), 0}];
              log_v2::sql()->debug(
                  "service ({}, {}) with severity_id {} => uid = {}",
                  s.host_id(), s.service_id(), s.severity_id(), sid);
            }
            if (sid)
              _resources_service_update.bind_value_as_u64(11, sid);
            else
              _resources_service_update.bind_null_u64(11);
            _resources_service_update.bind_value_as_str(12, name);
            _resources_service_update.bind_value_as_str(13, parent_name);
            _resources_service_update.bind_value_as_str(14, notes_url);
            _resources_service_update.bind_value_as_str(15, notes);
            _resources_service_update.bind_value_as_str(16, action_url);
            _resources_service_update.bind_value_as_bool(17, s.notify());
            _resources_service_update.bind_value_as_bool(18,
                                                         s.passive_checks());
            _resources_service_update.bind_value_as_bool(19, s.active_checks());
            _resources_service_update.bind_value_as_u64(20, s.icon_id());
            _resources_service_update.bind_value_as_u64(21, res_id);

            _mysql.run_statement(_resources_service_update,
                                 database::mysql_error::store_service, false,
                                 conn);
            _add_action(conn, actions::resources);
          }

          if (!_resources_tags_insert.prepared()) {
            _resources_tags_insert = _mysql.prepare_query(
                "INSERT INTO resources_tags (tag_id,resource_id) VALUES(?,?)");
          }
          if (!_resources_tags_remove.prepared())
            _resources_tags_remove = _mysql.prepare_query(
                "DELETE FROM resources_tags WHERE resource_id=?");
          _finish_action(-1, actions::tags);
          _resources_tags_remove.bind_value_as_u64(0, res_id);
          _mysql.run_statement(_resources_tags_remove,
                               database::mysql_error::delete_resources_tags,
                               false, conn);
          for (auto& tag : s.tags()) {
            auto it_tags_cache = _tags_cache.find({tag.id(), tag.type()});

            if (it_tags_cache == _tags_cache.end()) {
              log_v2::sql()->error(
                  "SQL: could not find in cache the tag ({}, {}) for service "
                  "({},{}): trying to add it.",
                  tag.id(), tag.type(), s.host_id(), s.service_id());
              if (!_tag_insert.prepared())
                _tag_insert = _mysql.prepare_query(
                    "INSERT INTO tags (id,type,name) VALUES(?,?,?)");
              _tag_insert.bind_value_as_u64(0, tag.id());
              _tag_insert.bind_value_as_u32(1, tag.type());
              _tag_insert.bind_value_as_str(2, "(unknown)");
              std::promise<uint64_t> p;
              std::future<uint64_t> future = p.get_future();
              _mysql.run_statement_and_get_int<uint64_t>(
                  _tag_insert, std::move(p),
                  database::mysql_task::LAST_INSERT_ID, conn);
              try {
                uint64_t tag_id = future.get();
                it_tags_cache =
                    _tags_cache.insert({{tag.id(), tag.type()}, tag_id}).first;
              } catch (const std::exception& e) {
                log_v2::sql()->error(
                    "SQL: unable to insert new tag ({},{}): {}", tag.id(),
                    tag.type(), e.what());
              }
            }

            if (it_tags_cache != _tags_cache.end()) {
              _resources_tags_insert.bind_value_as_u64(0,
                                                       it_tags_cache->second);
              _resources_tags_insert.bind_value_as_u64(1, res_id);
              log_v2::sql()->debug(
                  "SQL: new relation between service (resource_id: {},  ({}, "
                  "{})) and tag ({},{})",
                  res_id, s.host_id(), s.service_id(), tag.id(), tag.type());
              _mysql.run_statement(
                  _resources_tags_insert,
                  database::mysql_error::store_tags_resources_tags, false,
                  conn);
              _add_action(conn, actions::resources_tags);
            } else {
              log_v2::sql()->error(
                  "SQL: could not find the tag ({}, {}) in cache for host '{}'",
                  tag.id(), tag.type(), s.service_id());
            }
          }
        } else {
          if (found != _resource_cache.end()) {
            _resources_disable.bind_value_as_u64(0, found->second);

            _mysql.run_statement(_resources_disable,
                                 database::mysql_error::clean_resources, false,
                                 conn);
            _resource_cache.erase(found);
            _add_action(conn, actions::resources);
          } else {
            log_v2::sql()->info(
                "SQL: no need to remove service ({}, {}), it is not in "
                "database",
                s.host_id(), s.service_id());
          }
        }
      }
    } else
      log_v2::sql()->trace(
          "SQL: service '{}' has no host ID, service ID nor hostname, probably "
          "bam fake service",
          s.description());
  } else
    log_v2::sql()->error(
        "SQL: host with host_id = {} does not exist - unable to store service "
        "of that host. You should restart centengine",
        s.host_id());
}

/**
 *  Process an adaptive service event.
 *
 *  @param[in] e Uncasted service.
 *
 */
void stream::_process_pb_adaptive_service(const std::shared_ptr<io::data>& d) {
  log_v2::sql()->debug("SQL: processing pb adaptive service");
  _finish_action(-1, actions::host_parents | actions::comments |
                         actions::downtimes | actions::host_dependencies |
                         actions::service_dependencies);
  // Processed object.
  auto s{static_cast<const neb::pb_adaptive_service*>(d.get())};
  auto& as = s->obj();
  if (_cache_host_instance[as.host_id()]) {
    int32_t conn = _mysql.choose_connection_by_instance(
        _cache_host_instance[static_cast<uint32_t>(as.host_id())]);

    constexpr const char* buf = "UPDATE services SET";
    constexpr size_t size = strlen(buf);
    std::string query{buf};
    if (as.has_notify())
      query += fmt::format(" notify='{}',", as.notify() ? 1 : 0);
    if (as.has_active_checks())
      query += fmt::format(" active_checks='{}',", as.active_checks() ? 1 : 0);
    if (as.has_should_be_scheduled())
      query += fmt::format(" should_be_scheduled='{}',",
                           as.should_be_scheduled() ? 1 : 0);
    if (as.has_passive_checks())
      query +=
          fmt::format(" passive_checks='{}',", as.passive_checks() ? 1 : 0);
    if (as.has_event_handler_enabled())
      query += fmt::format(" event_handler_enabled='{}',",
                           as.event_handler_enabled() ? 1 : 0);
    if (as.has_flap_detection_enabled())
      query += fmt::format(" flap_detection='{}',",
                           as.flap_detection_enabled() ? 1 : 0);
    if (as.has_obsess_over_service())
      query += fmt::format(" obsess_over_service='{}',",
                           as.obsess_over_service() ? 1 : 0);
    if (as.has_event_handler())
      query += fmt::format(
          " event_handler='{}',",
          misc::string::escape(as.event_handler(),
                               get_services_col_size(services_event_handler)));
    if (as.has_check_command())
      query += fmt::format(
          " check_command='{}',",
          misc::string::escape(as.check_command(),
                               get_services_col_size(services_check_command)));
    if (as.has_check_interval())
      query += fmt::format(" check_interval={},", as.check_interval());
    if (as.has_retry_interval())
      query += fmt::format(" retry_interval={},", as.retry_interval());
    if (as.has_max_check_attempts())
      query += fmt::format(" max_check_attempts={},", as.max_check_attempts());
    if (as.has_check_freshness())
      query +=
          fmt::format(" check_freshness='{}',", as.check_freshness() ? 1 : 0);
    if (as.has_check_period())
      query += fmt::format(
          " check_period='{}',",
          misc::string::escape(as.check_period(),
                               get_services_col_size(services_check_period)));
    if (as.has_notification_period())
      query +=
          fmt::format(" notification_period='{}',",
                      misc::string::escape(
                          as.notification_period(),
                          get_services_col_size(services_notification_period)));

    // If nothing was added to query, we can exit immediately.
    if (query.size() > size) {
      query.resize(query.size() - 1);
      query += fmt::format(" WHERE host_id={} AND service_id={}", as.host_id(),
                           as.service_id());
      log_v2::sql()->trace("SQL: query <<{}>>", query);
      _mysql.run_query(query, database::mysql_error::store_service, false,
                       conn);
      _add_action(conn, actions::services);

      if (_store_in_resources) {
        constexpr const char* res_buf = "UPDATE resources SET";
        constexpr size_t res_size = strlen(res_buf);
        std::string res_query{res_buf};
        if (as.has_notify())
          res_query +=
              fmt::format(" notifications_enabled='{}',", as.notify() ? 1 : 0);
        if (as.has_active_checks())
          res_query += fmt::format(" active_checks_enabled='{}',",
                                   as.active_checks() ? 1 : 0);
        if (as.has_passive_checks())
          res_query += fmt::format(" passive_checks_enabled='{}',",
                                   as.passive_checks() ? 1 : 0);
        if (as.has_max_check_attempts())
          res_query +=
              fmt::format(" max_check_attempts={},", as.max_check_attempts());

        if (res_query.size() > res_size) {
          res_query.resize(res_query.size() - 1);
          res_query += fmt::format(" WHERE parent_id={} AND id={}",
                                   as.host_id(), as.service_id());
          log_v2::sql()->trace("SQL: query <<{}>>", res_query);
          _mysql.run_query(res_query, database::mysql_error::update_resources,
                           false, conn);
          _add_action(conn, actions::resources);
        }
      }
    }
  } else
    log_v2::sql()->error(
        "SQL: host with host_id = {} does not exist - unable to store service "
        "of that host. You should restart centengine",
        as.host_id());
}

/**
 * @brief Check if the index cache contains informations about the given
 * service. If these informations changed or do not exist, they are inserted
 * into the cache.
 *
 * @param ss A neb::pb_service.
 */
void stream::_check_and_update_index_cache(const Service& ss) {
  auto it_index_cache = _index_cache.find({ss.host_id(), ss.service_id()});

  fmt::string_view hv(misc::string::truncate(
      ss.host_name(), get_index_data_col_size(index_data_host_name)));
  fmt::string_view sv(misc::string::truncate(
      ss.description(),
      get_index_data_col_size(index_data_service_description)));
  bool special = ss.type() == BA;

  int32_t conn =
      _mysql.choose_connection_by_instance(_cache_host_instance[ss.host_id()]);

  // Not found
  if (it_index_cache == _index_cache.end()) {
    log_v2::sql()->debug("sql: index not found in cache for service ({}, {})",
                         ss.host_id(), ss.service_id());

    if (!_index_data_insert.prepared())
      _index_data_insert = _mysql.prepare_query(
          "INSERT INTO index_data "
          "(host_id,host_name,service_id,service_description,must_be_rebuild,"
          "special) VALUES (?,?,?,?,?,?)");

    uint64_t index_id = 0;

    _index_data_insert.bind_value_as_i32(0, ss.host_id());
    _index_data_insert.bind_value_as_str(1, hv);
    _index_data_insert.bind_value_as_i32(2, ss.service_id());
    _index_data_insert.bind_value_as_str(3, sv);
    _index_data_insert.bind_value_as_str(4, "0");
    _index_data_insert.bind_value_as_str(5, special ? "1" : "0");

    std::promise<uint64_t> p;
    std::future<uint64_t> future = p.get_future();
    _mysql.run_statement_and_get_int<uint64_t>(
        _index_data_insert, std::move(p), database::mysql_task::LAST_INSERT_ID,
        conn);
    try {
      index_id = future.get();
      log_v2::sql()->debug(
          "sql: new index {} added for service ({}, {}), special {}", index_id,
          ss.host_id(), ss.service_id(), special ? "1" : "0");
      index_info info{
          .index_id = index_id,
          .host_name = ss.host_name(),
          .service_description = ss.description(),
          .rrd_retention = _rrd_len,
          .interval = ss.check_interval(),
          .special = special,
          .locked = false,
      };
      log_v2::sql()->debug("sql: loaded index {} of ({}, {}) with rrd_len={}",
                           index_id, ss.host_id(), ss.service_id(),
                           info.rrd_retention);
      _index_cache[{ss.host_id(), ss.service_id()}] = std::move(info);
      // Create the metric mapping.
      auto im{std::make_shared<storage::index_mapping>(
          info.index_id, ss.host_id(), ss.service_id())};
      multiplexing::publisher pblshr;
      pblshr.write(im);
    } catch (const std::exception& e) {
      log_v2::sql()->debug(
          "sql: cannot insert new index for service ({}, {}): {}", ss.host_id(),
          ss.service_id(), e.what());
      if (!_index_data_query.prepared())
        _index_data_query = _mysql.prepare_query(
            "SELECT "
            "id,host_name,service_description,rrd_retention,check_interval,"
            "special,locked from index_data WHERE host_id=? AND service_id=?");

      _index_data_query.bind_value_as_i32(0, ss.host_id());
      _index_data_query.bind_value_as_i32(1, ss.service_id());
      std::promise<database::mysql_result> pq;
      std::future<database::mysql_result> future_pq = pq.get_future();
      log_v2::sql()->debug(
          "Attempt to get the index from the database for service ({}, {})",
          ss.host_id(), ss.service_id());

      _mysql.run_statement_and_get_result(_index_data_query, std::move(pq),
                                          conn);

      try {
        database::mysql_result res(future_pq.get());
        if (_mysql.fetch_row(res)) {
          index_id = res.value_as_u64(0);
          index_info info{
              .index_id = index_id,
              .host_name = res.value_as_str(1),
              .service_description = res.value_as_str(2),
              .rrd_retention =
                  res.value_as_u32(3) ? res.value_as_u32(3) : _rrd_len,
              .interval = res.value_as_u32(4) ? res.value_as_u32(4) : 5,
              .special = res.value_as_str(5) == "1",
              .locked = res.value_as_str(6) == "1",
          };
          log_v2::sql()->debug(
              "sql: loaded index {} of ({}, {}) with rrd_len={}, special={}, "
              "locked={}",
              index_id, ss.host_id(), ss.service_id(), info.rrd_retention,
              info.special, info.locked);
          _index_cache[{ss.host_id(), ss.service_id()}] = std::move(info);
          // Create the metric mapping.
          auto im{std::make_shared<storage::index_mapping>(
              info.index_id, ss.host_id(), ss.service_id())};
          multiplexing::publisher pblshr;
          pblshr.write(im);
        }
      } catch (const std::exception& e) {
      }
      if (index_id == 0)
        throw exceptions::msg_fmt(
            "Could not fetch index id of service ({}, {}): {}", ss.host_id(),
            ss.service_id(), e.what());
    }

  } else {
    uint64_t index_id = it_index_cache->second.index_id;

    if (it_index_cache->second.host_name != hv ||
        it_index_cache->second.service_description != sv ||
        it_index_cache->second.interval != ss.check_interval()) {
      if (!_index_data_update.prepared())
        _index_data_update = _mysql.prepare_query(
            "UPDATE index_data "
            "SET host_name=?, service_description=?, must_be_rebuild=?, "
            "special=?, check_interval=? "
            "WHERE id=?");

      _index_data_update.bind_value_as_str(0, hv);
      _index_data_update.bind_value_as_str(1, sv);
      _index_data_update.bind_value_as_str(2, "0");
      _index_data_update.bind_value_as_str(3, special ? "1" : "0");
      _index_data_update.bind_value_as_u32(4, ss.check_interval());
      _index_data_update.bind_value_as_u64(5, index_id);
      _mysql.run_statement(_index_data_update, mysql_error::update_index_data,
                           conn);
      it_index_cache->second.host_name = fmt::to_string(hv);
      it_index_cache->second.service_description = fmt::to_string(sv);
      it_index_cache->second.interval = ss.check_interval();
      log_v2::sql()->debug(
          "Updating index_data for host_id={} and service_id={}: host_name={}, "
          "service_description={}, check_interval={}",
          ss.host_id(), ss.service_id(), it_index_cache->second.host_name,
          it_index_cache->second.service_description,
          it_index_cache->second.interval);
    }
  }
}

/**
 *  Process a service status event.
 *
 *  @param[in] e Uncasted service status.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_service_status(const std::shared_ptr<io::data>& d) {
  if (!_store_in_hosts_services)
    return;

  _finish_action(-1, actions::host_parents | actions::comments |
                         actions::downtimes | actions::host_dependencies |
                         actions::service_dependencies);
  // Processed object.
  neb::service_status const& ss{
      *static_cast<neb::service_status const*>(d.get())};

  log_v2::perfdata()->info("SQL: service status output: <<{}>>", ss.output);
  log_v2::perfdata()->info("SQL: service status perfdata: <<{}>>",
                           ss.perf_data);

  time_t now = time(nullptr);
  if (ss.check_type ||           // - passive result
      !ss.active_checks_enabled  // - active checks are disabled,
                                 //   status might not be updated
      ||                         // - normal case
      ss.next_check >= now - 5 * 60 || !ss.next_check) {  // - initial state
    // Apply to DB.
    log_v2::sql()->info(
        "SQL: processing service status event (host: {}, service: {}, last "
        "check: {}, state ({}, {}))",
        ss.host_id, ss.service_id, ss.last_check, ss.current_state,
        ss.state_type);

    // Prepare queries.
    if (!_service_status_update.prepared()) {
      query_preparator::event_unique unique;
      unique.insert("host_id");
      unique.insert("service_id");
      query_preparator qp(neb::service_status::static_type(), unique);
      _service_status_update = qp.prepare_update(_mysql);
    }

    // Processing.
    _service_status_update << ss;
    int32_t conn =
        _mysql.choose_connection_by_instance(_cache_host_instance[ss.host_id]);
    _mysql.run_statement(_service_status_update,
                         database::mysql_error::store_service_status, false,
                         conn);
    _add_action(conn, actions::hosts);
  } else
    // Do nothing.
    log_v2::sql()->info(
        "SQL: not processing service status event (host: {}, service: {}, "
        "check type: {}, last check: {}, next check: {}, now: {}, state ({}, "
        "{}))",
        ss.host_id, ss.service_id, ss.check_type, ss.last_check, ss.next_check,
        now, ss.current_state, ss.state_type);

  /* perfdata part */
  _unified_sql_process_service_status(d);
}

/**
 *  Process a service status event.
 *
 *  @param[in] e Uncasted service status.
 *
 */
void stream::_process_pb_service_status(const std::shared_ptr<io::data>& d) {
  _finish_action(-1, actions::host_parents | actions::comments |
                         actions::downtimes | actions::host_dependencies |
                         actions::service_dependencies);
  // Processed object.
  auto s{static_cast<const neb::pb_service_status*>(d.get())};
  auto& sscr = s->obj();

  log_v2::sql()->debug(
      "SQL: pb service ({}, {}) status check result output: <<{}>>",
      sscr.host_id(), sscr.service_id(), sscr.output());
  log_v2::sql()->debug(
      "SQL: service ({}, {}) status check result perfdata: <<{}>>",
      sscr.host_id(), sscr.service_id(), sscr.perfdata());

  time_t now = time(nullptr);
  if (sscr.check_type() == ServiceStatus_CheckType_PASSIVE ||
      sscr.next_check() >= now - 5 * 60 ||  // usual case
      sscr.next_check() == 0) {             // initial state
    // Apply to DB.
    log_v2::sql()->info(
        "SQL: processing service status check result event proto (host: {}, "
        "service: {}, "
        "last check: {}, state ({}, {}))",
        sscr.host_id(), sscr.service_id(), sscr.last_check(), sscr.state(),
        sscr.state_type());

    // Processing.
    if (_store_in_hosts_services) {
      int32_t conn = _mysql.choose_connection_by_instance(
          _cache_host_instance[static_cast<uint32_t>(sscr.host_id())]);
      if (_bulk_prepared_statement) {
        if (!_sscr_bind->bind(conn))
          _sscr_bind->init_from_stmt(conn);
        auto* b = _sscr_bind->bind(conn).get();
        b->set_value_as_bool(0, sscr.checked());
        b->set_value_as_i32(1, sscr.check_type());
        b->set_value_as_i32(2, sscr.state());
        b->set_value_as_i32(3, sscr.state_type());
        b->set_value_as_i64(4, sscr.last_state_change());
        b->set_value_as_i32(5, sscr.last_hard_state());
        b->set_value_as_i64(6, sscr.last_hard_state_change());
        b->set_value_as_i64(7, sscr.last_time_ok());
        b->set_value_as_i64(8, sscr.last_time_warning());
        b->set_value_as_i64(9, sscr.last_time_critical());
        b->set_value_as_i64(10, sscr.last_time_unknown());
        std::string full_output{
            fmt::format("{}\n{}", sscr.output(), sscr.long_output())};
        size_t size = misc::string::adjust_size_utf8(
            full_output, get_services_col_size(services_output));
        b->set_value_as_str(11, fmt::string_view(full_output.data(), size));
        size = misc::string::adjust_size_utf8(
            sscr.perfdata(), get_services_col_size(services_perfdata));
        b->set_value_as_str(12, fmt::string_view(sscr.perfdata().data(), size));
        b->set_value_as_bool(13, sscr.flapping());
        b->set_value_as_f64(14, sscr.percent_state_change());
        b->set_value_as_f64(15, sscr.latency());
        b->set_value_as_f64(16, sscr.execution_time());
        if (sscr.last_check() == 0)
          b->set_null_i64(17);
        else
          b->set_value_as_i64(17, sscr.last_check());
        b->set_value_as_i64(18, sscr.next_check());
        b->set_value_as_bool(19, sscr.should_be_scheduled());
        b->set_value_as_i32(20, sscr.check_attempt());
        b->set_value_as_i32(21, sscr.notification_number());
        b->set_value_as_bool(22, sscr.no_more_notifications());
        b->set_value_as_i64(23, sscr.last_notification());
        b->set_value_as_i64(24, sscr.next_notification());
        b->set_value_as_bool(
            25, sscr.acknowledgement_type() != ServiceStatus_AckType_NONE);
        b->set_value_as_i32(26, sscr.acknowledgement_type());
        b->set_value_as_i32(27, sscr.scheduled_downtime_depth());
        b->set_value_as_i32(28, sscr.host_id());
        b->set_value_as_i32(29, sscr.service_id());
        b->next_row();
        log_v2::sql()->trace(
            "{} waiting updates for service status in services",
            b->current_row());
      } else {
        _sscr_update->bind_value_as_bool(0, sscr.checked());
        _sscr_update->bind_value_as_i32(1, sscr.check_type());
        _sscr_update->bind_value_as_i32(2, sscr.state());
        _sscr_update->bind_value_as_i32(3, sscr.state_type());
        _sscr_update->bind_value_as_i64(4, sscr.last_state_change());
        _sscr_update->bind_value_as_i32(5, sscr.last_hard_state());
        _sscr_update->bind_value_as_i64(6, sscr.last_hard_state_change());
        _sscr_update->bind_value_as_i64(7, sscr.last_time_ok());
        _sscr_update->bind_value_as_i64(8, sscr.last_time_warning());
        _sscr_update->bind_value_as_i64(9, sscr.last_time_critical());
        _sscr_update->bind_value_as_i64(10, sscr.last_time_unknown());
        std::string full_output{
            fmt::format("{}\n{}", sscr.output(), sscr.long_output())};
        size_t size = misc::string::adjust_size_utf8(
            full_output, get_services_col_size(services_output));
        _sscr_update->bind_value_as_str(
            11, fmt::string_view(full_output.data(), size));
        size = misc::string::adjust_size_utf8(
            sscr.perfdata(), get_services_col_size(services_perfdata));
        _sscr_update->bind_value_as_str(
            12, fmt::string_view(sscr.perfdata().data(), size));
        _sscr_update->bind_value_as_bool(13, sscr.flapping());
        _sscr_update->bind_value_as_f64(14, sscr.percent_state_change());
        _sscr_update->bind_value_as_f64(15, sscr.latency());
        _sscr_update->bind_value_as_f64(16, sscr.execution_time());
        _sscr_update->bind_value_as_i64(17, sscr.last_check(), is_not_zero);
        _sscr_update->bind_value_as_i64(18, sscr.next_check());
        _sscr_update->bind_value_as_bool(19, sscr.should_be_scheduled());
        _sscr_update->bind_value_as_i32(20, sscr.check_attempt());
        _sscr_update->bind_value_as_u64(21, sscr.notification_number());
        _sscr_update->bind_value_as_bool(22, sscr.no_more_notifications());
        _sscr_update->bind_value_as_i64(23, sscr.last_notification());
        _sscr_update->bind_value_as_i64(24, sscr.next_notification());
        _sscr_update->bind_value_as_bool(
            25, sscr.acknowledgement_type() != ServiceStatus_AckType_NONE);
        _sscr_update->bind_value_as_i32(26, sscr.acknowledgement_type());
        _sscr_update->bind_value_as_i32(27, sscr.scheduled_downtime_depth());
        _sscr_update->bind_value_as_i32(28, sscr.host_id());
        _sscr_update->bind_value_as_i32(29, sscr.service_id());

        _mysql.run_statement(*_sscr_update,
                             database::mysql_error::store_service_status, false,
                             conn);

        _add_action(conn, actions::services);
      }
    }

    if (_store_in_resources) {
      int32_t conn = _mysql.choose_connection_by_instance(
          _cache_host_instance[static_cast<uint32_t>(sscr.host_id())]);
      if (_bulk_prepared_statement) {
        if (!_sscr_resources_bind->bind(conn))
          _sscr_resources_bind->init_from_stmt(conn);
        auto* b = _sscr_resources_bind->bind(conn).get();
        b->set_value_as_i32(0, sscr.state());
        b->set_value_as_i32(1, svc_ordered_status[sscr.state()]);
        b->set_value_as_u64(2, sscr.last_state_change());
        b->set_value_as_bool(3, sscr.scheduled_downtime_depth() > 0);
        b->set_value_as_bool(
            4, sscr.acknowledgement_type() != ServiceStatus_AckType_NONE);
        b->set_value_as_bool(5,
                             sscr.state_type() == ServiceStatus_StateType_HARD);
        b->set_value_as_u32(6, sscr.check_attempt());
        b->set_value_as_bool(7, sscr.perfdata() != "");
        b->set_value_as_u32(8, sscr.check_type());
        if (sscr.last_check() == 0)
          b->set_null_u64(9);
        else
          b->set_value_as_u64(9, sscr.last_check());
        b->set_value_as_str(10, sscr.output());
        b->set_value_as_u64(11, sscr.service_id());
        b->set_value_as_u64(12, sscr.host_id());
        b->next_row();
        log_v2::sql()->trace(
            "{} waiting updates for service status in resources",
            b->current_row());
      } else {
        _sscr_resources_update->bind_value_as_i32(0, sscr.state());
        _sscr_resources_update->bind_value_as_i32(
            1, svc_ordered_status[sscr.state()]);
        _sscr_resources_update->bind_value_as_u64(2, sscr.last_state_change());
        _sscr_resources_update->bind_value_as_bool(
            3, sscr.scheduled_downtime_depth() > 0);
        _sscr_resources_update->bind_value_as_bool(
            4, sscr.acknowledgement_type() != ServiceStatus_AckType_NONE);
        _sscr_resources_update->bind_value_as_bool(
            5, sscr.state_type() == ServiceStatus_StateType_HARD);
        _sscr_resources_update->bind_value_as_u32(6, sscr.check_attempt());
        _sscr_resources_update->bind_value_as_bool(7, sscr.perfdata() != "");
        _sscr_resources_update->bind_value_as_u32(8, sscr.check_type());
        _sscr_resources_update->bind_value_as_u64(9, sscr.last_check(),
                                                  is_not_zero);
        _sscr_resources_update->bind_value_as_str(10, sscr.output());
        _sscr_resources_update->bind_value_as_u64(11, sscr.service_id());
        _sscr_resources_update->bind_value_as_u64(12, sscr.host_id());

        _mysql.run_statement(*_sscr_resources_update,
                             database::mysql_error::store_service_status, false,
                             conn);
        _add_action(conn, actions::resources);
      }
    }
  } else
    // Do nothing.
    log_v2::sql()->info(
        "SQL: not processing service status check result event (host: {}, "
        "service: {}, "
        "check type: {}, last check: {}, next check: {}, now: {}, state ({}, "
        "{}))",
        sscr.host_id(), sscr.service_id(), sscr.check_type(), sscr.last_check(),
        sscr.next_check(), now, sscr.state(), sscr.state_type());

  /* perfdata part */
  _unified_sql_process_pb_service_status(d);
}

void stream::_process_severity(const std::shared_ptr<io::data>& d) {
  if (!_store_in_resources)
    return;

  log_v2::sql()->debug("SQL: processing severity");
  _finish_action(-1, actions::resources);

  // Prepare queries.
  if (!_severity_insert.prepared()) {
    _severity_update = _mysql.prepare_query(
        "UPDATE severities SET id=?,type=?,name=?,level=?,icon_id=? WHERE "
        "severity_id=?");
    _severity_insert = _mysql.prepare_query(
        "INSERT INTO severities (id,type,name,level,icon_id) "
        "VALUES(?,?,?,?,?)");
  }
  // Processed object.
  auto s{static_cast<const neb::pb_severity*>(d.get())};
  auto& sv = s->obj();
  log_v2::sql()->trace(
      "SQL: severity event with id={}, type={}, name={}, level={}, icon_id={}",
      sv.id(), sv.type(), sv.name(), sv.level(), sv.icon_id());
  uint64_t severity_id = _severity_cache[{sv.id(), sv.type()}];
  int32_t conn = special_conn::severity % _mysql.connections_count();
  switch (sv.action()) {
    case Severity_Action_ADD:
      _add_action(conn, actions::severities);
      if (severity_id) {
        log_v2::sql()->trace("SQL: add already existing severity {}", sv.id());
        _severity_update.bind_value_as_u64(0, sv.id());
        _severity_update.bind_value_as_u32(1, sv.type());
        _severity_update.bind_value_as_str(2, sv.name());
        _severity_update.bind_value_as_u32(3, sv.level());
        _severity_update.bind_value_as_u64(4, sv.icon_id());
        _severity_update.bind_value_as_u64(5, severity_id);
        _mysql.run_statement(_severity_update,
                             database::mysql_error::store_severity, false,
                             conn);
      } else {
        log_v2::sql()->trace("SQL: add severity {}", sv.id());
        _severity_insert.bind_value_as_u64(0, sv.id());
        _severity_insert.bind_value_as_u32(1, sv.type());
        _severity_insert.bind_value_as_str(2, sv.name());
        _severity_insert.bind_value_as_u32(3, sv.level());
        _severity_insert.bind_value_as_u64(4, sv.icon_id());
        std::promise<uint64_t> p;
        std::future<uint64_t> future = p.get_future();
        _mysql.run_statement_and_get_int<uint64_t>(
            _severity_insert, std::move(p),
            database::mysql_task::LAST_INSERT_ID, conn);
        try {
          severity_id = future.get();
          _severity_cache[{sv.id(), sv.type()}] = severity_id;
        } catch (const std::exception& e) {
          log_v2::sql()->error(
              "unified sql: unable to insert new severity ({},{}): {}", sv.id(),
              sv.type(), e.what());
        }
      }
      break;
    case Severity_Action_MODIFY:
      _add_action(conn, actions::severities);
      log_v2::sql()->trace("SQL: modify severity {}", sv.id());
      _severity_update.bind_value_as_u64(0, sv.id());
      _severity_update.bind_value_as_u32(1, sv.type());
      _severity_update.bind_value_as_str(2, sv.name());
      _severity_update.bind_value_as_u32(3, sv.level());
      _severity_update.bind_value_as_u64(4, sv.icon_id());
      if (severity_id) {
        _severity_update.bind_value_as_u64(5, severity_id);
        _mysql.run_statement(_severity_update,
                             database::mysql_error::store_severity, false,
                             conn);
        _add_action(conn, actions::severities);
      } else
        log_v2::sql()->error(
            "unified sql: unable to modify severity ({}, {}): not in cache",
            sv.id(), sv.type());
      break;
    case Severity_Action_DELETE:
      log_v2::sql()->trace("SQL: remove severity {}: not implemented", sv.id());
      // FIXME DBO: Delete should be implemented later. This case is difficult
      // particularly when several pollers are running and some of them can
      // be stopped...
      break;
    default:
      log_v2::sql()->error("Bad action in severity object");
      break;
  }
}

void stream::_process_tag(const std::shared_ptr<io::data>& d) {
  if (!_store_in_resources)
    return;

  log_v2::sql()->info("SQL: processing tag");
  _finish_action(-1, actions::tags);

  // Prepare queries.
  if (!_tag_update.prepared())
    _tag_update = _mysql.prepare_query(
        "UPDATE tags SET id=?,type=?,name=? WHERE "
        "tag_id=?");
  if (!_tag_insert.prepared())
    _tag_insert = _mysql.prepare_query(
        "INSERT INTO tags (id,type,name) "
        "VALUES(?,?,?)");
  if (!_tag_delete.prepared())
    _tag_delete =
        _mysql.prepare_query("DELETE FROM resources_tags WHERE tag_id=?");

  // Processed object.
  auto s{static_cast<const neb::pb_tag*>(d.get())};
  auto& tg = s->obj();
  uint64_t tag_id = _tags_cache[{tg.id(), tg.type()}];
  int32_t conn = special_conn::tag % _mysql.connections_count();
  switch (tg.action()) {
    case Tag_Action_ADD:
      if (tag_id) {
        log_v2::sql()->trace("SQL: add already existing tag {}", tg.id());
        _tag_update.bind_value_as_u64(0, tg.id());
        _tag_update.bind_value_as_u32(1, tg.type());
        _tag_update.bind_value_as_str(2, tg.name());
        _tag_update.bind_value_as_u64(3, tag_id);
        _mysql.run_statement(_tag_update, database::mysql_error::store_tag,
                             false, conn);
      } else {
        log_v2::sql()->trace("SQL: add tag {}", tg.id());
        _tag_insert.bind_value_as_u64(0, tg.id());
        _tag_insert.bind_value_as_u32(1, tg.type());
        _tag_insert.bind_value_as_str(2, tg.name());
        std::promise<uint64_t> p;
        std::future<uint64_t> future = p.get_future();
        _mysql.run_statement_and_get_int<uint64_t>(
            _tag_insert, std::move(p), database::mysql_task::LAST_INSERT_ID,
            conn);
        try {
          tag_id = future.get();
          _tags_cache[{tg.id(), tg.type()}] = tag_id;
        } catch (const std::exception& e) {
          log_v2::sql()->error(
              "unified sql: unable to insert new tag ({},{}): {}", tg.id(),
              tg.type(), e.what());
        }
      }
      _add_action(conn, actions::tags);
      break;
    case Tag_Action_MODIFY:
      log_v2::sql()->trace("SQL: modify tag {}", tg.id());
      _tag_update.bind_value_as_u64(0, tg.id());
      _tag_update.bind_value_as_u32(1, tg.type());
      _tag_update.bind_value_as_str(2, tg.name());
      if (tag_id) {
        _tag_update.bind_value_as_u64(3, tag_id);
        _mysql.run_statement(_tag_update, database::mysql_error::store_tag,
                             false, conn);
        _add_action(conn, actions::tags);
      } else
        log_v2::sql()->error(
            "unified sql: unable to modify tag ({}, {}): not in cache", tg.id(),
            tg.type());
      break;
    case Tag_Action_DELETE: {
      auto it = _tags_cache.find({tg.id(), tg.type()});
      if (it != _tags_cache.end()) {
        uint64_t id = it->second;
        log_v2::sql()->trace("SQL: delete tag {}", id);
        _tag_delete.bind_value_as_u64(0, tg.id());
        _mysql.run_statement(_tag_delete,
                             database::mysql_error::delete_resources_tags,
                             false, conn);
        _tags_cache.erase(it);
      } else
        log_v2::sql()->warn(
            "SQL: unable to delete tag ({}, {}): it does not exist in cache",
            tg.id(), tg.type());
    } break;
    default:
      log_v2::sql()->error("Bad action in tag object");
      break;
  }
}

/**
 *  Process an instance configuration event.
 *
 *  @param[in] e  Uncasted instance configuration.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_instance_configuration(const std::shared_ptr<io::data>& d
                                             __attribute__((unused))) {}

/**
 *  Process a responsive instance event.
 *
 * @return The number of events that can be acknowledged.
 */
void stream::_process_responsive_instance(const std::shared_ptr<io::data>& d
                                          __attribute__((unused))) {}

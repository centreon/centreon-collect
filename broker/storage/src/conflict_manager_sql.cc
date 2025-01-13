/**
 * Copyright 2019-2024 Centreon
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
#include <fmt/format.h>

#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/broker/sql/mysql_result.hh"
#include "com/centreon/broker/sql/query_preparator.hh"
#include "com/centreon/broker/sql/table_max_size.hh"
#include "com/centreon/broker/storage/conflict_manager.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::database;
using namespace com::centreon::broker::storage;

/**
 *  @brief Clean tables with data associated to the instance.
 *
 *  Rather than delete appropriate entries in tables, they are instead
 *  deactivated using a specific flag.
 *
 *  @param[in] instance_id Instance ID to remove.
 */
void conflict_manager::_clean_tables(uint32_t instance_id) {
  // no hostgroup and servicegroup clean during this function
  {
    std::lock_guard<std::mutex> l(_group_clean_timer_m);
    _group_clean_timer.cancel();
  }

  /* Database version. */

  _finish_action(-1, -1);
  int32_t conn = _mysql.choose_connection_by_instance(instance_id);
  _logger_sql->debug(
      "conflict_manager: disable hosts and services (instance_id: {})",
      instance_id);
  /* Disable hosts and services. */
  std::string query(fmt::format(
      "UPDATE hosts AS h LEFT JOIN services AS s ON h.host_id = s.host_id "
      "SET h.enabled=0, s.enabled=0 WHERE h.instance_id={}",
      instance_id));
  _mysql.run_query(query, database::mysql_error::clean_hosts_services, conn);
  _add_action(conn, actions::hosts);

  /* Remove host group memberships. */
  _logger_sql->debug(
      "conflict_manager: remove host group memberships (instance_id: {})",
      instance_id);
  query = fmt::format(
      "DELETE hosts_hostgroups FROM hosts_hostgroups LEFT JOIN hosts ON "
      "hosts_hostgroups.host_id=hosts.host_id WHERE hosts.instance_id={}",
      instance_id);
  _mysql.run_query(query, database::mysql_error::clean_hostgroup_members, conn);
  _add_action(conn, actions::hostgroups);

  /* Remove service group memberships */
  _logger_sql->debug(
      "conflict_manager: remove service group memberships (instance_id: {})",
      instance_id);
  query = fmt::format(
      "DELETE services_servicegroups FROM services_servicegroups LEFT JOIN "
      "hosts ON services_servicegroups.host_id=hosts.host_id WHERE "
      "hosts.instance_id={}",
      instance_id);
  _mysql.run_query(query, database::mysql_error::clean_servicegroup_members,
                   conn);
  _add_action(conn, actions::servicegroups);

  /* Remove host parents. */
  _logger_sql->debug("conflict_manager: remove host parents (instance_id: {})",
                     instance_id);
  query = fmt::format(
      "DELETE hhp FROM hosts_hosts_parents AS hhp INNER JOIN hosts as h ON "
      "hhp.child_id=h.host_id OR hhp.parent_id=h.host_id WHERE "
      "h.instance_id={}",
      instance_id);
  _mysql.run_query(query, database::mysql_error::clean_host_parents, conn);
  _add_action(conn, actions::host_parents);

  /* Remove list of modules. */
  _logger_sql->debug("SQL: remove list of modules (instance_id: {})",
                     instance_id);
  query = fmt::format("DELETE FROM modules WHERE instance_id={}", instance_id);
  _mysql.run_query(query, database::mysql_error::clean_modules, conn);
  _add_action(conn, actions::modules);

  // Cancellation of downtimes.
  _logger_sql->debug("SQL: Cancellation of downtimes (instance_id: {})",
                     instance_id);
  query = fmt::format(
      "UPDATE downtimes SET cancelled=1 WHERE actual_end_time IS NULL AND "
      "cancelled=0 "
      "AND instance_id={}",
      instance_id);

  _mysql.run_query(query, database::mysql_error::clean_downtimes, conn);
  _add_action(conn, actions::downtimes);

  // Remove comments.
  _logger_sql->debug("conflict_manager: remove comments (instance_id: {})",
                     instance_id);

  query = fmt::format(
      "UPDATE comments SET deletion_time={} WHERE instance_id={} AND "
      "persistent=0 AND "
      "(deletion_time IS NULL OR deletion_time=0)",
      time(nullptr), instance_id);

  _mysql.run_query(query, database::mysql_error::clean_comments, conn);
  _add_action(conn, actions::comments);

  // Remove custom variables. No need to choose the good instance, there are
  // no constraint between custom variables and instances.
  _logger_sql->debug("Removing custom variables (instance_id: {})",
                     instance_id);
  query = fmt::format(
      "DELETE cv FROM customvariables AS cv INNER JOIN hosts AS h ON "
      "cv.host_id = h.host_id WHERE h.instance_id={}",
      instance_id);

  _finish_action(conn, actions::custom_variables | actions::hosts);
  _mysql.run_query(query, database::mysql_error::clean_customvariables, conn);
  _add_action(conn, actions::custom_variables);

  std::lock_guard<std::mutex> l(_group_clean_timer_m);
  _group_clean_timer.expires_after(std::chrono::minutes(1));
  _group_clean_timer.async_wait([this](const boost::system::error_code& err) {
    if (!err) {
      _clean_group_table();
    }
  });
}

void conflict_manager::_clean_group_table() {
  int32_t conn = _mysql.choose_best_connection(-1);
  /* Remove host groups. */
  _logger_sql->debug("conflict_manager: remove empty host groups ");
  _mysql.run_query(
      "DELETE hg FROM hostgroups AS hg LEFT JOIN hosts_hostgroups AS hhg ON "
      "hg.hostgroup_id=hhg.hostgroup_id WHERE hhg.hostgroup_id IS NULL",
      database::mysql_error::clean_empty_hostgroups, conn);
  _add_action(conn, actions::hostgroups);

  /* Remove service groups. */
  _logger_sql->debug("conflict_manager: remove empty service groups");

  _mysql.run_query(
      "DELETE sg FROM servicegroups AS sg LEFT JOIN services_servicegroups as "
      "ssg ON sg.servicegroup_id=ssg.servicegroup_id WHERE ssg.servicegroup_id "
      "IS NULL",
      database::mysql_error::clean_empty_servicegroups, conn);
  _add_action(conn, actions::servicegroups);
}

/**
 *  Update all the hosts and services of unresponsive instances.
 */
void conflict_manager::_update_hosts_and_services_of_unresponsive_instances() {
  _logger_sql->debug("conflict_manager: checking for outdated instances");

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
void conflict_manager::_update_hosts_and_services_of_instance(uint32_t id,
                                                              bool responsive) {
  int32_t conn = _mysql.choose_connection_by_instance(id);
  _finish_action(conn, actions::hosts);
  _finish_action(-1, actions::acknowledgements | actions::modules |
                         actions::downtimes | actions::comments);

  std::string query;
  if (responsive) {
    query = fmt::format(
        "UPDATE instances SET outdated=FALSE WHERE instance_id={}", id);
    _mysql.run_query(query, database::mysql_error::restore_instances, conn);
    _add_action(conn, actions::instances);
    query = fmt::format(
        "UPDATE hosts AS h LEFT JOIN services AS s ON h.host_id=s.host_id "
        "SET h.state=h.real_state,s.state=s.real_state WHERE h.instance_id={}",
        id);
    _mysql.run_query(query, database::mysql_error::restore_instances, conn);
    _add_action(conn, actions::hosts);
  } else {
    query = fmt::format(
        "UPDATE instances SET outdated=TRUE WHERE instance_id={}", id);
    _mysql.run_query(query, database::mysql_error::restore_instances, conn);
    _add_action(conn, actions::instances);
    query = fmt::format(
        "UPDATE hosts AS h LEFT JOIN services AS s ON h.host_id=s.host_id "
        "SET h.real_state=h.state,s.real_state=s.state,h.state={},s.state={} "
        "WHERE h.instance_id={}",
        static_cast<uint32_t>(com::centreon::engine::host::state_unreachable),
        static_cast<uint32_t>(com::centreon::engine::service::state_unknown),
        id);
    _mysql.run_query(query, database::mysql_error::restore_instances, conn);
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
void conflict_manager::_update_timestamp(uint32_t instance_id) {
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

bool conflict_manager::_is_valid_poller(uint32_t instance_id) {
  /* Check if the poller of id instance_id is deleted. */
  bool deleted = false;
  if (_cache_deleted_instance_id.find(instance_id) !=
      _cache_deleted_instance_id.end()) {
    _logger_sql->info(
        "conflict_manager: discarding some event related to a deleted poller "
        "({})",
        instance_id);
    deleted = true;
  } else
    /* Update poller timestamp. */
    _update_timestamp(instance_id);

  return !deleted;
}

void conflict_manager::_prepare_hg_insupdate_statement() {
  if (!_host_group_insupdate.prepared()) {
    query_preparator::event_unique unique;
    unique.insert("hostgroup_id");
    query_preparator qp(neb::host_group::static_type(), unique);
    _host_group_insupdate = qp.prepare_insert_or_update(_mysql);
  }
}

void conflict_manager::_prepare_sg_insupdate_statement() {
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
void conflict_manager::_process_acknowledgement(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  // Cast object.
  neb::acknowledgement const& ack =
      *static_cast<neb::acknowledgement const*>(d.get());

  // Log message.
  _logger_sql->info(
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
                         database::mysql_error::store_acknowledgement, conn);
  }
  *std::get<2>(t) = true;
}

/**
 *  Process a comment event.
 *
 *  @param[in] e  Uncasted comment.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_comment(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  _finish_action(-1, actions::hosts | actions::instances |
                         actions::host_parents | actions::comments);

  // Cast object.
  neb::comment const& cmmnt{*static_cast<neb::comment const*>(d.get())};

  int32_t conn = _mysql.choose_connection_by_instance(cmmnt.poller_id);

  // Log message.
  _logger_sql->info("SQL: processing comment of poller {} on ({}, {})",
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
                       conn);
  _add_action(conn, actions::comments);
  *std::get<2>(t) = true;
}

/**
 *  Process a custom variable event.
 *
 *  @param[in] e Uncasted custom variable.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_custom_variable(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
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
    _cv_queue.emplace_back(
        std::get<2>(t),
        fmt::format(
            "('{}',{},{},'{}',{},{},{},'{}')",
            misc::string::escape(cv.name,
                                 get_centreon_storage_customvariables_col_size(
                                     centreon_storage_customvariables_name)),
            cv.host_id, cv.service_id,
            misc::string::escape(
                cv.default_value,
                get_centreon_storage_customvariables_col_size(
                    centreon_storage_customvariables_default_value)),
            cv.modified ? 1 : 0, cv.var_type, cv.update_time,
            misc::string::escape(cv.value,
                                 get_centreon_storage_customvariables_col_size(
                                     centreon_storage_customvariables_value))));
    /* Here, we do not update the custom variable boolean ack flag, because
     * it will be updated later when the bulk query will be done:
     * conflict_manager::_update_customvariables() */
  } else {
    int conn = special_conn::custom_variable % _mysql.connections_count();
    _finish_action(-1, actions::custom_variables);

    _logger_sql->info("SQL: disabling custom variable '{}' of ({}, {})",
                      cv.name, cv.host_id, cv.service_id);
    _custom_variable_delete.bind_value_as_i32_k(":host_id", cv.host_id);
    _custom_variable_delete.bind_value_as_i32_k(":service_id", cv.service_id);
    _custom_variable_delete.bind_value_as_str_k(":name", cv.name);

    _mysql.run_statement(_custom_variable_delete,
                         database::mysql_error::remove_customvariable, conn);
    _add_action(conn, actions::custom_variables);
    *std::get<2>(t) = true;
  }
}

/**
 *  Process a custom variable status event.
 *
 *  @param[in] e Uncasted custom variable status.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_custom_variable_status(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);

  // Cast object.
  neb::custom_variable_status const& cv{
      *static_cast<neb::custom_variable_status const*>(d.get())};

  _cvs_queue.emplace_back(
      std::get<2>(t),
      fmt::format("('{}',{},{},{},{},'{}')",
                  misc::string::escape(
                      cv.name, get_centreon_storage_customvariables_col_size(
                                   centreon_storage_customvariables_name)),
                  cv.host_id, cv.service_id, cv.modified ? 1 : 0,
                  cv.update_time,
                  misc::string::escape(
                      cv.value, get_centreon_storage_customvariables_col_size(
                                    centreon_storage_customvariables_value))));

  _logger_sql->info("SQL: updating custom variable '{}' of ({}, {})", cv.name,
                    cv.host_id, cv.service_id);
}

/**
 *  Process a downtime event.
 *
 *  @param[in] e Uncasted downtime.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_downtime(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  // Cast object.
  const neb::downtime& dd = *static_cast<const neb::downtime*>(d.get());

  // Log message.
  _logger_sql->info(
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
    _downtimes_queue.emplace_back(
        std::get<2>(t),
        fmt::format(
            "({},{},'{}',{},{},{},{},{},{},{},{},{},{},{},{},{},{},'{}')",
            dd.actual_end_time.is_null()
                ? "NULL"
                : fmt::format("{}", dd.actual_end_time),
            dd.actual_start_time.is_null()
                ? "NULL"
                : fmt::format("{}", dd.actual_start_time),
            misc::string::escape(dd.author,
                                 get_centreon_storage_downtimes_col_size(
                                     centreon_storage_downtimes_author)),
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
            misc::string::escape(
                dd.comment, get_centreon_storage_downtimes_col_size(
                                centreon_storage_downtimes_comment_data))));
  }
}

/**
 *  Process an host check event.
 *
 *  @param[in] e Uncasted host check.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_host_check(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  _finish_action(-1, actions::instances | actions::downtimes |
                         actions::comments | actions::host_parents);

  // Cast object.
  neb::host_check const& hc = *static_cast<neb::host_check const*>(d.get());

  time_t now = time(nullptr);
  if (hc.check_type ||                  // - passive result
      !hc.active_checks_enabled ||      // - active checks are disabled,
                                        //   status might not be updated
      hc.next_check >= now - 5 * 60 ||  // - normal case
      !hc.next_check) {                 // - initial state
    // Apply to DB.
    _logger_sql->info("SQL: processing host check event (host: {}, command: {}",
                      hc.host_id, hc.command_line);

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
                           database::mysql_error::store_host_check, conn);
      _add_action(conn, actions::hosts);
    }
  } else
    // Do nothing.
    _logger_sql->info(
        "SQL: not processing host check event (host: {}, command: {}, check "
        "type: {}, next check: {}, now: {})",
        hc.host_id, hc.command_line, hc.check_type, hc.next_check, now);
  *std::get<2>(t) = true;
}

/**
 *  Process a host group event.
 *
 *  @param[in] e Uncasted host group.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_host_group(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  int32_t conn = special_conn::host_group % _mysql.connections_count();
  _finish_action(-1, actions::hosts);

  // Cast object.
  neb::host_group const& hg{*static_cast<neb::host_group const*>(d.get())};

  if (hg.enabled) {
    _logger_sql->info("SQL: enabling host group {} ('{}' on instance {})",
                      hg.id, hg.name, hg.poller_id);
    _prepare_hg_insupdate_statement();

    _host_group_insupdate << hg;
    _mysql.run_statement(_host_group_insupdate,
                         database::mysql_error::store_host_group, conn);
    _add_action(conn, actions::hostgroups);
    _hostgroup_cache.insert(hg.id);
  }
  // Delete group.
  else {
    _logger_sql->info("SQL: disabling host group {} ('{}' on instance {})",
                      hg.id, hg.name, hg.poller_id);

    // Delete group members.
    {
      std::string query(fmt::format(
          "DELETE hosts_hostgroups FROM hosts_hostgroups LEFT JOIN hosts"
          " ON hosts_hostgroups.host_id=hosts.host_id"
          " WHERE hosts_hostgroups.hostgroup_id={} AND hosts.instance_id={}",
          hg.id, hg.poller_id));
      _mysql.run_query(query, database::mysql_error::empty, conn);
      _hostgroup_cache.erase(hg.id);
    }
  }
  *std::get<2>(t) = true;
}

/**
 *  Process a host group member event.
 *
 *  @param[in] e Uncasted host group member.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_host_group_member(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  int32_t conn = special_conn::host_group % _mysql.connections_count();
  _finish_action(-1, actions::hostgroups | actions::hosts);

  // Cast object.
  neb::host_group_member const& hgm(
      *static_cast<neb::host_group_member const*>(d.get()));

  if (hgm.enabled) {
    // Log message.
    _logger_sql->info(
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
        _logger_sql->error(
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
                             database::mysql_error::store_host_group, conn);
        _add_action(conn, actions::hostgroups);
      }

      _host_group_member_insert << hgm;
      _mysql.run_statement(_host_group_member_insert,
                           database::mysql_error::store_host_group_member,
                           conn);
      _add_action(conn, actions::hostgroups);
    } else
      _logger_sql->error(
          "SQL: host with host_id = {} does not exist - unable to store "
          "unexisting host in a hostgroup. You should restart centengine.",
          hgm.host_id);
  }
  // Delete.
  else {
    // Log message.
    _logger_sql->info(
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
                         database::mysql_error::delete_host_group_member, conn);
    _add_action(conn, actions::hostgroups);
  }
  *std::get<2>(t) = true;
}

/**
 *  Process an host event.
 *
 *  @param[in] e Uncasted host.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_host(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  _finish_action(-1, actions::instances | actions::hostgroups |
                         actions::host_parents | actions::custom_variables |
                         actions::downtimes | actions::comments);
  neb::host& h = *static_cast<neb::host*>(d.get());

  // Log message.
  _logger_sql->debug(
      "SQL: processing host event (poller: {}, host: {}, name: {})",
      h.poller_id, h.host_id, h.host_name);

  // Processing
  if (_is_valid_poller(h.poller_id)) {
    // FixMe BAM Generate fake host, this host
    // does not contain a display_name
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
                           conn);
      _add_action(conn, actions::hosts);

      // Fill the cache...
      if (h.enabled)
        _cache_host_instance[h.host_id] = h.poller_id;
      else
        _cache_host_instance.erase(h.host_id);
    } else
      _logger_sql->trace(
          "SQL: host '{}' of poller {} has no ID nor alias, probably bam "
          "fake host",
          h.host_name, h.poller_id);
  }
  *std::get<2>(t) = true;
}

/**
 *  Process a host parent event.
 *
 *  @param[in] e Uncasted host parent.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_host_parent(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  int32_t conn = special_conn::host_parent % _mysql.connections_count();
  _finish_action(-1, actions::hosts | actions::comments | actions::downtimes);

  neb::host_parent const& hp(*static_cast<neb::host_parent const*>(d.get()));

  // Enable parenting.
  if (hp.enabled) {
    // Log message.
    _logger_sql->info("SQL: host {} is parent of host {}", hp.parent_id,
                      hp.host_id);

    // Prepare queries.
    if (!_host_parent_insert.prepared()) {
      query_preparator qp(neb::host_parent::static_type());
      _host_parent_insert = qp.prepare_insert(_mysql, true);
    }

    // Insert.
    _host_parent_insert << hp;
    _mysql.run_statement(_host_parent_insert,
                         database::mysql_error::store_host_parentship, conn);
    _add_action(conn, actions::host_parents);
  }
  // Disable parenting.
  else {
    _logger_sql->info("SQL: host {} is not parent of host {} anymore",
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
                         conn);
    _add_action(conn, actions::host_parents);
  }
  *std::get<2>(t) = true;
}

/**
 *  Process a host status event.
 *
 *  @param[in] e Uncasted host status.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_host_status(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  _finish_action(-1, actions::instances | actions::downtimes |
                         actions::comments | actions::custom_variables |
                         actions::hostgroups | actions::host_parents);

  // Processed object.
  neb::host_status const& hs(*static_cast<neb::host_status const*>(d.get()));

  time_t now = time(nullptr);
  if (hs.check_type ||                  // - passive result
      !hs.active_checks_enabled ||      // - active checks are disabled,
                                        //   status might not be updated
      hs.next_check >= now - 5 * 60 ||  // - normal case
      !hs.next_check) {                 // - initial state
    // Apply to DB.
    _logger_sql->info(
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
                         database::mysql_error::store_host_status, conn);
    _add_action(conn, actions::hosts);
  } else
    // Do nothing.
    _logger_sql->info(
        "SQL: not processing host status event (id: {}, check type: {}, last "
        "check: {}, next check: {}, now: {}, state: ({}, {}))",
        hs.host_id, hs.check_type, hs.last_check, hs.next_check, now,
        hs.current_state, hs.state_type);
  *std::get<2>(t) = true;
}

/**
 *  Process an instance event. The thread executing the command is controlled
 *  so that queries depending on this one will be made by the same thread.
 *
 *  @param[in] e Uncasted instance.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_instance(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  neb::instance& i(*static_cast<neb::instance*>(d.get()));
  int32_t conn = _mysql.choose_connection_by_instance(i.poller_id);
  _finish_action(-1, actions::hosts | actions::acknowledgements |
                         actions::modules | actions::downtimes |
                         actions::comments | actions::servicegroups |
                         actions::hostgroups);

  // Log message.
  _logger_sql->info(
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
                         database::mysql_error::store_poller, conn);
    _add_action(conn, actions::instances);
  }
  *std::get<2>(t) = true;
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
void conflict_manager::_process_instance_status(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  neb::instance_status& is = *static_cast<neb::instance_status*>(d.get());
  int32_t conn = _mysql.choose_connection_by_instance(is.poller_id);

  _finish_action(-1, actions::hosts | actions::acknowledgements |
                         actions::modules | actions::downtimes |
                         actions::comments);

  // Log message.
  _logger_sql->info(
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
                         database::mysql_error::update_poller, conn);
    _add_action(conn, actions::instances);
  }
  *std::get<2>(t) = true;
}

/**
 *  Process a log event.
 *
 *  @param[in] e Uncasted log.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_log(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);

  // Fetch proper structure.
  neb::log_entry const& le(*static_cast<neb::log_entry const*>(d.get()));

  // Log message.
  _logger_sql->info(
      "SQL: processing log of poller '{}' generated at {} (type {})",
      le.poller_name, le.c_time, le.msg_type);

  // Run query.
  _log_queue.emplace_back(std::make_pair(
      std::get<2>(t),
      fmt::format(
          "({},{},{},'{}','{}',{},{},'{}','{}',{},'{}',{},'{}')", le.c_time,
          le.host_id, le.service_id,
          misc::string::escape(le.host_name,
                               get_centreon_storage_logs_col_size(
                                   centreon_storage_logs_host_name)),
          misc::string::escape(le.poller_name,
                               get_centreon_storage_logs_col_size(
                                   centreon_storage_logs_instance_name)),
          le.log_type, le.msg_type,
          misc::string::escape(le.notification_cmd,
                               get_centreon_storage_logs_col_size(
                                   centreon_storage_logs_notification_cmd)),
          misc::string::escape(le.notification_contact,
                               get_centreon_storage_logs_col_size(
                                   centreon_storage_logs_notification_contact)),
          le.retry,
          misc::string::escape(le.service_description,
                               get_centreon_storage_logs_col_size(
                                   centreon_storage_logs_service_description)),
          le.status,
          misc::string::escape(le.output, get_centreon_storage_logs_col_size(
                                              centreon_storage_logs_output)))));
}

/**
 *  Process a service check event.
 *
 *  @param[in] e Uncasted service check.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_service_check(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  _finish_action(
      -1, actions::downtimes | actions::comments | actions::host_parents);

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
    _logger_sql->info(
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
                             conn);
      } else
        _logger_sql->error(
            "SQL: host with host_id = {} does not exist - unable to store "
            "service command check of that host. You should restart centengine",
            sc.host_id);
    }
  } else
    // Do nothing.
    _logger_sql->info(
        "SQL: not processing service check event (host: {}, service: {}, "
        "command: {}, check_type: {}, next_check: {}, now: {})",
        sc.host_id, sc.service_id, sc.command_line, sc.check_type,
        sc.next_check, now);
  *std::get<2>(t) = true;
}

/**
 *  Process a service group event.
 *
 *  @param[in] e Uncasted service group.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_service_group(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  int32_t conn = special_conn::service_group % _mysql.connections_count();
  _finish_action(-1, actions::hosts | actions::services);

  // Cast object.
  neb::service_group const& sg(
      *static_cast<neb::service_group const*>(d.get()));

  // Insert/update group.
  if (sg.enabled) {
    _logger_sql->info("SQL: enabling service group {} ('{}' on instance {})",
                      sg.id, sg.name, sg.poller_id);
    _prepare_sg_insupdate_statement();

    _service_group_insupdate << sg;
    _mysql.run_statement(_service_group_insupdate,
                         database::mysql_error::store_service_group, conn);
    _add_action(conn, actions::servicegroups);
    _servicegroup_cache.insert(sg.id);
  }
  // Delete group.
  else {
    _logger_sql->info("SQL: disabling service group {} ('{}' on instance {})",
                      sg.id, sg.name, sg.poller_id);

    // Delete group members.
    {
      std::string query(fmt::format(
          "DELETE services_servicegroups FROM services_servicegroups LEFT "
          "JOIN hosts ON services_servicegroups.host_id=hosts.host_id WHERE "
          "services_servicegroups.servicegroup_id={} AND "
          "hosts.instance_id={}",
          sg.id, sg.poller_id));
      _mysql.run_query(query, database::mysql_error::empty, conn);
      _add_action(conn, actions::servicegroups);
      _servicegroup_cache.erase(sg.id);
    }
  }
  *std::get<2>(t) = true;
}

/**
 *  Process a service group member event.
 *
 *  @param[in] e Uncasted service group member.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_service_group_member(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  int32_t conn = special_conn::service_group % _mysql.connections_count();
  _finish_action(-1,
                 actions::hosts | actions::servicegroups | actions::services);

  // Cast object.
  neb::service_group_member const& sgm(
      *static_cast<neb::service_group_member const*>(d.get()));

  if (sgm.enabled) {
    // Log message.
    _logger_sql->info(
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
      _logger_sql->error(
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
                           database::mysql_error::store_service_group, conn);
      _add_action(conn, actions::servicegroups);
    }

    _service_group_member_insert << sgm;
    _mysql.run_statement(_service_group_member_insert,
                         database::mysql_error::store_service_group_member,
                         conn);
    _add_action(conn, actions::servicegroups);
  }
  // Delete.
  else {
    // Log message.
    _logger_sql->info(
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
                         conn);
    _add_action(conn, actions::servicegroups);
  }
  *std::get<2>(t) = true;
}

/**
 *  Process a service event.
 *
 *  @param[in] e Uncasted service.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_service(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  _finish_action(
      -1, actions::host_parents | actions::comments | actions::downtimes);

  // Processed object.
  const neb::service& s(*static_cast<neb::service const*>(d.get()));
  if (_cache_host_instance[s.host_id]) {
    int32_t conn =
        _mysql.choose_connection_by_instance(_cache_host_instance[s.host_id]);

    // Log message.
    _logger_sql->info(
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
                           database::mysql_error::store_service, conn);
      _add_action(conn, actions::services);
    } else
      _logger_sql->trace(
          "SQL: service '{}' has no host ID, service ID nor hostname, probably "
          "bam fake service",
          s.service_description);
  } else
    _logger_sql->error(
        "SQL: host with host_id = {} does not exist - unable to store service "
        "of that host. You should restart centengine",
        s.host_id);
  *std::get<2>(t) = true;
}

/**
 *  Process a service status event.
 *
 *  @param[in] e Uncasted service status.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_service_status(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  auto& d = std::get<0>(t);
  _finish_action(
      -1, actions::host_parents | actions::comments | actions::downtimes);
  // Processed object.
  neb::service_status const& ss{
      *static_cast<neb::service_status const*>(d.get())};

  _logger_storage->info("SQL: service status output: <<{}>>", ss.output);
  _logger_storage->info("SQL: service status perfdata: <<{}>>", ss.perf_data);

  time_t now = time(nullptr);
  if (ss.check_type ||           // - passive result
      !ss.active_checks_enabled  // - active checks are disabled,
                                 //   status might not be updated
      ||                         // - normal case
      ss.next_check >= now - 5 * 60 || !ss.next_check) {  // - initial state
    // Apply to DB.
    _logger_sql->info(
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
                         database::mysql_error::store_service_status, conn);
    _add_action(conn, actions::services);
  } else
    // Do nothing.
    _logger_sql->info(
        "SQL: not processing service status event (host: {}, service: {}, "
        "check type: {}, last check: {}, next check: {}, now: {}, state ({}, "
        "{}))",
        ss.host_id, ss.service_id, ss.check_type, ss.last_check, ss.next_check,
        now, ss.current_state, ss.state_type);
  *std::get<2>(t) = true;
}

/**
 *  Process a responsive instance event.
 *
 * @return The number of events that can be acknowledged.
 */
void conflict_manager::_process_responsive_instance(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  *std::get<2>(t) = true;
}

void conflict_manager::_process_severity(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  _logger_sql->debug("SQL: process severity");
  auto& d = std::get<0>(t);
  _finish_action(-1, actions::severities);

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
  uint64_t severity_id = _severity_cache[{sv.id(), sv.type()}];
  int32_t conn = special_conn::severity % _mysql.connections_count();
  switch (sv.action()) {
    case Severity_Action_ADD:
      _add_action(conn, actions::severities);
      if (severity_id) {
        _logger_sql->trace("SQL: add already existing severity {}", sv.id());
        _severity_update.bind_value_as_u64(0, sv.id());
        _severity_update.bind_value_as_u32(1, sv.type());
        _severity_update.bind_value_as_str(2, sv.name());
        _severity_update.bind_value_as_u32(3, sv.level());
        _severity_update.bind_value_as_u64(4, sv.icon_id());
        _severity_update.bind_value_as_u64(5, severity_id);
        _mysql.run_statement(_severity_update,
                             database::mysql_error::store_severity, conn);
      } else {
        _logger_sql->trace("SQL: add severity {}", sv.id());
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
          _logger_sql->error(
              "unified sql: unable to insert new severity ({},{}): {}", sv.id(),
              sv.type(), e.what());
        }
      }
      break;
    case Severity_Action_MODIFY:
      _logger_sql->trace("SQL: modify severity {}", sv.id());
      _severity_update.bind_value_as_u64(0, sv.id());
      _severity_update.bind_value_as_u32(1, sv.type());
      _severity_update.bind_value_as_str(2, sv.name());
      _severity_update.bind_value_as_u32(3, sv.level());
      _severity_update.bind_value_as_u64(4, sv.icon_id());
      if (severity_id) {
        _severity_update.bind_value_as_u64(5, severity_id);
        _mysql.run_statement(_severity_update,
                             database::mysql_error::store_severity, conn);
        _add_action(conn, actions::severities);
      } else
        _logger_sql->error(
            "unified sql: unable to modify severity ({}, {}): not in cache",
            sv.id(), sv.type());
      break;
    case Severity_Action_DELETE:
      _logger_sql->trace("SQL: remove severity {}: not implemented", sv.id());
      // FIXME DBO: Delete should be implemented later. This case is difficult
      // particularly when several pollers are running and some of them can
      // be stopped...
      break;
    default:
      _logger_sql->error("Bad action in severity object");
      break;
  }
  *std::get<2>(t) = true;
}

void conflict_manager::_process_tag(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>& t) {
  _logger_sql->debug("SQL: process tag");
  auto& d = std::get<0>(t);
  _finish_action(-1, actions::tags);

  // Prepare queries.
  if (!_tag_insert.prepared()) {
    _tag_update = _mysql.prepare_query(
        "UPDATE tags SET id=?,type=?,name=? WHERE "
        "tag_id=?");
    _tag_insert = _mysql.prepare_query(
        "INSERT INTO tags (id,type,name) "
        "VALUES(?,?,?)");
  }
  // Processed object.
  auto s{static_cast<const neb::pb_tag*>(d.get())};
  auto& tg = s->obj();
  uint64_t tag_id = _tags_cache[{tg.id(), tg.type()}];
  int32_t conn = special_conn::tag % _mysql.connections_count();
  switch (tg.action()) {
    case Tag_Action_ADD:
      if (tag_id) {
        _logger_sql->trace("SQL: add already existing tag {}", tg.id());
        _tag_update.bind_value_as_u64(0, tg.id());
        _tag_update.bind_value_as_u32(1, tg.type());
        _tag_update.bind_value_as_str(2, tg.name());
        _tag_update.bind_value_as_u64(3, tag_id);
        _mysql.run_statement(_tag_update, database::mysql_error::store_tag,
                             conn);
      } else {
        _logger_sql->trace("SQL: add tag {}", tg.id());
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
          _logger_sql->error(
              "unified sql: unable to insert new tag ({},{}): {}", tg.id(),
              tg.type(), e.what());
        }
      }
      _add_action(conn, actions::tags);
      break;
    case Tag_Action_MODIFY:
      _logger_sql->trace("SQL: modify tag {}", tg.id());
      _tag_update.bind_value_as_u64(0, tg.id());
      _tag_update.bind_value_as_u32(1, tg.type());
      _tag_update.bind_value_as_str(2, tg.name());
      if (tag_id) {
        _tag_update.bind_value_as_u64(3, tag_id);
        _mysql.run_statement(_tag_update, database::mysql_error::store_tag,
                             conn);
        _add_action(conn, actions::tags);
      } else
        _logger_sql->error(
            "unified sql: unable to modify tag ({}, {}): not in cache", tg.id(),
            tg.type());
      break;
      break;
    default:
      _logger_sql->error("Bad action in tag object");
      break;
  }
}

/**
 * @brief Send a big query to update/insert a bulk of custom variables. When
 * the query is done, we set the corresponding boolean of each pair to true
 * to ack each event.
 *
 * When we exit the function, the custom variables queue is empty.
 */
void conflict_manager::_update_customvariables() {
  int32_t conn = special_conn::custom_variable % _mysql.connections_count();
  _finish_action(conn, actions::custom_variables);
  if (!_cv_queue.empty()) {
    auto it = _cv_queue.begin();
    std::ostringstream oss;
    oss << "INSERT INTO customvariables "
           "(name,host_id,service_id,default_value,modified,type,update_time,"
           "value) VALUES "
        << std::get<1>(*it);
    for (++it; it != _cv_queue.end(); ++it)
      oss << "," << std::get<1>(*it);

    /* Building of the query */
    oss << " ON DUPLICATE KEY UPDATE "
           "default_value=VALUES(default_VALUE),modified=VALUES(modified),type="
           "VALUES(type),update_time=VALUES(update_time),value=VALUES(value)";
    std::string query(oss.str());
    _mysql.run_query(query, database::mysql_error::update_customvariables,
                     conn);
    _logger_sql->debug("{} new custom variables inserted", _cv_queue.size());
    _logger_sql->trace("sending query << {} >>", query);
    _add_action(conn, actions::custom_variables);

    /* Acknowledgement and cleanup */
    while (!_cv_queue.empty()) {
      auto it = _cv_queue.begin();
      *std::get<0>(*it) = true;
      _cv_queue.pop_front();
    }
  }
  if (!_cvs_queue.empty()) {
    auto it = _cvs_queue.begin();
    std::ostringstream oss;
    oss << "INSERT INTO customvariables "
           "(name,host_id,service_id,modified,update_time,value) VALUES "
        << std::get<1>(*it);
    *std::get<0>(*it) = true;
    for (++it; it != _cvs_queue.end(); ++it)
      oss << "," << std::get<1>(*it);

    /* Building of the query */
    oss << " ON DUPLICATE KEY UPDATE "
           "modified=VALUES(modified),update_time=VALUES(update_time),value="
           "VALUES(value)";
    std::string query(oss.str());
    _mysql.run_query(query, database::mysql_error::update_customvariables,
                     conn);
    _logger_sql->debug("{} new custom variable status inserted",
                       _cvs_queue.size());
    _logger_sql->trace("sending query << {} >>", query);
    _add_action(conn, actions::custom_variables);

    /* Acknowledgement and cleanup */
    while (!_cvs_queue.empty()) {
      auto it = _cvs_queue.begin();
      *std::get<0>(*it) = true;
      _cvs_queue.pop_front();
    }
  }
}

/**
 * @brief Send a big query to update/insert a bulk of downtimes. When
 * the query is done, we set the corresponding boolean of each pair to true
 * to ack each event.
 *
 * When we exit the function, the downtimes queue is empty.
 */
void conflict_manager::_update_downtimes() {
  _logger_sql->debug("sql: update downtimes");
  int32_t conn = special_conn::downtime % _mysql.connections_count();
  _finish_action(-1, actions::hosts | actions::instances | actions::downtimes |
                         actions::host_parents);
  if (!_downtimes_queue.empty()) {
    auto it = _downtimes_queue.begin();
    std::ostringstream oss;
    oss << "INSERT INTO downtimes (actual_end_time,actual_start_time,author,"
           "type,deletion_time,duration,end_time,entry_time,"
           "fixed,host_id,instance_id,internal_id,service_id,"
           "start_time,triggered_by,cancelled,started,comment_data) "
           "VALUES"
        << std::get<1>(*it);
    for (++it; it != _downtimes_queue.end(); ++it)
      oss << "," << std::get<1>(*it);

    /* The duplicate part */
    oss << " ON DUPLICATE KEY UPDATE "
           "actual_end_time=GREATEST(COALESCE(actual_end_time,-1),VALUES("
           "actual_end_time)),actual_start_time=COALESCE(actual_start_time,"
           "VALUES(actual_start_time)),author=VALUES(author),cancelled=VALUES("
           "cancelled),comment_data=VALUES(comment_data),deletion_time=VALUES("
           "deletion_time),duration=VALUES(duration),end_time=VALUES(end_time),"
           "fixed=VALUES(fixed),start_time=VALUES(start_time),started=VALUES("
           "started),triggered_by=VALUES(triggered_by), type=VALUES(type)";
    std::string query{oss.str()};

    _mysql.run_query(query, database::mysql_error::store_downtime, conn);
    _logger_sql->debug("{} new downtimes inserted", _downtimes_queue.size());
    _logger_sql->trace("sending query << {} >>", query);
    _add_action(conn, actions::downtimes);

    /* Acknowledgement and cleanup */
    while (!_downtimes_queue.empty()) {
      auto it = _downtimes_queue.begin();
      *std::get<0>(*it) = true;
      _downtimes_queue.pop_front();
    }
  }
}

/**
 * @brief Send a big query to insert a bulk of logs. When the query is done,
 * we set the corresponding boolean of each pair to true to ack each event.
 *
 * When we exit the function, the logs queue is empty.
 */
void conflict_manager::_insert_logs() {
  if (_log_queue.empty())
    return;
  int32_t conn = special_conn::log % _mysql.connections_count();
  auto it = _log_queue.begin();
  std::ostringstream oss;

  /* Building of the query */
  oss << "INSERT INTO logs "
         "(ctime,host_id,service_id,host_name,instance_name,type,msg_type,"
         "notification_cmd,notification_contact,retry,service_description,"
         "status,output) VALUES "
      << std::get<1>(*it);
  *std::get<0>(*it) = true;
  for (++it; it != _log_queue.end(); ++it)
    oss << "," << std::get<1>(*it);

  std::string query(oss.str());
  _mysql.run_query(query, database::mysql_error::update_logs, conn);
  _logger_sql->debug("{} new logs inserted", _log_queue.size());
  _logger_sql->trace("sending query << {} >>", query);

  /* Acknowledgement and cleanup */
  while (!_log_queue.empty()) {
    auto it = _log_queue.begin();
    *std::get<0>(*it) = true;
    _log_queue.pop_front();
  }
}

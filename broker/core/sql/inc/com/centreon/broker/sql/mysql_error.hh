/**
 * Copyright 2018-2024 Centreon
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

#ifndef CCB_MYSQL_ERROR_HH
#define CCB_MYSQL_ERROR_HH

namespace com::centreon::broker {

namespace database {
/**
 *  @class mysql_thread mysql_thread.hh
 * "com/centreon/broker/storage/mysql_thread.hh"
 *  @brief Class representing a thread connected to the mysql server
 *
 */
class mysql_error {
 public:
  enum code {
    empty = 0,
    clean_hosts_services = 1,
    clean_hostgroup_members = 2,
    clean_servicegroup_members = 3,
    clean_empty_hostgroups = 4,
    clean_empty_servicegroups = 5,
    clean_host_parents = 6,
    clean_modules = 7,
    clean_downtimes = 8,
    clean_comments = 9,
    clean_customvariables = 10,
    restore_instances = 11,
    update_customvariables = 12,
    update_logs = 13,
    update_metrics = 14,
    insert_data = 15,
    delete_metric = 16,
    delete_index = 17,
    flag_index_data = 18,
    delete_hosts = 19,
    delete_modules = 20,
    update_index_state = 21,
    delete_availabilities = 22,
    insert_availability = 23,
    rebuild_ba = 24,
    close_event = 25,
    close_ba_events = 26,
    close_kpi_events = 27,
    delete_ba_durations = 28,
    store_host_state = 29,
    store_acknowledgement = 30,
    store_comment = 31,
    remove_customvariable = 32,
    store_customvariable = 33,
    store_downtime = 34,
    store_eventhandler = 35,
    store_flapping = 36,
    store_host_check = 37,
    store_host_group = 38,
    store_host_group_member = 39,
    delete_host_group_member = 40,
    store_host = 41,
    store_host_parentship = 42,
    store_host_status = 43,
    store_poller = 44,
    update_poller = 45,
    store_module = 46,
    store_service_check_command = 47,
    store_service_group = 48,
    store_service_group_member = 49,
    delete_service_group_member = 50,
    store_service = 51,
    store_service_status = 52,
    update_ba = 53,
    update_kpi = 54,
    update_kpi_event = 55,
    insert_kpi_event = 56,
    insert_ba = 57,
    insert_bv = 58,
    insert_dimension_ba_bv = 59,
    truncate_dimension_table = 60,
    insert_dimension_kpi = 61,
    insert_timeperiod = 62,
    insert_timeperiod_exception = 63,
    insert_exclusion_timeperiod = 64,
    insert_relation_ba_timeperiod = 65,
    store_severity = 66,
    clean_severities = 67,
    store_tag = 68,
    clean_resources_tags = 69,
    update_index_data = 70,
    update_resources = 71,
    store_host_resources = 72,
    store_tags_resources_tags = 73,
    delete_resources_tags = 74,
    clean_resources = 75,
    delete_poller = 76,
    update_hosts_enabled = 77,
    update_services_enabled = 78,
    update_hosts_resources_enabled = 79,
    update_services_resources_enabled = 80,
    insert_update_agent_information = 81
  };

  static constexpr const char* msg[]{
      "error: ",
      "could not clean hosts and services tables: ",
      "could not clean host groups memberships table: ",
      "could not clean service groups memberships table: ",
      "could not remove empty host groups: ",
      "could not remove empty service groups: ",
      "could not clean host parents table: ",
      "could not clean modules table: ",
      "could not clean downtimes table: ",
      "could not clean comments table: ",
      "could not clean custom variables table: ",
      "could not restore outdated instance: ",
      "could not store custom variables correctly: ",
      "could not store logs correctly: ",
      "could not update metrics: ",
      "could not insert data in data_bin: ",
      "could not delete metric: ",
      "could not delete index: ",
      "could not flag the index_data table to delete outdated entries: ",
      "could not delete outdated entries from the hosts table: ",
      "could not delete outdated entries from the modules table: ",
      "cannot update state of index: ",
      ("availability thread could not delete the BA availabilities from the "
       "reporting database: "),
      "availability thread could not insert an availability: ",
      "could not update the list of BAs to rebuild: ",
      "could not close inconsistent event: ",
      "could not close all ba events: ",
      "could not close all kpi events: ",
      "could not delete BA durations: ",
      "could not store host state event: ",
      "could not store acknowledgement: ",
      "could not store comment: ",
      "could not remove custom variable: ",
      "could not store custom variable: ",
      "could not store downtime: ",
      "could not store event handler: ",
      "could not store flapping status: ",
      "could not store host check: ",
      "could not store host group: ",
      "could not store host group membership: ",
      "could not delete membership of host to host group: ",
      "could not store host: ",
      "could not store host parentship: ",
      "could not store host status: ",
      "could not store poller: ",
      "could not update poller: ",
      "could not store module: ",
      "could not store service check command: ",
      "could not store service group: ",
      "could not store service group membership: ",
      "could not delete membersjip of service to service group: ",
      "could not store service: ",
      "could not store service status: ",
      "could not update BA: ",
      "could not update KPI: ",
      "could not update kpi event: ",
      "could not insert kpi event: ",
      "could not insert BA: ",
      "could not insert BV: ",
      "could not insert dimension of BA-BV relation: ",
      "could not truncate some dimension table: ",
      "could not insert dimension of KPI: ",
      "could not insert timeperiod: ",
      "could not insert exception of timeperiod: ",
      "could not insert exclusion of timeperiod: ",
      "could not insert relation of BA to timeperiod: ",
      "could not insert severity in severities table: ",
      "could not remove severities: ",
      "could not insert tag in tags table: ",
      "could not remove resources tags: ",
      "could not update index data: ",
      "could not update resources: ",
      "could not insert host in resources: ",
      "could not insert tag in resources_tags table: ",
      "could not delete entry in resources_tags table: ",
      "could not clean the resources table: ",
      "could not delete poller: ",
      "could not update the enabled flag in hosts table: ",
      "could not update the enabled flag in services table: ",
      "could not update the enabled flag in resources table for host: ",
      "could not update the enabled flag in resources table for service: ",
      "could not insert or update agent_information table: "};

  mysql_error() : _active(false) {}
  mysql_error(mysql_error const& other) = delete;
  mysql_error(mysql_error&& other) = delete;
  mysql_error(char const* message) : _message(message), _active(true) {}
  mysql_error& operator=(mysql_error const& other) = delete;
  std::string get_message() {
    clear();
    return std::move(_message);
  }

  template <typename... Args>
  void set_message(std::string const& format, const Args&... args) {
    _message = fmt::format(format, args...);
    _active = true;
  }
  void clear() { _active = false; }
  bool is_active() const { return _active; }

 private:
  std::string _message;
  std::atomic<bool> _active;
};

}  // namespace database

}  // namespace com::centreon::broker

#endif  // CCB_MYSQL_ERROR_HH

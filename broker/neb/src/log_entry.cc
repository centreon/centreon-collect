/**
 * Copyright 2009-2013,2015 Centreon
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

#include "com/centreon/broker/neb/log_entry.hh"

#include "com/centreon/broker/sql/table_max_size.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::neb;

/**
 *  @brief Default constructor.
 *
 *  Initialize members to 0, NULL or equivalent.
 */
log_entry::log_entry()
    : io::data(log_entry::static_type()),
      c_time(0),
      host_id(0),
      issue_start_time(0),
      log_type(0),
      msg_type(5),
      retry(0),
      service_id(0),
      status(0) {}

/**
 *  @brief Copy constructor.
 *
 *  Copy all internal data of the given object to the current instance.
 *
 *  @param[in] le Object to copy.
 */
log_entry::log_entry(log_entry const& le) : io::data(le) {
  _internal_copy(le);
}

/**
 *  Destructor.
 */
log_entry::~log_entry() {}

/**
 *  @brief Assignment operator.
 *
 *  Copy all internal data of the given object to the current instance.
 *
 *  @param[in] le Object to copy.
 *
 *  @return This object.
 */
log_entry& log_entry::operator=(log_entry const& le) {
  io::data::operator=(le);
  _internal_copy(le);
  return *this;
}

/**************************************
 *                                     *
 *          Private Methods            *
 *                                     *
 **************************************/

/**
 *  @brief Copy all internal data of the given object to the current
 *         instance.
 *
 *  Make a copy of all data defined within the log_entry class. This
 *  method is used by the copy constructor and the assignment operator.
 *
 *  @param[in] le Object to copy.
 */
void log_entry::_internal_copy(log_entry const& le) {
  c_time = le.c_time;
  host_id = le.host_id;
  host_name = le.host_name;
  issue_start_time = le.issue_start_time;
  log_type = le.log_type;
  msg_type = le.msg_type;
  notification_cmd = le.notification_cmd;
  notification_contact = le.notification_contact;
  output = le.output;
  poller_name = le.poller_name;
  retry = le.retry;
  service_description = le.service_description;
  service_id = le.service_id;
  status = le.status;
}

/**************************************
 *                                     *
 *           Static Objects            *
 *                                     *
 **************************************/

// Mapping.
mapping::entry const log_entry::entries[] = {
    mapping::entry(&log_entry::c_time, "ctime"),
    mapping::entry(&log_entry::host_id,
                   "host_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry(
        &log_entry::host_name,
        "host_name",
        get_centreon_storage_logs_col_size(centreon_storage_logs_host_name)),
    mapping::entry(&log_entry::poller_name,
                   "instance_name",
                   get_centreon_storage_logs_col_size(
                       centreon_storage_logs_instance_name)),
    mapping::entry(&log_entry::issue_start_time,
                   "",
                   mapping::entry::invalid_on_minus_one),
    mapping::entry(&log_entry::log_type, "type"),
    mapping::entry(&log_entry::msg_type, "msg_type"),
    mapping::entry(&log_entry::notification_cmd,
                   "notification_cmd",
                   get_centreon_storage_logs_col_size(
                       centreon_storage_logs_notification_cmd)),
    mapping::entry(&log_entry::notification_contact,
                   "notification_contact",
                   get_centreon_storage_logs_col_size(
                       centreon_storage_logs_notification_contact)),
    mapping::entry(&log_entry::retry, "retry"),
    mapping::entry(&log_entry::service_description,
                   "service_description",
                   get_centreon_storage_logs_col_size(
                       centreon_storage_logs_service_description),
                   mapping::entry::invalid_on_zero),
    mapping::entry(&log_entry::service_id,
                   "service_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&log_entry::status, "status"),
    mapping::entry(
        &log_entry::output,
        "output",
        get_centreon_storage_logs_col_size(centreon_storage_logs_output)),
    mapping::entry()};

// Operations.
static io::data* new_log_entry() {
  return new log_entry;
}
io::event_info::event_operations const log_entry::operations = {
    &new_log_entry, nullptr, nullptr};

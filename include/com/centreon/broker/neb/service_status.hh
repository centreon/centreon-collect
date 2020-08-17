/*
** Copyright 2009-2020 Centreon
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

#ifndef CCB_NEB_SERVICE_STATUS_HH
#define CCB_NEB_SERVICE_STATUS_HH

#include <string>
#include "com/centreon/broker/io/event_info.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/namespace.hh"
#include "com/centreon/broker/neb/host_service_status.hh"
#include "com/centreon/broker/timestamp.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/neb/internal.hh"

CCB_BEGIN()

namespace neb {
/**
 *  @class service_status service_status.hh
 * "com/centreon/broker/neb/service_status.hh"
 *  @brief When the status of a service change, such an event is generated.
 *
 *  This class represents a change in a service status.
 */
class service_status : public host_service_status {
 public:
  service_status(uint32_t type = static_type());
  service_status(std::string host_name,
                 bool acknowledged,
                 short acknowledgement_type,
                 bool active_checks_enabled,
                 std::string check_command,
                 double check_interval,
                 std::string check_period,
                 short check_type,
                 short current_check_attempt,
                 short current_state,
                 short downtime_depth,
                 std::string event_handler,
                 bool event_handler_enabled,
                 double execution_time,
                 bool flap_detection_enabled,
                 bool has_been_checked,
                 uint32_t host_id,
                 bool is_flapping,
                 timestamp last_check,
                 short last_hard_state,
                 timestamp last_hard_state_change,
                 timestamp last_notification,
                 timestamp last_state_change, //
                 timestamp last_time_critical,
                 timestamp last_time_ok,
                 timestamp last_time_unknown,
                 timestamp last_time_warning,
                 timestamp last_update,
                 double latency,
                 short max_check_attempts,
                 timestamp next_check,
                 timestamp next_notification,
                 bool no_more_notifications,
                 short notification_number,
                 bool obsess_over,
                 std::string output,
                 bool passive_checks_enabled,
                 double percent_state_change,
                 std::string perf_data,
                 double retry_interval,
                 std::string service_description,
                 bool should_be_scheduled,
                 short state_type);
  service_status(service_status const& other);
  virtual ~service_status();
  service_status& operator=(service_status const& other);
  constexpr static uint32_t static_type() {
    return io::events::data_type<io::events::neb,
                                 neb::de_service_status>::value;
  }

  std::string host_name;
  timestamp last_time_critical;
  timestamp last_time_ok;
  timestamp last_time_unknown;
  timestamp last_time_warning;
  std::string service_description;
  uint32_t service_id;

  static mapping::entry const entries[];
  static io::event_info::event_operations const operations;

 private:
  void _internal_copy(service_status const& other);
};
}  // namespace neb

CCB_END()

#endif  // !CCB_NEB_SERVICE_STATUS_HH

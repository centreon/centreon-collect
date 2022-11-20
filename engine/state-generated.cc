/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#include "state-generated.hh"

using namespace com::centreon::engine::configuration;

Anomalydetection make_anomalydetection() {
  Anomalydetection retval;

  retval.set_acknowledgement_timeout(0);
  retval.set_status_change(false);
  retval.set_checks_active(true);
  retval.set_checks_passive(true);
  retval.set_check_freshness(0);
  retval.set_check_interval(5);
  retval.set_event_handler_enabled(true);
  retval.set_first_notification_delay(0);
  retval.set_flap_detection_enabled(true);
  retval.set_freshness_threshold(0);
  retval.set_high_flap_threshold(0);
  retval.set_initial_state(engine::service::state_ok);
  retval.set_is_volatile(false);
  retval.set_low_flap_threshold(0);
  retval.set_max_check_attempts(3);
  retval.set_notifications_enabled(true);
  retval.set_notification_interval(30);
  retval.set_obsess_over_service(true);
  retval.set_process_perf_data(true);
  retval.set_retain_nonstatus_information(true);
  retval.set_retain_status_information(true);
  retval.set_retry_interval(1);
  retval.set_stalking_options(anomalydetection::none);
}

Command make_command() {
  Command retval;

}

Connector make_connector() {
  Connector retval;

}

Contact make_contact() {
  Contact retval;

  retval.set_can_submit_commands(true);
  retval.set_host_notifications_enabled(true);
  retval.set_host_notification_options(host::none);
  retval.set_retain_nonstatus_information(true);
  retval.set_retain_status_information(true);
  retval.set_service_notification_options(service::none);
  retval.set_service_notifications_enabled(true);
}

Contactgroup make_contactgroup() {
  Contactgroup retval;

}

Host make_host() {
  Host retval;

  retval.set_checks_active(true);
  retval.set_checks_passive(true);
  retval.set_check_freshness(false);
  retval.set_check_interval(5);
  retval.set_event_handler_enabled(true);
  retval.set_first_notification_delay(0);
  retval.set_flap_detection_enabled(true);
  retval.set_freshness_threshold(0);
  retval.set_high_flap_threshold(0);
  retval.set_initial_state(engine::host::state_up);
  retval.set_low_flap_threshold(0);
  retval.set_max_check_attempts(3);
  retval.set_notifications_enabled(true);
  retval.set_notification_interval(30);
  retval.set_obsess_over_host(true);
  retval.set_process_perf_data(true);
  retval.set_retain_nonstatus_information(true);
  retval.set_retain_status_information(true);
  retval.set_retry_interval(1);
  retval.set_stalking_options(host::none);
}

Hostdependency make_hostdependency() {
  Hostdependency retval;

  retval.set_inherits_parent(false);
}

Hostescalation make_hostescalation() {
  Hostescalation retval;

  retval.set_escalation_options(hostescalation::none);
  retval.set_first_notification(-2);
  retval.set_last_notification(-2);
  retval.set_notification_interval(0);
}

Hostgroup make_hostgroup() {
  Hostgroup retval;

}

Hostextinfo make_hostextinfo() {
  Hostextinfo retval;

  retval.set_coords_2d(-1, -1);
  retval.set_coords_3d(0.0, 0.0, 0.0);
}

Service make_service() {
  Service retval;

  retval.set_acknowledgement_timeout(0);
  retval.set_checks_active(true);
  retval.set_checks_passive(true);
  retval.set_check_freshness(0);
  retval.set_check_interval(5);
  retval.set_event_handler_enabled(true);
  retval.set_first_notification_delay(0);
  retval.set_flap_detection_enabled(true);
  retval.set_freshness_threshold(0);
  retval.set_high_flap_threshold(0);
  retval.set_initial_state(engine::service::state_ok);
  retval.set_is_volatile(false);
  retval.set_low_flap_threshold(0);
  retval.set_max_check_attempts(3);
  retval.set_notifications_enabled(true);
  retval.set_notification_interval(30);
  retval.set_obsess_over_service(true);
  retval.set_process_perf_data(true);
  retval.set_retain_nonstatus_information(true);
  retval.set_retain_status_information(true);
  retval.set_retry_interval(1);
  retval.set_stalking_options(service::none);
}

Servicedependency make_servicedependency() {
  Servicedependency retval;

  retval.set_inherits_parent(false);
}

Serviceescalation make_serviceescalation() {
  Serviceescalation retval;

  retval.set_escalation_options(serviceescalation::none);
  retval.set_first_notification(-2);
  retval.set_last_notification(-2);
  retval.set_notification_interval(0);
}

Serviceextinfo make_serviceextinfo() {
  Serviceextinfo retval;

}

Servicegroup make_servicegroup() {
  Servicegroup retval;

}

Severity make_severity() {
  Severity retval;

}

Tag make_tag() {
  Tag retval;

}

Timeperiod make_timeperiod() {
  Timeperiod retval;

}

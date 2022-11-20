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
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"

namespace com::centreon::engine::configuration {

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
  retval.set_flap_detection_options(action_svc_ok | action_svc_warning |
                                    action_svc_unknown | action_svc_critical);
  retval.set_freshness_threshold(0);
  retval.set_high_flap_threshold(0);
  retval.set_initial_state(engine::service::state_ok);
  retval.set_is_volatile(false);
  retval.set_low_flap_threshold(0);
  retval.set_max_check_attempts(3);
  retval.set_notifications_enabled(true);
  retval.set_notification_interval(30);
  retval.set_notification_options(action_svc_ok | action_svc_warning |
                                  action_svc_critical | action_svc_unknown |
                                  action_svc_flapping | action_svc_downtime);
  retval.set_obsess_over_service(true);
  retval.set_process_perf_data(true);
  retval.set_retain_nonstatus_information(true);
  retval.set_retain_status_information(true);
  retval.set_retry_interval(1);
  retval.set_stalking_options(action_svc_none);
  return retval;
}

Command make_command() {
  Command retval;
  return retval;
}

Connector make_connector() {
  Connector retval;
  return retval;
}

Contact make_contact() {
  Contact retval;
  retval.set_can_submit_commands(true);
  retval.set_host_notifications_enabled(true);
  retval.set_host_notification_options(action_hst_none);
  retval.set_retain_nonstatus_information(true);
  retval.set_retain_status_information(true);
  retval.set_service_notification_options(action_svc_none);
  retval.set_service_notifications_enabled(true);
  return retval;
}

Contactgroup make_contactgroup() {
  Contactgroup retval;
  return retval;
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
  retval.set_flap_detection_options(action_hst_up | action_hst_down |
                                    action_hst_unreachable);
  retval.set_freshness_threshold(0);
  retval.set_high_flap_threshold(0);
  retval.set_initial_state(engine::host::state_up);
  retval.set_low_flap_threshold(0);
  retval.set_max_check_attempts(3);
  retval.set_notifications_enabled(true);
  retval.set_notification_interval(30);
  retval.set_notification_options(action_hst_up | action_hst_down |
                                  action_hst_unreachable | action_hst_flapping |
                                  action_hst_downtime);
  retval.set_obsess_over_host(true);
  retval.set_process_perf_data(true);
  retval.set_retain_nonstatus_information(true);
  retval.set_retain_status_information(true);
  retval.set_retry_interval(1);
  retval.set_stalking_options(action_hst_none);
  return retval;
}

Hostdependency make_hostdependency() {
  Hostdependency retval;
  retval.set_execution_failure_options(action_hd_none);
  retval.set_inherits_parent(false);
  retval.set_notification_failure_options(action_hd_none);
  return retval;
}

Hostescalation make_hostescalation() {
  Hostescalation retval;
  retval.set_escalation_options(action_he_none);
  retval.set_first_notification(-2);
  retval.set_last_notification(-2);
  retval.set_notification_interval(0);
  return retval;
}

Hostgroup make_hostgroup() {
  Hostgroup retval;
  return retval;
}

Hostextinfo make_hostextinfo() {
  Hostextinfo retval;
  retval.mutable_coords_2d()->set_x(-1);
  retval.mutable_coords_2d()->set_y(-1);
  retval.mutable_coords_3d()->set_x(0.0);
  retval.mutable_coords_3d()->set_y(0.0);
  retval.mutable_coords_3d()->set_y(0.0);
  return retval;
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
  retval.set_flap_detection_options(action_svc_ok | action_svc_warning |
                                    action_svc_unknown | action_svc_critical);
  retval.set_freshness_threshold(0);
  retval.set_high_flap_threshold(0);
  retval.set_initial_state(engine::service::state_ok);
  retval.set_is_volatile(false);
  retval.set_low_flap_threshold(0);
  retval.set_max_check_attempts(3);
  retval.set_notifications_enabled(true);
  retval.set_notification_interval(30);
  retval.set_notification_options(action_svc_ok | action_svc_warning |
                                  action_svc_critical | action_svc_unknown |
                                  action_svc_flapping | action_svc_downtime);
  retval.set_obsess_over_service(true);
  retval.set_process_perf_data(true);
  retval.set_retain_nonstatus_information(true);
  retval.set_retain_status_information(true);
  retval.set_retry_interval(1);
  retval.set_stalking_options(action_svc_none);
  return retval;
}

Servicedependency make_servicedependency() {
  Servicedependency retval;
  retval.set_execution_failure_options(action_sd_none);
  retval.set_inherits_parent(false);
  retval.set_notification_failure_options(action_sd_none);
  return retval;
}

Serviceescalation make_serviceescalation() {
  Serviceescalation retval;
  retval.set_escalation_options(action_se_none);
  retval.set_first_notification(-2);
  retval.set_last_notification(-2);
  retval.set_notification_interval(0);
  return retval;
}

Serviceextinfo make_serviceextinfo() {
  Serviceextinfo retval;
  return retval;
}

Servicegroup make_servicegroup() {
  Servicegroup retval;
  return retval;
}

Severity make_severity() {
  Severity retval;
  return retval;
}

Tag make_tag() {
  Tag retval;
  return retval;
}

Timeperiod make_timeperiod() {
  Timeperiod retval;
  return retval;
}
};  // namespace com::centreon::engine::configuration

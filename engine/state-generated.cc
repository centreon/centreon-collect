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

void init_anomalydetection(Anomalydetection* obj) {
  obj->set_acknowledgement_timeout(0);
  obj->set_status_change(false);
  obj->set_checks_active(true);
  obj->set_checks_passive(true);
  obj->set_check_freshness(0);
  obj->set_check_interval(5);
  obj->set_event_handler_enabled(true);
  obj->set_first_notification_delay(0);
  obj->set_flap_detection_enabled(true);
  obj->set_flap_detection_options(action_svc_ok | action_svc_warning |
                                  action_svc_unknown | action_svc_critical);
  obj->set_freshness_threshold(0);
  obj->set_high_flap_threshold(0);
  obj->set_initial_state(engine::service::state_ok);
  obj->set_is_volatile(false);
  obj->set_low_flap_threshold(0);
  obj->set_max_check_attempts(3);
  obj->set_notifications_enabled(true);
  obj->set_notification_interval(30);
  obj->set_notification_options(action_svc_ok | action_svc_warning |
                                action_svc_critical | action_svc_unknown |
                                action_svc_flapping | action_svc_downtime);
  obj->set_obsess_over_service(true);
  obj->set_process_perf_data(true);
  obj->set_retain_nonstatus_information(true);
  obj->set_retain_status_information(true);
  obj->set_retry_interval(1);
  obj->set_stalking_options(action_svc_none);
}

void init_command(Command* /* obj */) {}

void init_connector(Connector* /* obj */) {}

void init_contact(Contact* obj) {
  obj->set_can_submit_commands(true);
  obj->set_host_notifications_enabled(true);
  obj->set_host_notification_options(action_hst_none);
  obj->set_retain_nonstatus_information(true);
  obj->set_retain_status_information(true);
  obj->set_service_notification_options(action_svc_none);
  obj->set_service_notifications_enabled(true);
}

void init_contactgroup(Contactgroup* /* obj */) {}

void init_host(Host* obj) {
  obj->set_checks_active(true);
  obj->set_checks_passive(true);
  obj->set_check_freshness(false);
  obj->set_check_interval(5);
  obj->set_event_handler_enabled(true);
  obj->set_first_notification_delay(0);
  obj->set_flap_detection_enabled(true);
  obj->set_flap_detection_options(action_hst_up | action_hst_down |
                                  action_hst_unreachable);
  obj->set_freshness_threshold(0);
  obj->set_high_flap_threshold(0);
  obj->set_initial_state(engine::host::state_up);
  obj->set_low_flap_threshold(0);
  obj->set_max_check_attempts(3);
  obj->set_notifications_enabled(true);
  obj->set_notification_interval(30);
  obj->set_notification_options(action_hst_up | action_hst_down |
                                action_hst_unreachable | action_hst_flapping |
                                action_hst_downtime);
  obj->set_obsess_over_host(true);
  obj->set_process_perf_data(true);
  obj->set_retain_nonstatus_information(true);
  obj->set_retain_status_information(true);
  obj->set_retry_interval(1);
  obj->set_stalking_options(action_hst_none);
}

void init_hostdependency(Hostdependency* obj) {
  obj->set_execution_failure_options(action_hd_none);
  obj->set_inherits_parent(false);
  obj->set_notification_failure_options(action_hd_none);
}

void init_hostescalation(Hostescalation* obj) {
  obj->set_escalation_options(action_he_none);
  obj->set_first_notification(-2);
  obj->set_last_notification(-2);
  obj->set_notification_interval(0);
}

void init_hostgroup(Hostgroup* /* obj */) {}

void init_service(Service* obj) {
  obj->set_acknowledgement_timeout(0);
  obj->set_checks_active(true);
  obj->set_checks_passive(true);
  obj->set_check_freshness(0);
  obj->set_check_interval(5);
  obj->set_event_handler_enabled(true);
  obj->set_first_notification_delay(0);
  obj->set_flap_detection_enabled(true);
  obj->set_flap_detection_options(action_svc_ok | action_svc_warning |
                                  action_svc_unknown | action_svc_critical);
  obj->set_freshness_threshold(0);
  obj->set_high_flap_threshold(0);
  obj->set_initial_state(engine::service::state_ok);
  obj->set_is_volatile(false);
  obj->set_low_flap_threshold(0);
  obj->set_max_check_attempts(3);
  obj->set_notifications_enabled(true);
  obj->set_notification_interval(30);
  obj->set_notification_options(action_svc_ok | action_svc_warning |
                                action_svc_critical | action_svc_unknown |
                                action_svc_flapping | action_svc_downtime);
  obj->set_obsess_over_service(true);
  obj->set_process_perf_data(true);
  obj->set_retain_nonstatus_information(true);
  obj->set_retain_status_information(true);
  obj->set_retry_interval(1);
  obj->set_stalking_options(action_svc_none);
}

void init_servicedependency(Servicedependency* obj) {
  obj->set_execution_failure_options(action_sd_none);
  obj->set_inherits_parent(false);
  obj->set_notification_failure_options(action_sd_none);
}

void init_serviceescalation(Serviceescalation* obj) {
  obj->set_escalation_options(action_se_none);
  obj->set_first_notification(-2);
  obj->set_last_notification(-2);
  obj->set_notification_interval(0);
}

void init_servicegroup(Servicegroup* /* obj */) {}

void init_severity(Severity* /* obj */) {}

void init_tag(Tag* /* obj */) {}

void init_timeperiod(Timeperiod* /* obj */) {}
};  // namespace com::centreon::engine::configuration

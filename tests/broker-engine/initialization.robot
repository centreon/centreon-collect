*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BEUS
    [Documentation]    Scenario: Verify resources table after initialization
    ...    Given the database is cleaned
    ...    And the Engine is started with a default configuration
    ...    And the Broker is started with a default configuration
    ...    Then the "resources" table contains exactly 1050 rows
    ...    And the "resources" table contains the good hosts and services
    ...    And the "hosts" table contains the good hosts
    ...    And the "services" table contains the good services
    ...    And the "customvariables" table contains the good customvariables
    ...    Then Engine and Broker are stopped
    ...    And we check again these contents

    [Tags]    broker    engine    MON-153802
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    3
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    Ctn Clean Before Suite
    ${cmd_id}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_id}    0
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    # We should have 50 hosts + 1000 services
    Check Query Result    SELECT COUNT(*) FROM resources    ==    ${1050}    retry_timeout=30s    retry_pause=2s
    # We check service 1:1 in resources
    ${output}    Query    SELECT internal_id,type,status,status_ordered,in_downtime,acknowledged,status_confirmed,check_attempts,max_check_attempts,poller_id,severity_id, name, alias, address,parent_name, icon_id, notes_url, notes, action_url, has_graph, notifications_enabled, passive_checks_enabled, active_checks_enabled, last_check_type, last_check, output, enabled, flapping, percent_state_change FROM resources WHERE id=1 AND parent_id=1
    Should Be Equal    "${output}"    "((None, 0, 4, 1, 0, 0, 1, 1, 3, 1, None, 'service_1', None, None, 'host_1', 0, '', '', '', 0, 1, 1, 1, 0, None, None, 1, 0, 0.0),)"    Service 1:1 not as expected in resources table (first step)

    # We check host 1 in resources
    ${output}    Query    SELECT internal_id,type,status,status_ordered,in_downtime,acknowledged,status_confirmed,check_attempts,max_check_attempts,poller_id,severity_id, name, alias, address,parent_name, icon_id, notes_url, notes, action_url, has_graph, notifications_enabled, passive_checks_enabled, active_checks_enabled, last_check_type, last_check, output, enabled, flapping, percent_state_change FROM resources WHERE id=1 AND parent_id=0
    Should Be Equal    "${output}"    "((None, 1, 4, 1, 0, 0, 1, 1, 3, 1, None, 'host_1', 'host_1', '1.0.0.0', 'host_1', 0, '', '', '', 0, 1, 1, 1, 0, None, None, 1, 0, 0.0),)"    Host 1 not as expected in resources table

    # We check service 1:1 in services
    ${output}    Query    SELECT description, acknowledged, acknowledgement_type, action_url, active_checks, check_attempt, check_freshness, check_interval, check_period, check_type, checked, command_line, default_active_checks, default_event_handler_enabled, default_failure_prediction, default_flap_detection, default_notify, default_passive_checks, default_process_perfdata, display_name, enabled, event_handler, event_handler_enabled, execution_time, failure_prediction, failure_prediction_options, first_notification_delay, flap_detection, flap_detection_on_critical, flap_detection_on_ok, flap_detection_on_unknown, flap_detection_on_warning, flapping, freshness_threshold, high_flap_threshold, icon_image, icon_image_alt, last_check, last_hard_state, last_hard_state_change, last_notification, last_state_change, last_time_critical, last_time_ok, last_time_unknown, last_time_warning, latency, low_flap_threshold, max_check_attempts, modified_attributes, next_notification, no_more_notifications, notes, notes_url, notification_interval, notification_number, notification_period, notify, notify_on_critical, notify_on_downtime, notify_on_flapping, notify_on_recovery, notify_on_unknown, notify_on_warning, obsess_over_service, output, passive_checks, percent_state_change, perfdata, process_perfdata, retain_nonstatus_information, retain_status_information, retry_interval, scheduled_downtime_depth, should_be_scheduled, stalk_on_critical, stalk_on_ok, stalk_on_unknown, stalk_on_warning, state, state_type, volatile, real_state FROM services WHERE host_id=1 AND service_id=1
    Should Be Equal    "${output}"    "(('service_1', 0, 0, '', 1, 1, 0, 5.0, '24x7', 0, 0, None, 1, 1, None, 1, 1, 1, None, 'service_1', 1, '', 1, 0.0, None, None, 0.0, 1, 1, 1, 1, 1, 0, 0.0, 0.0, '', '', None, 0, None, None, None, None, None, None, None, 0.0, 0.0, 3, None, None, 0, '', '', 0.0, 0, '', 1, 1, 1, 1, 1, 1, 1, 1, '', 1, 0.0, '', None, 1, 1, 5.0, 0, 1, 0, 0, 0, 0, 4, 1, 0, None),)"    Service 1:1 not as expected in services table (first step)

    ${svc_state}    Query    SELECT state FROM services WHERE host_id=1 AND service_id=1
    ${svc_last_update}    Query    SELECT last_update FROM services WHERE host_id=1 AND service_id=1
    ${svc_next_check}    Query    SELECT next_check FROM services WHERE host_id=1 AND service_id=1
    ${svc_latency}    Query    SELECT latency FROM services WHERE host_id=1 AND service_id=1

    # We check host 1 in hosts
    ${output}    Query    SELECT host_id, name, instance_id, acknowledged, acknowledgement_type, action_url, active_checks, address, alias, check_attempt, check_command, check_freshness, check_interval, check_period, check_type, checked, command_line, default_active_checks, default_event_handler_enabled, default_failure_prediction, default_flap_detection, default_notify, default_passive_checks, default_process_perfdata, display_name, enabled, event_handler, event_handler_enabled, execution_time, failure_prediction, first_notification_delay, flap_detection, flap_detection_on_down, flap_detection_on_unreachable, flap_detection_on_up, flapping, freshness_threshold, high_flap_threshold, icon_image, icon_image_alt, last_check, last_hard_state, last_hard_state_change, last_notification, last_state_change, last_time_down, last_time_unreachable, last_time_up, latency, low_flap_threshold, max_check_attempts, modified_attributes, next_host_notification, no_more_notifications, notes, notes_url, notification_interval, notification_number, notification_period, notify, notify_on_down, notify_on_downtime, notify_on_flapping, notify_on_recovery, notify_on_unreachable, obsess_over_host, output, passive_checks, percent_state_change, perfdata, process_perfdata, retain_nonstatus_information, retain_status_information, retry_interval, scheduled_downtime_depth, should_be_scheduled, stalk_on_down, stalk_on_unreachable, stalk_on_up, state, state_type, statusmap_image, timezone, real_state FROM hosts WHERE host_id=1

    Should Be Equal    "${output}"    "((1, 'host_1', 1, 0, 0, '', 1, '1.0.0.0', 'host_1', 1, 'checkh1', 0, 5.0, '24x7', 0, 0, '/tmp/var/lib/centreon-engine/check.pl --id 0', 1, 1, None, 1, 1, 1, None, 'host_1', 1, '', 1, 0.0, None, 0.0, 1, 1, 1, 1, 0, 0.0, 0.0, '', '', None, 0, None, None, None, None, None, None, 0.0, 0.0, 3, None, None, 0, '', '', 0.0, 0, '', 1, 1, 1, 1, 1, 1, 1, '', 1, 0.0, '', None, 1, 1, 1.0, 0, 1, 0, 0, 0, 4, 1, '', '', None),)"    Host 1 not as expected in hosts table

    ${output}    Query    SELECT instance_id, name, active_host_checks, active_service_checks, address, check_hosts_freshness, check_services_freshness, daemon_mode, description, engine, event_handlers, failure_prediction, flap_detection, global_host_event_handler, global_service_event_handler, last_alive, last_command_check, last_log_rotation, modified_host_attributes, modified_service_attributes, notifications, obsess_over_hosts, obsess_over_services, passive_host_checks, passive_service_checks, process_perfdata, running, deleted, outdated FROM instances
    Should Be Equal    "${output}"    "((1, 'Poller0', None, None, None, None, None, None, None, 'Centreon Engine', None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, 1, 0, 0),)"    Poller 1 not as expected in instances table

    Check Query Result    SELECT COUNT(*) FROM customvariables    ==    ${1150}    retry_timeout=30s    retry_pause=2s

    ${output}    Query    SELECT host_id, service_id, name, default_value, value, modified, type FROM customvariables WHERE host_id = 1 AND service_id = 1
    Should Be Equal    "${output}"    "((1, 1, 'KEY_SERV1_1', 'VAL_SERV1', 'VAL_SERV1', 0, 1),)"    Custom variable for service 1:1 not as expected in customvariables table

    Check Query Result    SELECT status FROM resources WHERE id=1 AND parent_id=0    ==    ${0}    retry_timeout=30s    retry_pause=2s
    Check Query Result    SELECT state FROM hosts WHERE host_id=1    ==    ${0}    retry_timeout=30s    retry_pause=2s
    Check Query Result    SELECT state FROM services WHERE host_id=1 AND service_id=1    ==    ${0}    retry_timeout=30s    retry_pause=2s

    ${svc_state1}    Query    SELECT state FROM services WHERE host_id=1 AND service_id=1
    ${svc_last_update1}    Query    SELECT last_update FROM services WHERE host_id=1 AND service_id=1
    ${svc_next_check1}    Query    SELECT next_check FROM services WHERE host_id=1 AND service_id=1
    ${svc_latency1}    Query    SELECT latency FROM services WHERE host_id=1 AND service_id=1

    Disconnect From Database

    Ctn Stop Engine
    Ctn Kindly Stop Broker

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    Log To Console    Checking the number of resources
    # We should have 50 hosts + 1000 services
    Check Query Result    SELECT COUNT(*) FROM resources    ==    ${1050}    retry_timeout=30s    retry_pause=2s

    # We check service 1:1 in resources
    Log To Console    Checking the service 1:1 after Broker Stop
    ${output}    Query    SELECT internal_id,type,status,status_ordered,in_downtime,acknowledged,status_confirmed,check_attempts,max_check_attempts,poller_id,severity_id, name, alias, address,parent_name, icon_id, notes_url, notes, action_url, has_graph, notifications_enabled, passive_checks_enabled, active_checks_enabled, last_check_type, enabled, flapping, percent_state_change FROM resources WHERE id=1 AND parent_id=1
    Should Be Equal    "${output}"    "((None, 0, 0, 0, 0, 0, 1, 1, 3, 1, None, 'service_1', None, None, 'host_1', 0, '', '', '', 1, 1, 1, 1, 0, 0, 0, 0.0),)"    Service 1:1 not as expected in resources table (second step)

    # We check host 1 in resources
    Log To Console    Checking the host 1 after Broker Stop
    ${output}    Query    SELECT internal_id,type,status,status_ordered,in_downtime,acknowledged,status_confirmed,check_attempts,max_check_attempts,poller_id,severity_id, name, alias, address,parent_name, icon_id, notes_url, notes, action_url, has_graph, notifications_enabled, passive_checks_enabled, active_checks_enabled, last_check_type, enabled, flapping, percent_state_change FROM resources WHERE id=1 AND parent_id=0
    Should Be Equal    "${output}"    "((None, 1, 0, 0, 0, 0, 1, 1, 3, 1, None, 'host_1', 'host_1', '1.0.0.0', 'host_1', 0, '', '', '', 0, 1, 1, 1, 0, 0, 0, 0.0),)"    Host 1 not as expected in resources table

    # We check service 1:1 in services
    Log To Console    Checking the service 1:1 in services table after Broker Stop
    ${output}    Query    SELECT description, acknowledged, acknowledgement_type, action_url, active_checks, check_attempt, check_freshness, check_interval, check_period, check_type, checked, default_active_checks, default_event_handler_enabled, default_failure_prediction, default_flap_detection, default_notify, default_passive_checks, default_process_perfdata, display_name, enabled, event_handler, event_handler_enabled, failure_prediction, failure_prediction_options, first_notification_delay, flap_detection, flap_detection_on_critical, flap_detection_on_ok, flap_detection_on_unknown, flap_detection_on_warning, flapping, freshness_threshold, high_flap_threshold, icon_image, icon_image_alt, last_hard_state, last_notification, last_time_critical, last_time_unknown, last_time_warning, low_flap_threshold, max_check_attempts, modified_attributes, next_notification, no_more_notifications, notes, notes_url, notification_interval, notification_number, notification_period, notify, notify_on_critical, notify_on_downtime, notify_on_flapping, notify_on_recovery, notify_on_unknown, notify_on_warning, obsess_over_service, passive_checks, percent_state_change, process_perfdata, retain_nonstatus_information, retain_status_information, retry_interval, scheduled_downtime_depth, should_be_scheduled, stalk_on_critical, stalk_on_ok, stalk_on_unknown, stalk_on_warning, state, state_type, volatile, real_state FROM services WHERE host_id=1 AND service_id=1
    Should Be Equal    "${output}"    "(('service_1', 0, 0, '', 1, 1, 0, 5.0, '24x7', 0, 1, 1, 1, None, 1, 1, 1, None, 'service_1', 0, '', 1, None, None, 0.0, 1, 1, 1, 1, 1, 0, 0.0, 0.0, '', '', 0, None, None, None, None, 0.0, 3, None, None, 0, '', '', 0.0, 0, '', 1, 1, 1, 1, 1, 1, 1, 1, 1, 0.0, None, 1, 1, 5.0, 0, 1, 0, 0, 0, 0, 0, 1, 0, None),)"    Service 1:1 not as expected in services table (second step)


    # We check host 1 in hosts
    Log To Console    Checking the host 1 in hosts table after Broker Stop
    ${output}    Query    SELECT host_id, name, instance_id, acknowledged, acknowledgement_type, action_url, active_checks, address, alias, check_attempt, check_command, check_freshness, check_interval, check_period, check_type, checked, command_line, default_active_checks, default_event_handler_enabled, default_failure_prediction, default_flap_detection, default_notify, default_passive_checks, default_process_perfdata, display_name, enabled, event_handler, event_handler_enabled, failure_prediction, first_notification_delay, flap_detection, flap_detection_on_down, flap_detection_on_unreachable, flap_detection_on_up, flapping, freshness_threshold, high_flap_threshold, icon_image, icon_image_alt, last_hard_state, last_hard_state_change, last_notification, last_time_down, last_time_unreachable, low_flap_threshold, max_check_attempts, modified_attributes, next_host_notification, no_more_notifications, notes, notes_url, notification_interval, notification_number, notification_period, notify, notify_on_down, notify_on_downtime, notify_on_flapping, notify_on_recovery, notify_on_unreachable, obsess_over_host, passive_checks, percent_state_change, perfdata, process_perfdata, retain_nonstatus_information, retain_status_information, retry_interval, scheduled_downtime_depth, should_be_scheduled, stalk_on_down, stalk_on_unreachable, stalk_on_up, state, state_type, statusmap_image, timezone, real_state FROM hosts WHERE host_id=1

    Should Be Equal    "${output}"    "((1, 'host_1', 1, 0, 0, '', 1, '1.0.0.0', 'host_1', 1, 'checkh1', 0, 5.0, '24x7', 0, 1, '/tmp/var/lib/centreon-engine/check.pl --id 0', 1, 1, None, 1, 1, 1, None, 'host_1', 0, '', 1, None, 0.0, 1, 1, 1, 1, 0, 0.0, 0.0, '', '', 0, None, None, None, None, 0.0, 3, None, None, 0, '', '', 0.0, 0, '', 1, 1, 1, 1, 1, 1, 1, 1, 0.0, '', None, 1, 1, 1.0, 0, 1, 0, 0, 0, 0, 1, '', '', None),)"    Host 1 not as expected in hosts table

    Log To Console    Checking the poller 1 in instances table after Broker Stop
    ${output}    Query    SELECT instance_id, name, active_host_checks, active_service_checks, address, check_hosts_freshness, check_services_freshness, daemon_mode, description, engine, event_handlers, failure_prediction, flap_detection, global_host_event_handler, global_service_event_handler, last_alive, last_command_check, last_log_rotation, modified_host_attributes, modified_service_attributes, notifications, obsess_over_hosts, obsess_over_services, passive_host_checks, passive_service_checks, process_perfdata, running, deleted, outdated FROM instances
    Should Be Equal    "${output}"    "((1, 'Poller0', None, None, None, None, None, None, None, 'Centreon Engine', None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, 0, 0, 0),)"    Poller 1 not as expected in instances table

    Log To Console    Checking the number of customvariables
    Check Query Result    SELECT COUNT(*) FROM customvariables    ==    ${0}    retry_timeout=30s    retry_pause=2s

    ${svc_state2}    Query    SELECT state FROM services WHERE host_id=1 AND service_id=1
    ${svc_last_update2}    Query    SELECT last_update FROM services WHERE host_id=1 AND service_id=1
    ${svc_next_check2}    Query    SELECT next_check FROM services WHERE host_id=1 AND service_id=1
    ${svc_latency2}    Query    SELECT latency FROM services WHERE host_id=1 AND service_id=1

    Check Query Result    SELECT perfdata FROM services WHERE host_id=1 AND service_id=1    contains    metric=    retry_timeout=30s    retry_pause=2s
    Check Query Result    SELECT output FROM services WHERE host_id=1 AND service_id=1    contains    Test check    retry_timeout=30s    retry_pause=2s
    Check Query Result    SELECT command_line FROM services WHERE host_id=1 AND service_id=1    contains    /tmp/var/lib/centreon-engine/check.pl --id    retry_timeout=30s    retry_pause=2s

    Should Be Equal As Integers    ${svc_state[0][0]}    ${4}    Service 1:1 should be UNKNOWN at startup
    Should Be Equal As Integers    ${svc_state1[0][0]}    ${0}    Service 1:1 should be OK after the first check
    Should Be Equal As Integers    ${svc_state2[0][0]}    ${0}    Service 1:1 should be OK after the second check

    Should Be True    ${svc_last_update[0][0]} > 0    Service 1:1 should have a last update date after the first check
    Should Be True    ${svc_last_update1[0][0]} >= ${svc_last_update[0][0]}    Service 1:1 should have a last value greater or equal to the previous one.
    Should Be True    ${svc_last_update2[0][0]} >= ${svc_last_update1[0][0]}    Service 1:1 should have a last value greater or equal to the previous one.

    Should Be True    ${svc_next_check[0][0]} > ${svc_last_update[0][0]}    Service 1:1 should have a next check date greater than its last update date after the first check
    Should Be True    ${svc_next_check1[0][0]} > ${svc_last_update1[0][0]}    Service 1:1 should have a next check date greater than its last update date (step 1)
    Should Be True    ${svc_next_check2[0][0]} > ${svc_last_update2[0][0]}    Service 1:1 should have a next check date greater than its last update date (step 2)

    Disconnect From Database

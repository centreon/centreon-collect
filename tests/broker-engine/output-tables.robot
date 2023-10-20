*** Settings ***
Documentation       Engine/Broker tests on bbdo_version 3.0.0 and protobuf bbdo embedded events.

Resource            ../resources/resources.robot
Library             Process
Library             String
Library             DateTime
Library             DatabaseLibrary
Library             OperatingSystem
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py
Library             ../resources/specific-duplication.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Test Cases ***
BERES1
    [Documentation]    store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
    [Tags]    broker    engine    protobuf    bbdo
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    sql    trace
    Config Broker Sql Output    central    unified_sql
    Broker Config Output Set    central    central-broker-unified-sql    store_in_resources    yes
    Broker Config Output Set    central    central-broker-unified-sql    store_in_hosts_services    no
    Clear Retention
    ${start}    Get Current Date
    Sleep    1s
    Start Broker
    Start Engine
    ${content_not_present}    Create List
    ...    processing host status event (host:
    ...    UPDATE hosts SET checked=i
    ...    processing service status event (host:
    ...    UPDATE services SET checked=
    ${content_present}    Create List    UPDATE resources SET status=
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content_present}    60
    Should Be True    ${result}    no updates concerning resources available.
    FOR    ${l}    IN    ${content_not_present}
        ${result}    Find In Log    ${centralLog}    ${start}    ${content_not_present}
        Should Not Be True    ${result[0]}    There are updates of hosts/services table(s).
    END
    Stop Engine
    Kindly Stop Broker

BEHS1
    [Documentation]    store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
    [Tags]    broker    engine    protobuf    bbdo
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    sql    trace
    Config Broker Sql Output    central    unified_sql
    Broker Config Output Set    central    central-broker-unified-sql    store_in_resources    no
    Broker Config Output Set    central    central-broker-unified-sql    store_in_hosts_services    yes
    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${content_present}    Create List    UPDATE hosts SET checked=    UPDATE services SET checked=
    ${content_not_present}    Create List
    ...    INSERT INTO resources
    ...    UPDATE resources SET
    ...    UPDATE tags
    ...    INSERT INTO tags
    ...    UPDATE severities
    ...    INSERT INTO severities
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content_present}    60
    Should Be True    ${result}    no updates concerning hosts/services available.
    FOR    ${l}    IN    ${content_not_present}
        ${result}    Find In Log    ${centralLog}    ${start}    ${content_not_present}
        Should Not Be True    ${result[0]}    There are updates of the resources table.
    END
    Stop Engine
    Kindly Stop Broker

BEINSTANCESTATUS
    [Documentation]    Instance status to bdd
    [Tags]    broker    engine
    Config Engine    ${1}    ${50}    ${20}
    Engine Config Set Value    0    enable_flap_detection    1    True
    Engine Config Set Value    0    enable_notifications    0    True
    Engine Config Set Value    0    execute_host_checks    0    True
    Engine Config Set Value    0    execute_service_checks    0    True
    Engine Config Set Value    0    global_host_event_handler    command_1    True
    Engine Config Set Value    0    global_service_event_handler    command_2    True
    Engine Config Set Value    0    instance_heartbeat_interval    1    True
    Engine Config Set Value    0    obsess_over_hosts    1    True
    Engine Config Set Value    0    obsess_over_services    1    True
    Engine Config Set Value    0    accept_passive_host_checks    0    True
    Engine Config Set Value    0    accept_passive_service_checks    0    True

    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    sql    trace
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Config Broker Sql Output    central    unified_sql
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.
    ${result}    check_field_db_value
    ...    SELECT global_host_event_handler FROM instances WHERE instance_id=1
    ...    command_1
    ...    30
    Should Be True    ${result}    global_host_event_handler not updated.
    ${result}    check_field_db_value
    ...    SELECT global_service_event_handler FROM instances WHERE instance_id=1
    ...    command_2
    ...    2
    Should Be True    ${result}    global_service_event_handler not updated.
    ${result}    check_field_db_value    SELECT flap_detection FROM instances WHERE instance_id=1    ${1}    3
    Should Be True    ${result}    flap_detection not updated.
    ${result}    check_field_db_value    SELECT notifications FROM instances WHERE instance_id=1    ${0}    3
    Should Be True    ${result}    notifications not updated.
    ${result}    check_field_db_value    SELECT active_host_checks FROM instances WHERE instance_id=1    ${0}    3
    Should Be True    ${result}    active_host_checks not updated.
    ${result}    check_field_db_value    SELECT active_service_checks FROM instances WHERE instance_id=1    ${0}    3
    Should Be True    ${result}    active_service_checks not updated.
    ${result}    check_field_db_value    SELECT check_hosts_freshness FROM instances WHERE instance_id=1    ${0}    3
    Should Be True    ${result}    check_hosts_freshness not updated.
    ${result}    check_field_db_value
    ...    SELECT check_services_freshness FROM instances WHERE instance_id=1
    ...    ${1}
    ...    3
    Should Be True    ${result}    check_services_freshness not updated.
    ${result}    check_field_db_value    SELECT obsess_over_hosts FROM instances WHERE instance_id=1    ${1}    3
    Should Be True    ${result}    obsess_over_hosts not updated.
    ${result}    check_field_db_value    SELECT obsess_over_services FROM instances WHERE instance_id=1    ${1}    3
    Should Be True    ${result}    obsess_over_services not updated.
    ${result}    check_field_db_value    SELECT passive_host_checks FROM instances WHERE instance_id=1    ${0}    3
    Should Be True    ${result}    passive_host_checks not updated.
    ${result}    check_field_db_value    SELECT passive_service_checks FROM instances WHERE instance_id=1    ${0}    3
    Should Be True    ${result}    passive_service_checks not updated.
    Stop Engine
    Kindly Stop Broker

BEINSTANCE
    [Documentation]    Instance to bdd
    [Tags]    broker    engine
    Config Engine    ${1}    ${50}    ${20}

    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    sql    trace
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Config Broker Sql Output    central    unified_sql
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM instances

    #as GetCurrent Date floor milliseconds to upper or lower integer, we substract 1s
    ${start}    get_round_current_date
    Start Broker
    Start Engine
    ${engine_pid}    Get Engine Pid    e0
    ${result}    check_field_db_value    SELECT pid FROM instances WHERE instance_id=1    ${engine_pid}    30
    Should Be True    ${result}    no correct engine pid in instances table.
    ${result}    check_field_db_value    SELECT engine FROM instances WHERE instance_id=1    Centreon Engine    3
    Should Be True    ${result}    no correct engine in instances table.
    ${result}    check_field_db_value    SELECT running FROM instances WHERE instance_id=1    ${1}    3
    Should Be True    ${result}    no correct running in instances table.
    ${result}    check_field_db_value    SELECT name FROM instances WHERE instance_id=1    Poller0    3
    Should Be True    ${result}    no correct name in instances table.
    ${result}    check_field_db_value    SELECT end_time FROM instances WHERE instance_id=1    ${0}    3
    Should Be True    ${result}    no correct end_time in instances table.
    @{bdd_start_time}    Query    SELECT start_time FROM instances WHERE instance_id=1
    ${now}    get_round_current_date
    Should Be True
    ...    ${start} <= ${bdd_start_time[0][0]} and ${bdd_start_time[0][0]} <= ${now}
    ...    sg=no correct start_time in instances table.

BE_NOTIF_OVERFLOW
    [Documentation]    bbdo 2.0 notification number =40000. make an overflow => notification_number null in db
    [Tags]    broker    engine    protobuf    bbdo
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Broker Config Add Item    module0    bbdo_version    2.0.0
    Broker Config Add Item    central    bbdo_version    2.0.0
    Config Broker Sql Output    central    unified_sql
    Broker Config Log    central    sql    trace
    Broker Config Log    central    perfdata    trace

    Clear Retention

    Start Broker
    Start Engine

    ${start}    Get Current Date
    ${content}    Create List    INITIAL SERVICE STATE: host_16;service_314;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_16 should be raised before we can start our external commands.

    Set Svc Notification Number    host_16    service_314    40000
    Process Service Result Hard    host_16    service_314    2    output critical for 314
    ${result}    Check Service Status With Timeout    host_16    service_314    2    30
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Query
    ...    SELECT s.notification_number FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_16' AND s.description='service_314'
    Should Be True    ${output[0][0]} == None    notification_number is not null

    Stop Engine
    Kindly Stop Broker

BE_TIME_NULL_SERVICE_RESOURCE
    [Documentation]    With BBDO 3, notification_interval time must be set to NULL on 0 in services, hosts and resources tables.
    [Tags]    broker    engine    protobuf    bbdo
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker Sql Output    central    unified_sql
    Config BBDO3    1

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM services
    Execute SQL String    DELETE FROM resources
    Execute SQL String    DELETE FROM hosts

    Clear Retention

    Start Broker
    Start Engine

    FOR    ${index}    IN RANGE    300
        ${output}    Query
        ...    SELECT r.last_status_change, s.last_hard_state_change, s.last_notification, s.next_notification , s.last_state_change, s.last_time_critical, s.last_time_ok, s.last_time_unknown, s.last_time_warning, h.last_hard_state_change, h.last_notification, h.next_host_notification, h.last_state_change, h.last_time_down, h.last_time_unreachable, h.last_time_up FROM services s, resources r, hosts h WHERE h.host_id=1 AND s.service_id=1 AND r.id=1 AND r.parent_id=1
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None),)"
            BREAK
        END
    END
    Should Be Equal As Strings
    ...    ${output}
    ...    ((None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None),)
    Stop Engine
    Kindly Stop Broker

BE_DEFAULT_NOTIFCATION_INTERVAL_IS_ZERO_SERVICE_RESOURCE
    [Documentation]    default notification_interval must be set to NULL in services, hosts and resources tables.
    [Tags]    broker    engine    protobuf    bbdo
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker Sql Output    central    unified_sql
    Config BBDO3    1

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM services
    Execute SQL String    DELETE FROM resources
    Execute SQL String    DELETE FROM hosts

    Clear Retention

    Start Broker
    Start Engine

    FOR    ${index}    IN RANGE    300
        ${output}    Query
        ...    SELECT s.notification_interval, h.notification_interval FROM services s, hosts h WHERE h.host_id=1 AND s.service_id=1
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((0.0, 0.0),)"            BREAK
    END
    Should Be Equal As Strings    ${output}    ((0.0, 0.0),)
    Stop Engine
    Kindly Stop Broker

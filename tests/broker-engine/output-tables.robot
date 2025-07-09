*** Settings ***
Documentation       Engine/Broker tests on bbdo_version 3.0.0 and protobuf bbdo embedded events.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BERES1
    [Documentation]    store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    central-broker-unified-sql    store_in_resources    yes
    Ctn Broker Config Output Set    central    central-broker-unified-sql    store_in_hosts_services    no
    Ctn Clear Retention
    ${start}    Get Current Date
    Sleep    1s
    Ctn Start Broker
    Ctn Start Engine
    ${content_not_present}    Create List
    ...    processing host status event (host:
    ...    UPDATE hosts SET checked=i
    ...    processing service status event (host:
    ...    UPDATE services SET checked=
    ${content_present}    Create List    UPDATE resources SET status=
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content_present}    60
    Should Be True    ${result}    no updates concerning resources available.
    FOR    ${l}    IN    ${content_not_present}
        ${result}    Ctn Find In Log    ${centralLog}    ${start}    ${content_not_present}
        Should Not Be True    ${result[0]}    There are updates of hosts/services table(s).
    END
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEHS1
    [Documentation]    store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    central-broker-unified-sql    store_in_resources    no
    Ctn Broker Config Output Set    central    central-broker-unified-sql    store_in_hosts_services    yes
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content_present}    Create List    UPDATE hosts SET checked=    UPDATE services SET checked=
    ${content_not_present}    Create List
    ...    INSERT INTO resources
    ...    UPDATE resources SET
    ...    UPDATE tags
    ...    INSERT INTO tags
    ...    UPDATE severities
    ...    INSERT INTO severities
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content_present}    60
    Should Be True    ${result}    no updates concerning hosts/services available.
    FOR    ${l}    IN    ${content_not_present}
        ${result}    Ctn Find In Log    ${centralLog}    ${start}    ${content_not_present}
        Should Not Be True    ${result[0]}    There are updates of the resources table.
    END
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEINSTANCESTATUS
    [Documentation]    Instance status to bdd
    [Tags]    broker    engine
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Engine Config Set Value    0    enable_flap_detection    1    True
    Ctn Engine Config Set Value    0    enable_notifications    0    True
    Ctn Engine Config Set Value    0    execute_host_checks    0    True
    Ctn Engine Config Set Value    0    execute_service_checks    0    True
    Ctn Engine Config Set Value    0    global_host_event_handler    command_1    True
    Ctn Engine Config Set Value    0    global_service_event_handler    command_2    True
    Ctn Engine Config Set Value    0    instance_heartbeat_interval    1    True
    Ctn Engine Config Set Value    0    obsess_over_hosts    1    True
    Ctn Engine Config Set Value    0    obsess_over_services    1    True
    Ctn Engine Config Set Value    0    accept_passive_host_checks    0    True
    Ctn Engine Config Set Value    0    accept_passive_service_checks    0    True

    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Config BBDO3    1
    Ctn Config Broker Sql Output    central    unified_sql
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.
    ${result}    Ctn Check Field Db Value
    ...    SELECT global_host_event_handler FROM instances WHERE instance_id=1
    ...    command_1
    ...    30
    Should Be True    ${result}    global_host_event_handler not updated.
    ${result}    Ctn Check Field Db Value
    ...    SELECT global_service_event_handler FROM instances WHERE instance_id=1
    ...    command_2
    ...    2
    Should Be True    ${result}    global_service_event_handler not updated.
    ${result}    Ctn Check Field Db Value    SELECT flap_detection FROM instances WHERE instance_id=1    ${1}    3
    Should Be True    ${result}    flap_detection not updated.
    ${result}    Ctn Check Field Db Value    SELECT notifications FROM instances WHERE instance_id=1    ${0}    3
    Should Be True    ${result}    notifications not updated.
    ${result}    Ctn Check Field Db Value    SELECT active_host_checks FROM instances WHERE instance_id=1    ${0}    3
    Should Be True    ${result}    active_host_checks not updated.
    ${result}    Ctn Check Field Db Value    SELECT active_service_checks FROM instances WHERE instance_id=1    ${0}    3
    Should Be True    ${result}    active_service_checks not updated.
    ${result}    Ctn Check Field Db Value    SELECT check_hosts_freshness FROM instances WHERE instance_id=1    ${0}    3
    Should Be True    ${result}    check_hosts_freshness not updated.
    ${result}    Ctn Check Field Db Value
    ...    SELECT check_services_freshness FROM instances WHERE instance_id=1
    ...    ${1}
    ...    3
    Should Be True    ${result}    check_services_freshness not updated.
    ${result}    Ctn Check Field Db Value    SELECT obsess_over_hosts FROM instances WHERE instance_id=1    ${1}    3
    Should Be True    ${result}    obsess_over_hosts not updated.
    ${result}    Ctn Check Field Db Value    SELECT obsess_over_services FROM instances WHERE instance_id=1    ${1}    3
    Should Be True    ${result}    obsess_over_services not updated.
    ${result}    Ctn Check Field Db Value    SELECT passive_host_checks FROM instances WHERE instance_id=1    ${0}    3
    Should Be True    ${result}    passive_host_checks not updated.
    ${result}    Ctn Check Field Db Value    SELECT passive_service_checks FROM instances WHERE instance_id=1    ${0}    3
    Should Be True    ${result}    passive_service_checks not updated.
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEINSTANCE
    [Documentation]    Instance to bdd
    [Tags]    broker    engine
    Ctn Config Engine    ${1}    ${50}    ${20}

    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Config BBDO3    1
    Ctn Config Broker Sql Output    central    unified_sql
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM instances

    # as GetCurrent Date floor milliseconds to upper or lower integer, we substract 1s
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${engine_pid}    Ctn Get Engine Pid    e0
    ${result}    Ctn Check Field Db Value    SELECT pid FROM instances WHERE instance_id=1    ${engine_pid}    30
    Should Be True    ${result}    no correct engine pid in instances table.
    ${result}    Ctn Check Field Db Value    SELECT engine FROM instances WHERE instance_id=1    Centreon Engine    3
    Should Be True    ${result}    no correct engine in instances table.
    ${result}    Ctn Check Field Db Value    SELECT running FROM instances WHERE instance_id=1    ${1}    3
    Should Be True    ${result}    no correct running in instances table.
    ${result}    Ctn Check Field Db Value    SELECT name FROM instances WHERE instance_id=1    Poller0    3
    Should Be True    ${result}    no correct name in instances table.
    ${result}    Ctn Check Field Db Value    SELECT end_time FROM instances WHERE instance_id=1    ${0}    3
    Should Be True    ${result}    no correct end_time in instances table.
    @{bdd_start_time}    Query    SELECT start_time FROM instances WHERE instance_id=1
    ${now}    Ctn Get Round Current Date
    Should Be True
    ...    ${start} <= ${bdd_start_time[0][0]} and ${bdd_start_time[0][0]} <= ${now}
    ...    sg=no correct start_time in instances table.

BE_NOTIF_OVERFLOW
    [Documentation]    bbdo 2.0 notification number =40000. make an overflow => notification_number null in db
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Add Item    module0    bbdo_version    2.0.0
    Ctn Broker Config Add Item    central    bbdo_version    2.0.0
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    perfdata    trace

    Ctn Clear Retention

    Ctn Start Broker
    Ctn Start Engine

    ${start}    Get Current Date
    ${content}    Create List    INITIAL SERVICE STATE: host_16;service_314;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_16 should be raised before we can start our external commands.

    Ctn Set Svc Notification Number    host_16    service_314    40000
    Ctn Process Service Result Hard    host_16    service_314    2    output critical for 314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    30  HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Query
    ...    SELECT s.notification_number FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_16' AND s.description='service_314'
    Should Be True    ${output[0][0]} == None    notification_number is not null

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BE_TIME_NULL_SERVICE_RESOURCE
    [Documentation]    With BBDO 3, notification_interval time must be set to NULL on 0 in services, hosts and resources tables.
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    1

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM services
    Execute SQL String    DELETE FROM resources
    Execute SQL String    DELETE FROM hosts

    Ctn Clear Retention

    Ctn Start Broker
    Ctn Start Engine

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
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BE_DEFAULT_NOTIFCATION_INTERVAL_IS_ZERO_SERVICE_RESOURCE
    [Documentation]    default notification_interval must be set to NULL in services, hosts and resources tables.
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    1

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM services
    Execute SQL String    DELETE FROM resources
    Execute SQL String    DELETE FROM hosts

    Ctn Clear Retention

    Ctn Start Broker
    Ctn Start Engine

    FOR    ${index}    IN RANGE    300
        ${output}    Query
        ...    SELECT s.notification_interval, h.notification_interval FROM services s, hosts h WHERE h.host_id=1 AND s.service_id=1
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((0.0, 0.0),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((0.0, 0.0),)
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BE_FLAPPING_SERVICE_RESOURCE
    [Documentation]    With BBDO 3, flapping detection must be set in services and resources tables.
    [Tags]    broker    engine    protobuf    MON-154773
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    1
    Ctn Engine Config Set Value    0    enable_flap_detection    1
    Ctn Set Services Passive    ${0}    service_1
    Ctn Engine Config Set Value In Services    0    service_1    flap_detection_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    low_flap_threshold    10
    Ctn Engine Config Set Value In Services    0    service_1    high_flap_threshold    20
    Ctn Engine Config Set Value In Services    0    service_1    flap_detection_options    all

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM services
    Execute SQL String    DELETE FROM resources
    Execute SQL String    DELETE FROM hosts

    Ctn Clear Retention

    Ctn Start Broker    
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

    # generate flapping
    FOR    ${index}    IN RANGE    21
        Ctn Process Service Result Hard    host_1    service_1    2    flapping
        Ctn Process Service Check Result    host_1    service_1    0    flapping
        Sleep    1s
    END

    ${result}    Ctn Check Service Flapping   host_1    service_1    30    5    50
    Should Be True    ${result}   The service or resource (host_1,service_1) is not flapping as expected

    [Teardown]    Ctn Stop Engine Broker And Save Logs    


BE_FLAPPING_HOST_RESOURCE
    [Documentation]    With BBDO 3, flapping detection must be set in hosts and resources tables.
    [Tags]    broker    engine    protobuf    MON-154773
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    1
    Ctn Engine Config Set Value    0    enable_flap_detection    1
    Ctn Set Hosts Passive    ${0}    host_1
    Ctn Engine Config Set Value In Hosts    0    host_1    flap_detection_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    low_flap_threshold    10
    Ctn Engine Config Set Value In Hosts    0    host_1    high_flap_threshold    20
    Ctn Engine Config Set Value In Hosts    0    host_1    flap_detection_options    all
    Ctn Broker Config Log    central    sql    trace

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM services
    Execute SQL String    DELETE FROM resources
    Execute SQL String    DELETE FROM hosts

    Ctn Clear Retention

    Ctn Start Broker    
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

    # generate flapping
    FOR    ${index}    IN RANGE    21
        Ctn Process Host Result Hard    host_1    2    flapping
        Ctn Process Host Check Result    host_1    0    flapping
        Sleep    1s
    END

    ${result}    Ctn Check Host Flapping   host_1    30    5    50
    Should Be True    ${result}   The host or resource host_1 is not flapping as expected

    [Teardown]    Ctn Stop Engine Broker And Save Logs    

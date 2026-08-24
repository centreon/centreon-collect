*** Settings ***
Documentation       Engine/Broker tests on bbdo_version 3.0.0 and protobuf bbdo embedded events.

Resource    ../resources/import.resource

Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Stop Processes
Test Teardown    Ctn Save Logs If Failed


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
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Config BBDO3    1
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
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Config BBDO3    1
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
    Disconnect From Database
    Should Be True
    ...    ${start} <= ${bdd_start_time[0][0]} and ${bdd_start_time[0][0]} <= ${now}
    ...    sg=no correct start_time in instances table.

BE_NOTIF_OVERFLOW
    [Documentation]    bbdo 2.0 notification number =40000. make an overflow => notification_number null in db
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
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
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Query
    ...    SELECT s.notification_number FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_16' AND s.description='service_314'
    Should Be True    ${output[0][0]} == None    notification_number is not null
    Disconnect From Database

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BE_TIME_NULL_SERVICE_RESOURCE
    [Documentation]    With BBDO 3, notification_interval time must be set to NULL on 0 in services, hosts and resources tables.
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
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
    Disconnect From Database
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BE_DEFAULT_NOTIFICATION_INTERVAL_IS_ZERO_SERVICE_RESOURCE
    [Documentation]    default notification_interval must be set to NULL in services, hosts and resources tables.
    [Tags]    broker    engine    protobuf    bbdo
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
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
    Disconnect From Database
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
    Disconnect From Database

    Ctn Clear Retention
    Ctn Clear Logs

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # generate flapping
    FOR    ${index}    IN RANGE    21
        Ctn Process Service Result Hard    host_1    service_1    2    flapping
        Ctn Process Service Check Result    host_1    service_1    0    flapping
        Sleep    1s
    END

    ${result}    Ctn Check Service Flapping    host_1    service_1    30    5    50
    Should Be True    ${result}    The service or resource (host_1,service_1) is not flapping as expected

    [Teardown]    Ctn Stop Engine Broker And Save Logs

BE_FLAPPING_HOST_RESOURCE
    [Documentation]    With BBDO 3, flapping detection must be set in hosts and resources tables.
    [Tags]    broker    engine    protobuf    MON-154773
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
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
    Disconnect From Database

    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # generate flapping
    FOR    ${index}    IN RANGE    21
        Ctn Process Host Result Hard    host_1    2    flapping
        Ctn Process Host Check Result    host_1    0    flapping
        Sleep    1s
    END

    ${result}    Ctn Check Host Flapping    host_1    30    5    50
    Should Be True    ${result}    The host or resource host_1 is not flapping as expected

    [Teardown]    Ctn Stop Engine Broker And Save Logs

BE_FLAPPING_HOST_ADAPTIVE
    [Documentation]    Scenario: re-enabling flap detection on a host updates the flapping flag without waiting for a check
    ...    Given a passive host that flaps, with its flapping flag set in the "hosts" and "resources" tables
    ...    When flap detection is disabled on that host
    ...    Then the flapping flag is cleared in both tables
    ...    When flap detection is enabled again on that host
    ...    Then the flapping flag is set back in both tables, carried by an adaptive host status
    ...    And no check result was needed for that
    [Tags]    broker    engine    protobuf
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
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
    Disconnect From Database

    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # generate flapping
    FOR    ${index}    IN RANGE    21
        Ctn Process Host Result Hard    host_1    2    flapping
        Ctn Process Host Check Result    host_1    0    flapping
        Sleep    1s
    END

    ${result}    Ctn Check Host Flapping    host_1    30    5    50
    Should Be True    ${result}    The host or resource host_1 is not flapping as expected

    # check_for_flapping() only (re)starts flapping on a non-UP state, and the loop
    # above ends on a UP result. Put host_1 back to DOWN before playing with flap
    # detection, otherwise re-enabling it below cannot flap the host again.
    # Waiting for the state in DB also acts as a barrier: passive results are
    # applied by the reaper, asynchronously from the external command processing,
    # so without it the DOWN could land after the DISABLE below.
    Ctn Process Host Result Hard    host_1    2    flapping
    ${result}    Ctn Check Host Status    host_1    ${1}    ${1}    ${True}
    Should Be True    ${result}    host_1 should be DOWN/HARD before toggling flap detection

    # Disabling flap detection clears the flag. This path already republished the
    # whole status before this test existed, so it only sets up the next step.
    Ctn Disable Host Flap Detection    ${0}    host_1
    ${result}    Ctn Check Host Flapping Value    host_1    ${0}    ${30}
    Should Be True    ${result}    The flapping flag of host_1 has not been cleared

    # Re-enabling it makes check_for_flapping() flap the host again. The host is
    # passive and no check result is sent from here on, so an adaptive host
    # status carrying the flapping flag is the only way the DB can learn about
    # it: without it the flag would stay at 0 until the next check.
    Ctn Enable Host Flap Detection    ${0}    host_1
    ${result}    Ctn Check Host Flapping Value    host_1    ${1}    ${30}
    Should Be True    ${result}    The flapping flag of host_1 has not been set back through the adaptive host status

    [Teardown]    Ctn Stop Engine Broker And Save Logs

BE_FLAPPING_SERVICE_ADAPTIVE
    [Documentation]    Scenario: re-enabling flap detection on a service updates the flapping flag without waiting for a check
    ...    Given a passive service that flaps, with its flapping flag set in the "services" and "resources" tables
    ...    When flap detection is disabled on that service
    ...    Then the flapping flag is cleared in both tables
    ...    When flap detection is enabled again on that service
    ...    Then the flapping flag is set back in both tables, carried by an adaptive service status
    ...    And no check result was needed for that
    [Tags]    broker    engine    protobuf
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    Ctn Engine Config Set Value    0    enable_flap_detection    1
    Ctn Set Services Passive    ${0}    service_1
    Ctn Engine Config Set Value In Services    0    service_1    flap_detection_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    low_flap_threshold    10
    Ctn Engine Config Set Value In Services    0    service_1    high_flap_threshold    20
    Ctn Engine Config Set Value In Services    0    service_1    flap_detection_options    all
    Ctn Broker Config Log    central    sql    trace

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM services
    Execute SQL String    DELETE FROM resources
    Execute SQL String    DELETE FROM hosts
    Disconnect From Database

    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # generate flapping
    FOR    ${index}    IN RANGE    21
        Ctn Process Service Result Hard    host_1    service_1    2    flapping
        Ctn Process Service Check Result    host_1    service_1    0    flapping
        Sleep    1s
    END

    ${result}    Ctn Check Service Flapping    host_1    service_1    30    5    50
    Should Be True    ${result}    The service or resource (host_1,service_1) is not flapping as expected

    # check_for_flapping() only (re)starts flapping on a non-OK state, and the loop
    # above ends on an OK result. Put service_1 back to CRITICAL before playing with
    # flap detection, otherwise re-enabling it below cannot flap the service again.
    # Waiting for the state in DB also acts as a barrier: passive results are
    # applied by the reaper, asynchronously from the external command processing,
    # so without it the CRITICAL could land after the DISABLE below.
    Ctn Process Service Result Hard    host_1    service_1    2    flapping
    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${2}    ${30}    HARD
    Should Be True    ${result}    (host_1,service_1) should be CRITICAL/HARD before toggling flap detection

    # Disabling flap detection clears the flag through a full service status, so
    # this step only sets up the next one.
    Ctn Disable Service Flap Detection    ${0}    host_1    service_1
    ${result}    Ctn Check Service Flapping Value    host_1    service_1    ${0}    ${30}
    Should Be True    ${result}    The flapping flag of (host_1,service_1) has not been cleared

    # Re-enabling it makes check_for_flapping() flap the service again. The service
    # is passive and no check result is sent from here on, so an adaptive service
    # status carrying the flapping flag is the only way the DB can learn about it:
    # this path used to republish the whole status, it is now restricted to the
    # flapping attribute alone.
    Ctn Enable Service Flap Detection    ${0}    host_1    service_1
    ${result}    Ctn Check Service Flapping Value    host_1    service_1    ${1}    ${30}
    Should Be True    ${result}    The flapping flag of (host_1,service_1) has not been set back through the adaptive service status

    # no_rrd_test=True: disabling flap detection republishes the whole service status
    # with an unchanged last_check (the service is passive, no check happens in
    # between), so the RRD gets a second status update for that very second and
    # rejects it. Nothing to fix on the test side: the pb_status time IS last_check,
    # so no amount of waiting changes it. Note the adaptive status of the last step
    # does NOT cause this, it never reaches the RRD.
    # The RRD log is then removed: no_rrd_test only skips the check, it does not
    # clean the file, so these expected errors would be found by the next test to
    # run with a standard teardown, and would fail it. Removing it here, after
    # Ctn Save Logs If Failed has run, keeps the log available on failure.
    [Teardown]    Run Keywords
    ...    Ctn Stop Engine Broker And Save Logs    no_rrd_test=True
    ...    AND    Remove File    ${rrdLog}

BE_FLAPPING_GLOBAL_ADAPTIVE
    [Documentation]    Scenario: the program wide flap detection commands update the flapping flag of every object they touch
    ...    Given a passive host that flaps and a passive service of ANOTHER host that flaps, with their flapping flag set in the "hosts", "services" and "resources" tables
    ...    When flap detection is disabled program wide with DISABLE_FLAP_DETECTION
    ...    Then the flapping flag of both objects is cleared
    ...    When flap detection is enabled program wide with ENABLE_FLAP_DETECTION
    ...    Then the flapping flag of both objects is set back, each carried by its own adaptive status
    ...    And no check result was needed for that
    [Tags]    broker    engine    protobuf
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    Ctn Engine Config Set Value    0    enable_flap_detection    1
    # host_1 flaps, and service_21 flaps -- service_21 belongs to host_2, NOT to
    # host_1, and this is mandatory: service::check_for_flapping() only feeds the
    # state history of a service whose host is UP (service.cc:2020). A service of
    # the flapping host would keep a percent_state_change of 0 and never flap.
    # host_2 is left with its active checks, which keep it UP.
    Ctn Set Hosts Passive    ${0}    host_1
    Ctn Set Services Passive    ${0}    service_21
    Ctn Engine Config Set Value In Hosts    0    host_1    flap_detection_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    low_flap_threshold    10
    Ctn Engine Config Set Value In Hosts    0    host_1    high_flap_threshold    20
    Ctn Engine Config Set Value In Hosts    0    host_1    flap_detection_options    all
    Ctn Engine Config Set Value In Services    0    service_21    flap_detection_enabled    1
    Ctn Engine Config Set Value In Services    0    service_21    low_flap_threshold    10
    Ctn Engine Config Set Value In Services    0    service_21    high_flap_threshold    20
    Ctn Engine Config Set Value In Services    0    service_21    flap_detection_options    all
    Ctn Broker Config Log    central    sql    trace

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM services
    Execute SQL String    DELETE FROM resources
    Execute SQL String    DELETE FROM hosts
    Disconnect From Database

    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Generate flapping on host_1 and (host_2,service_21) at the same time. This is
    # the 'Ctn Process * Result Hard' + 'Ctn Process * Check Result' pattern of the
    # other flapping tests, interleaved so both objects share the same seconds
    # instead of doubling the duration of the test.
    FOR    ${index}    IN RANGE    21
        FOR    ${i}    IN RANGE    3
            Ctn Process Host Check Result    host_1    2    flapping
            Ctn Process Service Check Result    host_2    service_21    2    flapping
            Sleep    1s
        END
        Ctn Process Host Check Result    host_1    0    flapping
        Ctn Process Service Check Result    host_2    service_21    0    flapping
        Sleep    1s
    END

    ${result}    Ctn Check Host Flapping    host_1    30    5    50
    Should Be True    ${result}    The host or resource host_1 is not flapping as expected
    ${result}    Ctn Check Service Flapping    host_2    service_21    30    5    50
    Should Be True    ${result}    The service or resource (host_2,service_21) is not flapping as expected

    # check_for_flapping() only (re)starts flapping on a non-UP/non-OK state, and
    # the loop above ends on a UP/OK result. Both objects have to be put back to a
    # problem state before playing with flap detection.
    FOR    ${i}    IN RANGE    3
        Ctn Process Host Check Result    host_1    2    flapping
        Ctn Process Service Check Result    host_2    service_21    2    flapping
        Sleep    1s
    END
    ${result}    Ctn Check Host Status    host_1    ${1}    ${1}    ${True}
    Should Be True    ${result}    host_1 should be DOWN/HARD before toggling flap detection
    ${result}    Ctn Check Service Status With Timeout    host_2    service_21    ${2}    ${30}    HARD
    Should Be True    ${result}    (host_2,service_21) should be CRITICAL/HARD before toggling flap detection

    # The program wide DISABLE walks every host and every service and clears the
    # flag of those that were flapping, through a full status. It only sets up the
    # next step.
    Ctn Disable Flap Detection
    ${result}    Ctn Check Host Flapping Value    host_1    ${0}    ${30}
    Should Be True    ${result}    The flapping flag of host_1 has not been cleared
    ${result}    Ctn Check Service Flapping Value    host_2    service_21    ${0}    ${30}
    Should Be True    ${result}    The flapping flag of (host_2,service_21) has not been cleared

    # The program wide ENABLE calls check_for_flapping() on every object without
    # going through host::enable_flap_detection()/service::enable_flap_detection(),
    # so the adaptive status it sends for each object it just flapped is the only
    # way the DB can learn about it. Both objects are passive: no check result is
    # sent from here on.
    Ctn Enable Flap Detection
    ${result}    Ctn Check Host Flapping Value    host_1    ${1}    ${30}
    Should Be True    ${result}    The flapping flag of host_1 has not been set back through the adaptive host status
    ${result}    Ctn Check Service Flapping Value    host_2    service_21    ${1}    ${30}
    Should Be True    ${result}    The flapping flag of (host_2,service_21) has not been set back through the adaptive service status

    # no_rrd_test=True and Remove File: same reasons as BE_FLAPPING_SERVICE_ADAPTIVE,
    # the full status republished by the DISABLE carries an unchanged last_check, and
    # the expected errors must not be left behind for the next test.
    [Teardown]    Run Keywords
    ...    Ctn Stop Engine Broker And Save Logs    no_rrd_test=True
    ...    AND    Remove File    ${rrdLog}

BE_FLAPPING_SERVICE_STOP_NO_EXTRA_CHECK
    [Documentation]    Scenario: the end of a flapping is published by the check result that caused it
    ...    Given a passive service that flaps
    ...    When stable check results are sent until Engine logs the end of the flapping
    ...    And no further check result is sent
    ...    Then the flapping flag is cleared in the "services" and "resources" tables
    ...    And it did not wait for one more check result to get there
    [Tags]    broker    engine    protobuf
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    Ctn Engine Config Set Value    0    enable_flap_detection    1
    Ctn Set Services Passive    ${0}    service_1
    Ctn Engine Config Set Value In Services    0    service_1    flap_detection_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    low_flap_threshold    10
    Ctn Engine Config Set Value In Services    0    service_1    high_flap_threshold    20
    Ctn Engine Config Set Value In Services    0    service_1    flap_detection_options    all
    Ctn Broker Config Log    central    sql    trace

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM services
    Execute SQL String    DELETE FROM resources
    Execute SQL String    DELETE FROM hosts
    Disconnect From Database

    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Generate flapping and stop as soon as the service really flaps: the lower the
    # percentage of state change when we stop, the fewer stable results are needed
    # below to fall back under the low threshold.
    ${flapping}    Set Variable    ${False}
    FOR    ${index}    IN RANGE    21
        FOR    ${i}    IN RANGE    3
            Ctn Process Service Check Result    host_1    service_1    2    flapping
            Sleep    1s
        END
        Ctn Process Service Check Result    host_1    service_1    0    flapping
        Sleep    1s
        ${flapping}    Ctn Check Service Flapping Value    host_1    service_1    ${1}    ${1}
        IF    ${flapping}    BREAK
    END
    Should Be True    ${flapping}    service_1 never started flapping

    # Stabilize with OK results until Engine logs the end of the flapping. One result
    # at a time, each one acknowledged in DB through its own output before deciding
    # whether another one is needed: sending one result too many would publish a
    # status of its own and hide the very defect this test is about.
    ${flap_start}    Ctn Get Round Current Date
    ${content}    Create List    SERVICE FLAPPING ALERT: host_1;service_1;STOPPED
    ${stopped}    Set Variable    ${False}
    FOR    ${index}    IN RANGE    30
        Ctn Process Service Check Result    host_1    service_1    0    stable_${index}
        ${consumed}    Ctn Check Service Check Status With Timeout
        ...    host_1
        ...    service_1
        ...    ${30}
        ...    ${0}
        ...    ${0}
        ...    stable_${index}
        Should Be True    ${consumed}    the stable_${index} result has not been taken into account
        ${stopped}    ${not_found}    Ctn Find In Log    ${engineLog0}    ${flap_start}    ${content}
        IF    ${stopped}    BREAK
    END
    Should Be True    ${stopped}    service_1 never stopped flapping

    # No check result is sent from here on. If the status published by the check that
    # stopped the flapping did not carry the flag, nothing will ever clear it: this
    # is what happened when check_for_flapping() ran after update_status() in
    # service::handle_async_check_result().
    # A generous timeout is needed here, unlike in the other flapping tests: the flag
    # travels in a regular service status, which unified_sql accumulates in its bulk
    # binds and flushes periodically (measured: ~30s for resources, ~35s for
    # services), whereas an adaptive status runs a direct query and lands in a
    # second.
    ${result}    Ctn Check Service Flapping Value    host_1    service_1    ${0}    ${90}
    Should Be True    ${result}    The flapping flag of (host_1,service_1) has not been cleared by the check result that stopped the flapping

    [Teardown]    Ctn Stop Engine Broker And Save Logs

*** Settings ***
Documentation       Downtime management via Broker gRPC (notification_mode = broker).
...                 These tests are the counterpart of downtimes.robot but with Broker
...                 acting as the downtime authority instead of Engine.

Resource    ../resources/import.resource

Suite Setup    Ctn Clean Downtimes Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Stop Processes
Test Teardown    Ctn Save Logs If Failed


*** Test Cases ***
BEBDTMASS1
    [Documentation]    Scenario: Mass downtime scheduling via Broker gRPC (BBDO3)
    ...    Given 3 pollers with 50 hosts and 20 services each
    ...    When host downtimes are scheduled via Broker gRPC on 50 hosts
    ...    Then 1050 downtimes appear in the database (1 host + 20 services each)
    ...    When all host downtimes are deleted via Broker gRPC
    ...    Then the database contains 0 downtimes
    [Tags]    broker    engine    downtime    broker_downtime
    Ctn Config Engine    ${3}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}
    Ctn Config BBDO3    3
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${3}
    Ctn Wait For Broker Downtime Manager    ${start}

    # Schedule host downtimes via Broker gRPC — each creates 1 host + 20 service downtimes
    # 50 hosts total: poller0=host_1..17, poller1=host_18..34, poller2=host_35..50
    @{dt_ids}    Create List
    FOR    ${i}    IN RANGE    ${17}
        ${id}    Ctn Broker Schedule Host Downtime    host_${i + 1}    ${3600}
        Append To List    ${dt_ids}    ${id}
        ${id}    Ctn Broker Schedule Host Downtime    host_${i + 18}    ${3600}
        Append To List    ${dt_ids}    ${id}
        IF    ${i} < ${16}
            ${id}    Ctn Broker Schedule Host Downtime    host_${i + 35}    ${3600}
            Append To List    ${dt_ids}    ${id}
        END
    END

    ${result}    Ctn Check Number Of Downtimes    ${1050}    ${start}    ${60}
    Should Be True    ${result}    We should have 1050 downtimes enabled.

    # Delete all host downtimes (service downtimes are triggered and deleted in cascade)
    FOR    ${id}    IN    @{dt_ids}
        Ctn Broker Delete Downtime    ${id}
    END

    ${result}    Ctn Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should Be True    ${result}    We should have no downtime enabled.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEBDTSVCFIXED
    [Documentation]    Scenario: Single service downtime via Broker gRPC (BBDO3)
    ...    Given a service downtime is scheduled via Broker gRPC
    ...    Then 1 downtime appears in the database
    ...    When the downtime is deleted via Broker gRPC
    ...    Then the database contains 0 downtimes
    [Tags]    broker    engine    downtime    broker_downtime
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Ctn Wait For Broker Downtime Manager    ${start}

    ${start}    Ctn Get Round Current Date
    ${dt_id}    Ctn Broker Schedule Service Downtime    host_1    service_1    ${3600}

    ${result}    Ctn Check Number Of Downtimes    ${1}    ${start}    ${60}
    Should Be True    ${result}    We should have 1 downtime in DB.

    Ctn Broker Delete Downtime    ${dt_id}

    ${result}    Ctn Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should Be True    ${result}    We should have no downtime in DB.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEBDTSVCFIXED_CHECK_DEPTH
    [Documentation]    Scenario: Service downtime depth via Broker gRPC (BBDO3)
    ...    Given a service downtime is scheduled via Broker gRPC
    ...    Then the service scheduled_downtime_depth is 1 in the database
    ...    When the downtime is deleted
    ...    Then the service scheduled_downtime_depth is 0
    [Tags]    broker    engine    downtime    broker_downtime
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Ctn Wait For Broker Downtime Manager    ${start}

    ${dt_id}    Ctn Broker Schedule Service Downtime    host_1    service_1    ${3600}

    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_1    1    ${60}
    Should Be True    ${result}    The service should be in downtime (depth=1).

    Ctn Broker Delete Downtime    ${dt_id}

    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_1    0    ${60}
    Should Be True    ${result}    The service should no longer be in downtime (depth=0).

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEBDTHOSTFIXED
    [Documentation]    Scenario: Host downtime via Broker gRPC (BBDO3)
    ...    Given a host downtime is scheduled via Broker gRPC
    ...    Then 21 downtimes appear in the database (1 host + 20 services)
    ...    When the host downtime is deleted
    ...    Then the database contains 0 downtimes
    [Tags]    broker    engine    downtime    broker_downtime
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Ctn Wait For Broker Downtime Manager    ${start}

    ${start}    Ctn Get Round Current Date
    ${dt_id}    Ctn Broker Schedule Host Downtime    host_1    ${3600}

    ${result}    Ctn Check Number Of Downtimes    ${21}    ${start}    ${60}
    Should Be True    ${result}    We should have 21 downtimes (1 host + 20 services) in DB.

    Ctn Broker Delete Downtime    ${dt_id}

    ${result}    Ctn Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should Be True    ${result}    We should have no downtime in DB.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEBDTSVCREN
    [Documentation]    Scenario: Service downtime survives service rename (Broker gRPC, BBDO3)
    ...    Given a service downtime is scheduled via Broker gRPC
    ...    When the service is renamed via Engine config reload
    ...    Then the downtime is still active (tracked by ID, not name)
    ...    When the downtime is deleted
    ...    Then the database contains 0 downtimes
    [Tags]    broker    engine    downtime    broker_downtime
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Ctn Wait For Broker Downtime Manager    ${start}

    ${start}    Ctn Get Round Current Date
    ${dt_id}    Ctn Broker Schedule Service Downtime    host_1    service_1    ${3600}

    ${result}    Ctn Check Number Of Downtimes    ${1}    ${start}    ${60}
    Should Be True    ${result}    We should have 1 downtime in DB.

    # Rename the service — the downtime (tracked by service_id) should survive
    Ctn Rename Service    ${0}    host_1    service_1    toto_1
    Ctn Reload Engine

    ${result}    Ctn Check Number Of Downtimes    ${1}    ${start}    ${60}
    Should Be True    ${result}    Downtime should still be active after service rename.

    Ctn Broker Delete Downtime    ${dt_id}

    ${result}    Ctn Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should Be True    ${result}    We should have no downtime in DB after deletion.

    Ctn Stop Engine
    Ctn Kindly Stop Broker


BEBDTIM
    [Documentation]    New services with several pollers are created. Then downtimes are set on all
    ...    configured hosts via Broker gRPC. This results in 5250 downtimes (250 hosts × 21).
    ...    Then all downtimes are removed.
    [Tags]    broker    engine    services    host    downtimes    broker_downtime
    Ctn Config Engine    ${5}    ${250}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${5}
    Ctn Config BBDO3    5
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${5}
    Ctn Wait For Broker Downtime Manager    ${start}

    # Schedule host downtimes via Broker gRPC — 250 hosts × 21 downtimes = 5250
    @{dt_ids}    Create List
    FOR    ${i}    IN RANGE    ${50}
        ${id}    Ctn Broker Schedule Host Downtime    host_${i + 1}      ${3600}
        Append To List    ${dt_ids}    ${id}
        ${id}    Ctn Broker Schedule Host Downtime    host_${i + 51}     ${3600}
        Append To List    ${dt_ids}    ${id}
        ${id}    Ctn Broker Schedule Host Downtime    host_${i + 101}    ${3600}
        Append To List    ${dt_ids}    ${id}
        ${id}    Ctn Broker Schedule Host Downtime    host_${i + 151}    ${3600}
        Append To List    ${dt_ids}    ${id}
        ${id}    Ctn Broker Schedule Host Downtime    host_${i + 201}    ${3600}
        Append To List    ${dt_ids}    ${id}
    END

    ${result}    Ctn Check Number Of Downtimes    ${5250}    ${start}    ${60}
    Should Be True    ${result}    We should have 5250 downtimes enabled.

    FOR    ${id}    IN    @{dt_ids}
        Ctn Broker Delete Downtime    ${id}
    END

    ${result}    Ctn Check Number Of Downtimes    ${0}    ${start}    ${60}
    Should Be True    ${result}    There are still some downtimes enabled.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEBDRRD1
    [Documentation]    A service is forced checked then a downtime is set on this service via
    ...    Broker gRPC. The service is forced checked again and the downtime is removed.
    ...    Then we should not get any error in cbd RRD of kind 'ignored update error in file...'.
    [Tags]    broker    engine    services    protobuf    broker_downtime
    Ctn Clear Logs
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_functions    trace
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    neb    debug
    Ctn Config BBDO3    1
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Ctn Wait For Broker Downtime Manager    ${start}

    Ctn Process Service Check Result With Metrics    host_1    service_1    2    host_1:service_1 is CRITICAL HARD    20
    Sleep    1s
    Ctn Process Service Check Result With Metrics    host_1    service_1    2    host_1:service_1 is CRITICAL HARD    20
    Sleep    1s
    Ctn Process Service Check Result With Metrics    host_1    service_1    2    host_1:service_1 is CRITICAL HARD    20
    Sleep    1s
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    2    ${60}    HARD
    Should Be True    ${result}    The service should be in CRITICAL state HARD.

    ${result}    Grep File    ${rrdLog}    "ignored update error in file"
    Should Be Empty    ${result}
    ...    There should not be any error in cbd RRD of kind 'ignored update error in file...' After step 1.

    ${dt_id}    Ctn Broker Schedule Service Downtime    host_1    service_1    ${3600}
    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_1    1    ${60}
    Should Be True    ${result}    The service should be in downtime.

    ${result}    Grep File    ${rrdLog}    "ignored update error in file"
    Should Be Empty    ${result}
    ...    There should not be any error in cbd RRD of kind 'ignored update error in file...' After step 2.

    Ctn Process Service Check Result With Metrics    host_1    service_1    1    host_1:service_1 is WARNING HARD    20
    Sleep    1s
    Ctn Process Service Check Result With Metrics    host_1    service_1    1    host_1:service_1 is WARNING HARD    20
    Sleep    1s
    Ctn Process Service Check Result With Metrics    host_1    service_1    1    host_1:service_1 is WARNING HARD    20
    Sleep    1s
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    1    ${60}    HARD
    Should Be True    ${result}    The service should be in WARNING state HARD.

    ${result}    Grep File    ${rrdLog}    "ignored update error in file"
    Should Be Empty    ${result}
    ...    There should not be any error in cbd RRD of kind 'ignored update error in file...' After step 3.

    Ctn Broker Delete Downtime    ${dt_id}
    ${result}    Ctn Check Number Of Downtimes    ${0}    ${start}    ${120}
    Should Be True    ${result}    We should have no downtime enabled.

    ${result}    Grep File    ${rrdLog}    "ignored update error in file"
    Should Be Empty    ${result}
    ...    There should not be any error in cbd RRD of kind 'ignored update error in file...' After step 4.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

*** Keywords ***
Ctn Wait For Broker Downtime Manager
    [Documentation]    Wait until Broker has loaded its downtime_manager
    ...    (notification_mode=broker) so the gRPC ScheduleDowntime/DeleteDowntime
    ...    endpoints are usable. Avoids a startup race where a gRPC call would be
    ...    rejected with "Downtime management is not enabled".
    [Arguments]    ${start}    ${timeout}=${60}
    ${content}    Create List    downtime management enabled, downtime manager loaded
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    ${timeout}
    Should Be True    ${result}    Broker did not enable downtime management (notification_mode=broker) in time

Ctn Clean Downtimes Before Suite
    [Documentation]    Run suite setup and clear all downtimes before starting the suite.
    Ctn Clean Before Suite
    Ctn Clear Downtimes

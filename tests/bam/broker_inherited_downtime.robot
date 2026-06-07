*** Settings ***
Documentation       BAM inherited downtimes when Broker is the downtime authority
...                 (notification_mode = broker). The counterpart of
...                 centralized_pb_inherited_downtime.robot, but instead of sending
...                 SCHEDULE_SVC_DOWNTIME / DEL_SVC_DOWNTIME_FULL external commands to
...                 Engine, BAM drives the in-process Broker downtime_manager directly.
...
...                 Test order matters: the only test that issues a gRPC DeleteDowntime
...                 (BECBAMBRKIDT3) runs last, so its gRPC call cannot leak onto a later
...                 test's broker (same gRPC port) and cancel a still-active downtime.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn BAM Broker Downtime Setup
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BECBAMBRKIDT1
    [Documentation]    Given BBDO3 / centralized config with notification_mode = broker
    ...    And a 'worst' BA with one service in critical state
    ...    And a downtime scheduled on the service via Broker gRPC sets an inherited downtime on the BA
    ...    When the KPI service recovers (becomes OK) while still under downtime
    ...    Then BAM removes the inherited downtime from the BA via the Broker downtime_manager
    ...    (the inherited downtime removal is driven by BAM state recomputation, not by a gRPC delete)
    [Tags]    broker    downtime    engine    bam    broker_downtime
    Ctn Config BAM Broker Downtime    ${1}

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}
    Ctn Add Bam Config To Broker    central

    ${cmd_1}    Ctn Get Service Command Id    314
    Ctn Set Command Status    ${cmd_1}    2
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    1

    # KPI set to critical -> BA critical
    Ctn Process Service Result Hard    host_16    service_314    2    output critical for service_314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected

    # Downtime on the KPI via Broker gRPC -> inherited downtime on the BA
    ${dt_id}    Ctn Broker Schedule Service Downtime    host_16    service_314    3600
    ${result}    Ctn Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    The service (host_16, service_314) is not in downtime as it should be
    ${result}    Ctn Check Service Downtime With Timeout    _Module_BAM_1    ba_1    1    60
    Should Be True    ${result}    The BA ba_1 is not in downtime as it should

    # The KPI recovers to OK while still under downtime -> the BA becomes OK
    # -> BAM must remove the inherited downtime (remove branch of _handle_inherited_downtime).
    Ctn Process Service Result Hard    host_16    service_314    0    output ok for service_314
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA ba_1 is not OK as expected after KPI recovery

    ${result}    Ctn Check Service Downtime With Timeout    _Module_BAM_1    ba_1    0    60
    Should Be True    ${result}    The inherited downtime on BA ba_1 must be removed after KPI recovery

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BECBAMBRKIDT2
    [Documentation]    Given BBDO3 / centralized config with notification_mode = broker
    ...    And a 'worst' BA with one service in critical state
    ...    And a downtime scheduled on the service via Broker gRPC sets an inherited downtime on the BA
    ...    When Engine is restarted (Broker stays up and remains the downtime authority)
    ...    Then both the KPI downtime and the inherited downtime are still present
    ...    (Engine, being aware that Broker owns downtimes, does not reset the depth on reload)
    [Tags]    broker    downtime    engine    bam    broker_downtime    start    stop
    Ctn Config BAM Broker Downtime    ${1}

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}
    Ctn Add Bam Config To Broker    central

    ${cmd_1}    Ctn Get Service Command Id    314
    Ctn Set Command Status    ${cmd_1}    2
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    1

    # KPI critical -> BA critical
    Ctn Process Service Result Hard    host_16    service_314    2    output critical for service_314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected

    # Downtime on the KPI via Broker gRPC -> inherited downtime on the BA
    Ctn Broker Schedule Service Downtime    host_16    service_314    3600
    ${result}    Ctn Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    The service (host_16, service_314) is not in downtime as it should be
    ${result}    Ctn Check Service Downtime With Timeout    _Module_BAM_1    ba_1    1    60
    Should Be True    ${result}    The BA ba_1 is not in downtime as it should

    # Engine is restarted; Broker stays up and keeps the downtimes in its manager.
    Ctn Stop Engine
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    1

    # After Engine reload, the depth must still be owned by Broker (Engine omits it).
    ${result}    Ctn Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    The KPI downtime must survive Engine restart
    ${result}    Ctn Check Service Downtime With Timeout    _Module_BAM_1    ba_1    1    60
    Should Be True    ${result}    The inherited downtime on BA ba_1 must survive Engine restart

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BECBAMBRKIDT3
    [Documentation]    Given BBDO3 / centralized config with notification_mode = broker
    ...    And a 'worst' BA with one service in critical state
    ...    And the BA is in critical state because of its service
    ...    When a downtime is scheduled on this service via Broker gRPC
    ...    Then Broker (not Engine) sets an inherited downtime on the BA virtual service
    ...    When the downtime is removed from the service via Broker gRPC
    ...    Then the inherited downtime is removed from the BA
    [Tags]    broker    downtime    engine    bam    broker_downtime
    Ctn Config BAM Broker Downtime    ${1}

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}
    Ctn Add Bam Config To Broker    central

    ${cmd_1}    Ctn Get Service Command Id    314
    Ctn Set Command Status    ${cmd_1}    2
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    1

    # KPI set to critical
    Ctn Process Service Result Hard    host_16    service_314    2    output critical for 314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected

    # A downtime is scheduled on the KPI service via Broker gRPC
    ${dt_id}    Ctn Broker Schedule Service Downtime    host_16    service_314    3600
    ${result}    Ctn Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    The service (host_16, service_314) is not in downtime as it should be

    # Broker propagates an inherited downtime to the BA virtual service
    ${result}    Ctn Check Service Downtime With Timeout    _Module_BAM_1    ba_1    1    60
    Should Be True    ${result}    The BA ba_1 is not in downtime as it should

    # Two downtimes: the one on the KPI and the inherited one on the BA
    ${result}    Ctn Number Of Downtimes Is    2    30
    Should Be True    ${result}    We should have two downtimes (KPI + inherited BA)

    # No external command must have been sent to Engine by BAM
    ${ext}    Grep File    ${engineLog0}    SCHEDULE_SVC_DOWNTIME;_Module_BAM_
    Should Be Empty    ${ext}    BAM must not send SCHEDULE_SVC_DOWNTIME to Engine in broker mode

    # The KPI downtime is removed via Broker gRPC
    Ctn Broker Delete Downtime    ${dt_id}
    ${result}    Ctn Check Service Downtime With Timeout    host_16    service_314    0    60
    Should Be True    ${result}    The service (host_16, service_314) is in downtime and should not.

    # The inherited downtime on the BA must be removed too
    ${result}    Ctn Check Service Downtime With Timeout    _Module_BAM_1    ba_1    0    60
    Should Be True    ${result}    The BA ba_1 is in downtime and should not

    ${result}    Ctn Number Of Downtimes Is    0    60
    Should Be True    ${result}    No downtime should still be running.

    Ctn Stop Engine
    Ctn Kindly Stop Broker


*** Keywords ***
Ctn BAM Broker Downtime Setup
    [Documentation]    Stop processes, clear prot/retention files and reset downtime tables.
    Ctn Stop Processes
    Ctn Clear Prot Files
    Ctn Clear Retention
    Ctn Clear Commands Status
    Ctn Clear Downtimes
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${date}    Get Current Date    result_format=epoch
    Execute SQL String
    ...    UPDATE downtimes SET deletion_time=${date}, actual_end_time=${date} WHERE actual_end_time is null
    Execute SQL String    UPDATE services SET scheduled_downtime_depth=0
    Execute SQL String    UPDATE hosts SET scheduled_downtime_depth=0
    Execute SQL String    UPDATE resources SET in_downtime=0
    Disconnect From Database

Ctn Config BAM Broker Downtime
    [Documentation]    Centralized BBDO3 configuration with Broker as the downtime authority.
    [Arguments]    ${nb_pollers}=${1}
    Ctn Config Centralized Engine    ${nb_pollers}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    downtimes    trace
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    rrd    core    error
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine
    Ctn Notify Broker Of Engine Config Change    ${0}

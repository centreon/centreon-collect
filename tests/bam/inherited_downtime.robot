*** Settings ***
Documentation       Centreon Broker and BAM

Resource            ../resources/resources.robot
Library             Process
Library             DatabaseLibrary
Library             DateTime
Library             OperatingSystem
Library             ../resources/Broker.py
Library             ../resources/Engine.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          BAM Setup
Test Teardown       Save logs If Failed


*** Test Cases ***
BEBAMIDT1
    [Documentation]    A BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. The downtime is removed from the service, the inherited downtime is then deleted.
    [Tags]    broker    downtime    engine    bam
    Clear Commands Status
    Config Broker    module
    Config Broker    central
    Broker Config Log    central    bam    trace
    Config Broker    rrd
    Config Engine    ${1}

    Clone Engine Config To DB
    Add Bam Config To Engine

    @{svc}=    Set Variable    ${{ [("host_16", "service_314")] }}
    Create BA With Services    test    worst    ${svc}
    Add Bam Config To Broker    central
    # Command of service_314 is set to critical
    ${cmd_1}=    Get Command Id    314
    Log To Console    service_314 has command id ${cmd_1}
    Set Command Status    ${cmd_1}    2
    Start Broker
    ${start}=    Get Current Date
    Start Engine
    # Let's wait for the initial service states.
    ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314
    ${result}=    Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    msg=The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    ${result}=    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    msg=The BA ba_1 is not CRITICAL as expected

    # A downtime is put on service_314
    Schedule Service Downtime    host_16    service_314    3600
    ${result}=    Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    msg=The service (host_16, service_314) is not in downtime as it should be
    ${result}=    Check Service Downtime With Timeout    _Module_BAM_1    ba_1    1    60
    Should Be True    ${result}    msg=The BA ba_1 is not in downtime as it should

    # The downtime is deleted
    Delete Service Downtime    host_16    service_314
    ${result}=    Check Service Downtime With Timeout    host_16    service_314    0    60
    Should Be True    ${result}    msg=The service (host_16, service_314) is in downtime and should not.

    ${result}=    Check Downtimes With Timeout    0    60
    Should Be True    ${result}    msg=We should have no more running downtimes

    ${result}=    Check Service Downtime With Timeout    _Module_BAM_1    ba_1    0    60
    Should Be True    ${result}    msg=The BA ba_1 is in downtime as it should not

    Stop Engine
    Kindly Stop Broker

BEBAMIDT2
    [Documentation]    A BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. Engine is restarted. Broker is restarted. The two downtimes are still there with no duplicates. The downtime is removed from the service, the inherited downtime is then deleted.
    [Tags]    broker    downtime    engine    bam    start    stop
    Clear Commands Status
    Config Broker    module
    Config Broker    central
    Broker Config Log    central    bam    trace
    Config Broker    rrd
    Config Engine    ${1}

    Clone Engine Config To DB
    Add Bam Config To Engine

    @{svc}=    Set Variable    ${{ [("host_16", "service_314")] }}
    Create BA With Services    test    worst    ${svc}
    Add Bam Config To Broker    central
    # Command of service_314 is set to critical
    ${cmd_1}=    Get Command Id    314
    Log To Console    service_314 has command id ${cmd_1}
    Set Command Status    ${cmd_1}    2
    Start Broker
    ${start}=    Get Current Date
    Start Engine
    # Let's wait for the initial service states.
    ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314
    ${result}=    Check Service Status With Timeout    host_16    service_314    2    60
    Should Be True    ${result}    msg=The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    ${result}=    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    msg=The BA ba_1 is not CRITICAL as expected

    # A downtime is put on service_314
    Schedule Service Downtime    host_16    service_314    3600
    ${result}=    Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    msg=The service (host_16, service_314) is not in downtime as it should be

    ${result}=    Check Downtimes With Timeout    2    60
    Should Be True    ${result}    msg=We should have one running downtime

    ${result}=    Check Service Downtime With Timeout    _Module_BAM_1    ba_1    1    60
    Should Be True    ${result}    msg=The BA ba_1 is not in downtime as it should

    FOR    ${i}    IN RANGE    2
        # Engine is restarted
        Stop Engine
        ${start}=    Get Current Date
        Start Engine
        # Let's wait for the initial service states.
        ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;
        ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

        # Broker is restarted
        log to console    Broker is stopped (step ${i})
        Kindly Stop Broker
        log to console    Broker is started
        Start Broker
    END

    # There are still two downtimes: the one on the ba and the one on the kpi.
    ${result}=    Check Downtimes With Timeout    2    60
    Should Be True    ${result}    msg=We should have two downtimes

    # The downtime is deleted
    Delete Service Downtime    host_16    service_314
    ${result}=    Check Service Downtime With Timeout    host_16    service_314    0    60
    Should Be True    ${result}    msg=The service (host_16, service_314) is in downtime and should not.
    ${result}=    Check Downtimes With Timeout    0    60
    Should Be True    ${result}    msg=We should have no more downtime

    ${result}=    Check Service Downtime With Timeout    _Module_BAM_1    ba_1    0    60
    Should Be True    ${result}    msg=The BA ba_1 is in downtime as it should not

    log to console    Broker is stopped (end of BEBAMIDT2)
    Stop Engine
    Kindly Stop Broker

BEBAMIGNDT1
    [Documentation]    A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
    [Tags]    broker    downtime    engine    bam
    Clear Commands Status
    Config Broker    module
    Config Broker    central
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    trace
    broker_config_source_log    central    true
    Broker Config Flush Log    module0    0
    Broker Config Log    module0    neb    trace
    Config Broker    rrd
    Config Engine    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Engine Config Set Value    ${0}    log_level_functions    trace
    Engine Config Set Value    ${0}    log_flush_period    0    True

    Clone Engine Config To DB
    Add Bam Config To Engine

    @{svc}=    Set Variable    ${{ [("host_16", "service_313"), ("host_16", "service_314")] }}
    Create BA With Services    test    worst    ${svc}    ignore
    Add Bam Config To Broker    central

    # Command of service_313 is set to ok
    ${cmd_1}=    Get Command Id    313
    Log To Console    service_313 has command id ${cmd_1}
    Set Command Status    ${cmd_1}    0

    # Command of service_314 is set to critical
    ${cmd_2}=    Get Command Id    314
    Log To Console    service_314 has command id ${cmd_2}
    Set Command Status    ${cmd_2}    2

    Start Broker
    ${start}=    Get Round Current Date
    Start Engine
    # Let's wait for the initial service states.
    ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

    # KPI set to ok
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_313    0    output critical for 313
    ${result}=    Check Service Status With Timeout    host_16    service_313    0    60
    Should Be True    ${result}    msg=The service (host_16,service_313) is not OK as expected

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314
    ${result}=    Check Service Status With Timeout    host_16    service_314    2    60
    Should Be True    ${result}    msg=The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    ${result}=    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    msg=The BA ba_1 is not CRITICAL as expected
    Log To console    The BA is critical.

    # Two downtimes are applied on service_314
    Schedule Service Downtime    host_16    service_314    3600
    ${result}=    Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    msg=The service (host_16, service_314) is not in downtime as it should be
    Log to console    One downtime applied to service_314.

    Schedule Service Downtime    host_16    service_314    1800
    ${result}=    Check Service Downtime With Timeout    host_16    service_314    2    60
    Should Be True    ${result}    msg=The service (host_16, service_314) is not in downtime as it should be
    Log to console    Two downtimes applied to service_314.

    ${result}=    Check Service Downtime With Timeout    _Module_BAM_1    ba_1    0    60
    Should Be True    ${result}    msg=The BA ba_1 is in downtime but should not
    Log to console    The BA is configured to ignore kpis in downtime

    ${result}=    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    msg=The service in downtime should be ignored while computing the state of this BA.
    Log to console    The BA is OK, since the critical service is in downtime.

    # The first downtime is deleted
    Delete Service Downtime    host_16    service_314

    ${result}=    Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    msg=The service (host_16, service_314) does not contain 1 downtime as it should
    Log to console    Still one downtime applied to service_314.

    ${result}=    Check Downtimes With Timeout    1    60
    Should Be True    ${result}    msg=We should have one downtime

    ${result}=    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    msg=The BA is not OK whereas the service_314 is still in downtime.
    Log to console    The BA is still OK

    # The second downtime is deleted
    Delete Service Downtime    host_16    service_314

    ${result}=    Check Service Downtime With Timeout    host_16    service_314    0    60
    Should Be True    ${result}    msg=The service (host_16, service_314) does not contain 0 downtime as it should
    Log to console    No more downtime applied to service_314.

    ${result}=    Check Downtimes With Timeout    0    60
    Should Be True    ${result}    msg=We should have no more running downtimes

    ${result}=    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    msg=The critical service is no more in downtime, the BA should be critical.
    Log to console    The BA is now critical (no more downtime)

    Stop Engine
    Kindly Stop Broker

BEBAMIGNDT2
    [Documentation]    A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. The first downtime reaches its end, the BA is still OK, but when the second downtime reaches its end, the BA should be CRITICAL.
    [Tags]    broker    downtime    engine    bam
    Clear Commands Status
    Config Broker    module
    Config Broker    central
    Broker Config Log    central    core    error
    Broker Config Log    central    bam    trace
    Config Broker    rrd
    Config Engine    ${1}

    Clone Engine Config To DB
    Add Bam Config To Engine

    @{svc}=    Set Variable    ${{ [("host_16", "service_313"), ("host_16", "service_314")] }}
    Create BA With Services    test    worst    ${svc}    ignore
    Add Bam Config To Broker    central
    # Command of service_314 is set to critical
    ${cmd_1}=    Get Command Id    313
    Log To Console    service_314 has command id ${cmd_1}
    Set Command Status    ${cmd_1}    0
    ${cmd_2}=    Get Command Id    314
    Log To Console    service_314 has command id ${cmd_2}
    Set Command Status    ${cmd_2}    2
    Start Broker
    ${start}=    Get Current Date
    Start Engine
    # Let's wait for the initial service states.
    ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

    # KPI set to ok
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_313    0    output critical for 313
    ${result}=    Check Service Status With Timeout    host_16    service_313    0    60
    Should Be True    ${result}    msg=The service (host_16,service_313) is not OK as expected

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314
    ${result}=    Check Service Status With Timeout    host_16    service_314    2    60
    Should Be True    ${result}    msg=The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    ${result}=    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    msg=The BA ba_1 is not CRITICAL as expected
    Log To console    The BA is critical.

    # Two downtimes are applied on service_314
    Schedule Service Downtime    host_16    service_314    60
    ${result}=    Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    msg=The service (host_16, service_314) is not in downtime as it should be
    Log to console    One downtime applied to service_314.

    Schedule Service Downtime    host_16    service_314    30
    ${result}=    Check Service Downtime With Timeout    host_16    service_314    2    60
    Should Be True    ${result}    msg=The service (host_16, service_314) is not in downtime as it should be
    Log to console    Two downtimes applied to service_314.

    ${result}=    Check Service Downtime With Timeout    _Module_BAM_1    ba_1    0    60
    Should Be True    ${result}    msg=The BA ba_1 is in downtime but should not
    Log to console    The BA is configured to ignore kpis in downtime

    ${result}=    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    msg=The service in downtime should be ignored while computing the state of this BA.
    Log to console    The BA is OK, since the critical service is in downtime.

    # The first downtime should reach its end

    Log to console    After 30s, the first downtime should be finished.
    ${result}=    Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    msg=The service (host_16, service_314) does not contain 1 downtime as it should
    Log to console    Still one downtime applied to service_314.

    Log to console    After 30s, the second downtime should be finished.
    ${result}=    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    msg=The BA is not OK whereas the service_314 is still in downtime.
    Log to console    The BA is still OK

    # The second downtime finishes
    ${result}=    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    msg=The critical service is no more in downtime, the BA should be critical.
    Log to console    The BA is now critical (no more downtime)

    Stop Engine
    Kindly Stop Broker

BEBAMIGNDTU1
    [Documentation]    A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. The first downtime reaches its end, the BA is still OK, but when the second downtime reaches its end, the BA should be CRITICAL.
    [Tags]    broker    downtime    engine    bam    bbdo
    Clear Commands Status
    Config Broker    module
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    core    error
    Broker Config Log    central    bam    trace
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Config Engine    ${1}

    Clone Engine Config To DB
    Add Bam Config To Engine

    @{svc}=    Set Variable    ${{ [("host_16", "service_313"), ("host_16", "service_314")] }}
    Create BA With Services    test    worst    ${svc}    ignore
    Add Bam Config To Broker    central
    # Command of service_314 is set to critical
    ${cmd_1}=    Get Command Id    313
    Log To Console    service_314 has command id ${cmd_1}
    Set Command Status    ${cmd_1}    0
    ${cmd_2}=    Get Command Id    314
    Log To Console    service_314 has command id ${cmd_2}
    Set Command Status    ${cmd_2}    2
    Start Broker
    ${start}=    Get Current Date
    Start Engine
    # Let's wait for the initial service states.
    ${content}=    Create List    INITIAL SERVICE STATE: host_50;service_1000;
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

    # KPI set to ok
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_313    0    output critical for 313
    ${result}=    Check Service Status With Timeout    host_16    service_313    0    60
    Should Be True    ${result}    msg=The service (host_16,service_313) is not OK as expected

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314
    ${result}=    Check Service Status With Timeout    host_16    service_314    2    60
    Should Be True    ${result}    msg=The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    ${result}=    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    msg=The BA ba_1 is not CRITICAL as expected
    Log To console    The BA is critical.

    # Two downtimes are applied on service_314
    Schedule Service Downtime    host_16    service_314    60
    ${result}=    Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    msg=The service (host_16, service_314) is not in downtime as it should be
    Log to console    One downtime applied to service_314.

    Schedule Service Downtime    host_16    service_314    30
    ${result}=    Check Service Downtime With Timeout    host_16    service_314    2    60
    Should Be True    ${result}    msg=The service (host_16, service_314) is not in downtime as it should be
    Log to console    Two downtimes applied to service_314.

    ${result}=    Check Service Downtime With Timeout    _Module_BAM_1    ba_1    0    60
    Should Be True    ${result}    msg=The BA ba_1 is in downtime but should not
    Log to console    The BA is configured to ignore kpis in downtime

    ${result}=    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    msg=The service in downtime should be ignored while computing the state of this BA.
    Log to console    The BA is OK, since the critical service is in downtime.

    # The first downtime should reach its end

    Log to console    After 30s, the first downtime should be finished.
    ${result}=    Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    msg=The service (host_16, service_314) does not contain 1 downtime as it should
    Log to console    Still one downtime applied to service_314.

    Log to console    After 30s, the second downtime should be finished.
    ${result}=    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    msg=The BA is not OK whereas the service_314 is still in downtime.
    Log to console    The BA is still OK

    # The second downtime finishes
    ${result}=    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    msg=The critical service is no more in downtime, the BA should be critical.
    Log to console    The BA is now critical (no more downtime)

    Stop Engine
    Kindly Stop Broker
    ${lst}=    Create List    1    0    3
    ${result}=    Check Types in resources    ${lst}
    Should Be True    ${result}    msg=The table 'resources' should contain rows of types SERVICE, HOST and BA.


*** Keywords ***
BAM Setup
    Stop Processes
    Clear Retention
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${date}=    Get Current Date    result_format=epoch
    log to console    date=${date}
    Execute SQL String
    ...    UPDATE downtimes SET deletion_time=${date}, actual_end_time=${date} WHERE actual_end_time is null

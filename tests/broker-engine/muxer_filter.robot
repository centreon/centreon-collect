*** Settings ***
Documentation       Centreon Broker and Engine anomaly detection

Resource            ../resources/resources.robot
Library             DateTime
Library             Process
Library             OperatingSystem
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save logs If Failed


*** Test Cases ***
STUPID_FILTER
    [Documentation]    stream connector with a bad configured filter generate a log error message
    [Tags]    broker    engine
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    module    ${1}
    Config Broker    rrd
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Config BBDO3    1
    Broker Config Output Set Json    central    central-broker-unified-sql    filters    {"category": ["bbdo"]}

    ${start}=    Get Current Date
    Start Broker    True
    Start Engine

    ${content}=    Create List    central-broker-unified-sql needs all these categories: bbdo neb
    ${result}=    Find In Log with Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling bad filter should be available.

    Stop Engine
    Kindly Stop Broker    True

FILTER_ON_LUA_CAT
    [Documentation]    stream connector with a bad configured filter generate a log error message
    [Tags]    broker    engine
    Remove File    /tmp/all_lua_event.log

    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    module    ${1}
    Config Broker    rrd
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Config BBDO3    1
    Broker Config Add Lua Output    central    test-filter    ${SCRIPTS}test-log-all-event.lua
    Broker Config Output Set Json    central    test-filter    filters    {"category": [ "storage"]}

    Start Broker    True
    Start Engine

    Wait Until Created    /tmp/all_lua_event.log
    FOR    ${index}    IN RANGE    30
        ${grep_res}=    Grep File    /tmp/all_lua_event.log    "category":3
        Sleep    1s
        IF    len("""${grep_res}""") > 0            BREAK
    END
    Should Not Be Empty    ${grep_res}    msg=no storage event found

    Sleep    5

    ${grep_res}=    Grep File    /tmp/all_lua_event.log    "category":1
    Should Be Empty    ${grep_res}    msg=neb event found

    Stop Engine
    Kindly Stop Broker    True

FILTER_ON_LUA_EVENT
    [Documentation]    stream connector with a bad configured filter generate a log error message
    [Tags]    broker    engine
    Remove File    /tmp/all_lua_event.log

    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    module    ${1}
    Config Broker    rrd
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Config BBDO3    1
    Broker Config Add Lua Output
    ...    central
    ...    test-filter
    ...    ${SCRIPTS}test-log-all-event.lua
    Broker Config Output Set Json
    ...    central
    ...    test-filter
    ...    filters
    ...    {"category": [ "storage:pb_metric_mapping"]}

    Start Broker    True
    Start Engine

    Wait Until Created    /tmp/all_lua_event.log

    FOR    ${index}    IN RANGE    30
        #search for pb_metric_mapping
        ${grep_res}=    Grep File    /tmp/all_lua_event.log    "_type":196620
        Sleep    1s
        IF    len("""${grep_res}""") > 0            BREAK
    END
    Should Not Be Empty    ${grep_res}    msg=no pb_metric_mapping event found
    #wait for some other events
    Sleep    5

    Stop Engine
    Kindly Stop Broker    True

    ${grep_res}=    Grep File    /tmp/all_lua_event.log    "_type":196620
    ${grep_res2}=    Grep File    /tmp/all_lua_event.log    "category"

    Should Be Equal    ${grep_res}    ${grep_res2}

BAM_STREAM_FILTER
    [Documentation]    With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. we watch its events
    [Tags]    broker    engine    bam
    Clear Commands Status
    Config Broker    module    ${1}
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    core    trace
    Config BBDO3    ${1}
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
    Start Broker    True
    ${start}=    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314

    ${result}=    Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    msg=The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    ${result}=    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    msg=The BA ba_1 is not CRITICAL as expected

    #monitoring
    FOR    ${cpt}    IN    RANGE 30
        #pb_service
        ${grep_res1}=    Grep File    ${centralLog}    centreon-bam-monitoring event of type 1001b written
        #pb_service_status
        ${grep_res2}=    Grep File    ${centralLog}    centreon-bam-monitoring event of type 1001d written
        #pb_ba_status
        ${grep_res3}=    Grep File    ${centralLog}    centreon-bam-monitoring event of type 60013 written
        #pb_kpi_status
        ${grep_res4}=    Grep File    ${centralLog}    centreon-bam-monitoring event of type 6001b written

        #reject KpiEvent
        ${grep_res5}=    Grep File    ${centralLog}    centreon-bam-monitoring reject bam:KpiEvent
        #reject storage
        ${grep_res6}=    Grep File    ${centralLog}    centreon-bam-monitoring reject storage

        IF    len("""${grep_res1}""") > 0 and len("""${grep_res2}""") > 0 and len("""${grep_res3}""") > 0 and len("""${grep_res4}""") > 0 and len("""${grep_res5}""") > 0 and len("""${grep_res6}""") > 0
            BREAK
        END
    END

    Should Not Be Empty    ${grep_res1}    msg=no pb_service event
    Should Not Be Empty    ${grep_res2}    msg=no pb_service_status event
    Should Not Be Empty    ${grep_res3}    msg=no pb_ba_status event
    Should Not Be Empty    ${grep_res4}    msg=no pb_kpi_status event
    Should Not Be Empty    ${grep_res5}    msg=no KpiEvent event
    Should Not Be Empty    ${grep_res6}    msg=no storage event rejected

    #reporting
    #pb_ba_event
    ${grep_res}=    Grep File    ${centralLog}    centreon-bam-reporting event of type 60014 written
    Should Not Be Empty    ${grep_res}    msg=no pb_ba_event
    #pb_kpi_event
    ${grep_res}=    Grep File    ${centralLog}    centreon-bam-reporting event of type 60015 written
    Should Not Be Empty    ${grep_res}    msg=no pb_kpi_event
    #reject storage
    ${grep_res}=    Grep File    ${centralLog}    centreon-bam-reporting reject storage
    Should Not Be Empty    ${grep_res}    msg=no rejected storage event
    #reject neb
    ${grep_res}=    Grep File    ${centralLog}    centreon-bam-reporting reject neb
    Should Not Be Empty    ${grep_res}    msg=no rejected neb event

    Stop Engine
    Kindly Stop Broker    True

UNIFIED_SQL_FILTER
    [Documentation]    With bbdo version 3.0.1, we watch events written or rejected in unified_sql
    [Tags]    broker    engine    bam
    Clear Retention
    Config Broker    module    ${1}
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    core    trace
    Config BBDO3    ${1}
    Config Engine    ${1}

    Start Broker    True
    ${start}=    Get Current Date
    Start Engine

    # Let's wait for the external command check start
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.

    # one service set to critical in order to have some events
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314

    ${result}=    Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    msg=The service (host_16,service_314) is not CRITICAL as expected

    #de_pb_service de_pb_service_status de_pb_host de_pb_custom_variable de_pb_log_entry de_pb_host_check
    FOR    ${event}    IN    1001b    1001d    1001e    10025    10029    10027
        ${to_search}=    Catenate    central-broker-unified-sql event of type    ${event}    written
        ${grep_res}=    Grep File    ${centralLog}    ${to_search}
        Should Not Be Empty    ${grep_res}
    END

    Stop Engine
    Kindly Stop Broker    True

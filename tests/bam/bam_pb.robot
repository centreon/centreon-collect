*** Settings ***
Documentation       Centreon Broker and BAM with bbdo version 3.0.1

Resource            ../resources/resources.robot
Library             Process
Library             DatabaseLibrary
Library             DateTime
Library             OperatingSystem
Library             String
Library             ../resources/Broker.py
Library             ../resources/Engine.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          BAM Setup
Test Teardown       Save logs If Failed


*** Test Cases ***
BAPBSTATUS
    [Documentation]    With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. We also check stats output
    [Tags]    broker    downtime    engine    bam
    Clear Commands Status
    Config Broker    module
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    trace
    Config BBDO3    ${1}
    Config Engine    ${1}

    Clone Engine Config To DB
    Add Bam Config To Engine

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Create BA With Services    test    worst    ${svc}
    Add Bam Config To Broker    central
    # Command of service_314 is set to critical
    ${cmd_1}    Get Command Id    314
    Log To Console    service_314 has command id ${cmd_1}
    Set Command Status    ${cmd_1}    2
    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314

    ${result}    Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    ${result}    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Query
    ...    SELECT acknowledged, downtime, in_downtime, current_status FROM mod_bam WHERE name='test'
    Should Be Equal As Strings    ${output}    ((0.0, 0.0, 0, 2),)

    # Little check of the GetBa gRPC command
    ${result}    Run Keyword And Return Status    File Should Exist    /tmp/output
    Run Keyword If    ${result} is True    Remove File    /tmp/output
    Broker Get Ba    51001    1    /tmp/output
    Wait Until Created    /tmp/output
    ${result}    Grep File    /tmp/output    digraph
    Should Not Be Empty    ${result}    /tmp/output does not contain the word 'digraph'

    # check broker stats
    ${res}    Get Broker Stats    central    1: 127.0.0.1:[0-9]+    10    endpoint central-broker-master-input    peers
    Should Be True    ${res}    no central-broker-master-input.peers found in broker stat output

    ${res}    Get Broker Stats    central    listening    10    endpoint central-broker-master-input    state
    Should Be True    ${res}    central-broker-master-input not listening

    ${res}    Get Broker Stats    central    connected    10    endpoint centreon-bam-monitoring    state
    Should Be True    ${res}    central-bam-monitoring not connected

    ${res}    Get Broker Stats    central    connected    10    endpoint centreon-bam-reporting    state
    Should Be True    ${res}    central-bam-reporting not connected

    Reload Engine
    Reload Broker

    # check broker stats
    ${res}    Get Broker Stats    central    1: 127.0.0.1:[0-9]+    10    endpoint central-broker-master-input    peers
    Should Be True    ${res}    no central-broker-master-input.peers found in broker stat output

    ${res}    Get Broker Stats    central    listening    10    endpoint central-broker-master-input    state
    Should Be True    ${res}    central-broker-master-input not listening

    ${res}    Get Broker Stats    central    connected    10    endpoint centreon-bam-monitoring    state
    Should Be True    ${res}    central-bam-monitoring not connected

    ${res}    Get Broker Stats    central    connected    10    endpoint centreon-bam-reporting    state
    Should Be True    ${res}    central-bam-reporting not connected

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BABEST_SERVICE_CRITICAL
    [Documentation]    With bbdo version 3.0.1, a BA of type 'best' with 2 serv, ba is critical only if the 2 services are critical
    [Tags]    broker    engine    bam
    Clear Commands Status
    Clear Retention
    Config Broker    module
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    trace
    Config BBDO3    ${1}
    Config Engine    ${1}

    Clone Engine Config To DB
    Add Bam Config To Engine

    @{svc}    Set Variable    ${{ [("host_16", "service_314"), ("host_16", "service_303")] }}
    Create BA With Services    test    best    ${svc}
    Add Bam Config To Broker    central
    # Command of service_314 is set to critical
    ${cmd_1}    Get Command Id    314
    Log To Console    service_314 has command id ${cmd_1}
    Set Command Status    ${cmd_1}    2
    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314

    ${result}    Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    # The BA should remain OK
    Sleep    2s
    ${result}    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA ba_1 is not OK as expected

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_303    2    output critical for 314

    ${result}    Check Service Status With Timeout    host_16    service_303    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected

    # The BA should become critical
    ${result}    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected

    # KPI set to OK
    Process Service Check Result    host_16    service_314    0    output ok for 314

    ${result}    Check Service Status With Timeout    host_16    service_314    0    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not OK as expected

    # The BA should become OK
    ${result}    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA ba_1 is not OK as expected

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BA_IMPACT_2KPI_SERVICES
    [Documentation]    With bbdo version 3.0.1, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
    [Tags]    broker    engine    bam
    Clear Commands Status
    Clear Retention
    Config Broker    module
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    trace
    Config BBDO3    ${1}
    Config Engine    ${1}
    # This is to avoid parasite status.
    Set Services Passive    ${0}    service_30.

    Clone Engine Config To DB
    Add Bam Config To Engine
    Add Bam Config To Broker    central

    ${id_ba__sid}    create_ba    test    impact    20    35
    Add Service KPI    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Add Service KPI    host_16    service_303    ${id_ba__sid[0]}    40    30    20

    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # service_302 critical service_303 warning => ba warning 30%
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    1
    ...    output warning for service_303
    ${result}    check_service_status_with_timeout    host_16    service_302    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    check_service_status_with_timeout    host_16    service_303    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not WARNING as expected
    ${result}    check_ba_status_with_timeout    test    1    60
    Should Be True    ${result}    The BA ba_1 is not WARNING as expected

    # service_302 critical service_303 critical => ba critical 80%
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    check_service_status_with_timeout    host_16    service_303    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    check_ba_status_with_timeout    test    2    60
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected

    # service_302 ok => ba ok
    Process Service Check Result    host_16    service_302    0    output ok for service_302
    ${result}    check_service_status_with_timeout    host_16    service_302    0    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not OK as expected
    ${result}    check_ba_status_with_timeout    test    0    60
    Should Be True    ${result}    The BA ba_1 is not OK as expected

    # both warning => ba ok
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    1
    ...    output warning for service_302
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    1
    ...    output warning for service_303
    ${result}    check_service_status_with_timeout    host_16    service_302    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not WARNING as expected
    ${result}    check_service_status_with_timeout    host_16    service_303    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not WARNING as expected
    ${result}    check_ba_status_with_timeout    test    0    60
    Should Be True    ${result}    The BA ba_1 is not OK as expected

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BA_RATIO_PERCENT_BA_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio percent' with 2 serv an 1 ba with one service
    [Tags]    broker    engine    bam
    Clear Commands Status
    Clear Retention
    Config Broker    module
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    trace
    Broker Config Source Log    central    1
    Config BBDO3    ${1}
    Config Engine    ${1}
    # This is to avoid parasite status.
    Set Services Passive    ${0}    service_30.

    Clone Engine Config To DB
    Add Bam Config To Engine
    Add Bam Config To Broker    central

    ${id_ba__sid}    create_ba    test    ratio_percent    67    49
    Add Service KPI    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Add Service KPI    host_16    service_303    ${id_ba__sid[0]}    40    30    20

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${id_ba__sid__child}    create_ba_with_services    test_child    worst    ${svc}
    add_ba_kpi    ${id_ba__sid__child[0]}    ${id_ba__sid[0]}    1    2    3

    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # one serv critical => ba ok
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    check_service_status_with_timeout    host_16    service_302    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    sleep    2s
    ${result}    check_ba_status_with_timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected

    # two serv critical => ba warning
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    check_service_status_with_timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    check_service_status_with_timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    check_ba_status_with_timeout    test    1    30
    Should Be True    ${result}    The BA test is not WARNING as expected

    # two serv critical and child ba critical => mother ba critical
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_314
    ...    2
    ...    output critical for service_314
    ${result}    check_service_status_with_timeout    host_16    service_314    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected
    ${result}    check_ba_status_with_timeout    test_child    2    30
    Should Be True    ${result}    The BA test_child is not CRITICAL as expected
    ${result}    check_ba_status_with_timeout    test    2    30
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BA_RATIO_NUMBER_BA_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio number' with 2 services and one ba with 1 service
    [Tags]    broker    engine    bam
    Clear Commands Status
    Clear Retention
    Config Broker    module
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    trace
    Broker Config Source Log    central    1
    Config BBDO3    ${1}
    Config Engine    ${1}
    # This is to avoid parasite status.
    Set Services Passive    ${0}    service_30.

    Clone Engine Config To DB
    Add Bam Config To Engine
    Add Bam Config To Broker    central

    ${id_ba__sid}    create_ba    test    ratio_number    3    2
    Add Service KPI    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Add Service KPI    host_16    service_303    ${id_ba__sid[0]}    40    30    20

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${id_ba__sid__child}    create_ba_with_services    test_child    worst    ${svc}
    Add BA KPI    ${id_ba__sid__child[0]}    ${id_ba__sid[0]}    1    2    3

    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # One service CRITICAL => The BA is still OK
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    check_service_status_with_timeout    host_16    service_302    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected

    ${result}    check_ba_status_with_timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected

    # Two services CRITICAL => The BA passes to WARNING
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    check_service_status_with_timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    check_service_status_with_timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    check_ba_status_with_timeout    test    1    60
    Should Be True    ${result}    The test BA is not in WARNING as expected

    # Two services CRITICAL and also the child BA => The mother BA passes to CRITICAL
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_314
    ...    2
    ...    output critical for service_314
    ${result}    check_service_status_with_timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    check_service_status_with_timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    check_service_status_with_timeout    host_16    service_314    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected
    ${result}    check_ba_status_with_timeout    test_child    2    30
    Should Be True    ${result}    The BA test_child is not CRITICAL as expected
    ${result}    check_ba_status_with_timeout    test    2    60
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BA_BOOL_KPI
    [Documentation]    With bbdo version 3.0.1, a BA of type 'worst' with 1 boolean kpi
    [Tags]    broker    engine    bam
    Clear Commands Status
    Clear Retention
    Config Broker    module
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    trace
    Broker Config Source Log    central    1
    Config BBDO3    ${1}
    Config Engine    ${1}

    Clone Engine Config To DB
    Add Bam Config To Engine
    Add Bam Config To Broker    central

    ${id_ba__sid}    create_ba    test    worst    100    100
    add_boolean_kpi
    ...    ${id_ba__sid[0]}
    ...    {host_16 service_302} {IS} {OK} {OR} ( {host_16 service_303} {IS} {OK} {AND} {host_16 service_314} {NOT} {UNKNOWN} )
    ...    False
    ...    100

    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # 302 warning and 303 critical    => ba critical
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    1
    ...    output warning for service_302
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    Process Service Check Result    host_16    service_314    0    output OK for service_314
    ${result}    check_service_status_with_timeout    host_16    service_302    1    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not WARNING as expected
    ${result}    check_service_status_with_timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    check_service_status_with_timeout    host_16    service_314    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not OK as expected

#    schedule_forced_svc_check    _Module_BAM_1    ba_1
    ${result}    check_ba_status_with_timeout    test    2    30
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BEPB_DIMENSION_BV_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_bv_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Clear Commands Status
    Clear Retention

    Remove File    /tmp/all_lua_event.log
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config BBDO3    ${1}
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    trace
    Config Broker Sql Output    central    unified_sql

    Clone Engine Config To DB
    Add Bam Config To Broker    central

    Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-log-all-event.lua

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM mod_bam_ba_groups
    Execute SQL String
    ...    INSERT INTO mod_bam_ba_groups (id_ba_group, ba_group_name, ba_group_description) VALUES (574, 'virsgtr', 'description_grtmxzo')

    Start Broker    True
    Start Engine
    Wait Until Created    /tmp/all_lua_event.log    30s
    FOR    ${index}    IN RANGE    10
        ${grep_res}    Grep File
        ...    /tmp/all_lua_event.log
        ...    "_type":393238, "category":6, "element":22, "bv_id":574, "bv_name":"virsgtr", "bv_description":"description_grtmxzo"
        Sleep    1s
        IF    len("""${grep_res}""") > 0    BREAK
    END

    Should Not Be Empty    ${grep_res}    event not found

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    ${True}

BEPB_DIMENSION_BA_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_ba_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Clear Commands Status
    Clear Retention

    Remove File    /tmp/all_lua_event.log
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config BBDO3    ${1}
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    trace
    Config Broker Sql Output    central    unified_sql

    Clone Engine Config To DB
    Add Bam Config To Broker    central
    Add Bam Config To Engine

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Create BA With Services    test    worst    ${svc}

    Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-log-all-event.lua

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    SET FOREIGN_KEY_CHECKS=0
    Execute SQL String
    ...    UPDATE mod_bam set description='fdpgvo75', sla_month_percent_warn=1.23, sla_month_percent_crit=4.56, sla_month_duration_warn=852, sla_month_duration_crit=789, id_reporting_period=741

    Start Broker    True
    Start Engine
    Wait Until Created    /tmp/all_lua_event.log    30s
    FOR    ${index}    IN RANGE    10
        ${grep_res}    Grep File
        ...    /tmp/all_lua_event.log
        ...    "_type":393241, "category":6, "element":25, "ba_id":1, "ba_name":"test", "ba_description":"fdpgvo75", "sla_month_percent_crit":4.56, "sla_month_percent_warn":1.23, "sla_duration_crit":789, "sla_duration_warn":852
        Sleep    1s
        IF    len("""${grep_res}""") > 0    BREAK
    END

    Should Not Be Empty    ${grep_res}    event not found

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    ${True}

BEPB_DIMENSION_BA_BV_RELATION_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_ba_bv_relation_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Clear Commands Status
    Clear Retention

    Remove File    /tmp/all_lua_event.log
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config BBDO3    ${1}
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    trace
    Config Broker Sql Output    central    unified_sql

    Clear Db    mod_bam_reporting_relations_ba_bv
    Clone Engine Config To DB
    Add Bam Config To Engine
    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Create BA With Services    test    worst    ${svc}

    Add Bam Config To Broker    central

    Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-log-all-event.lua

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Delete All Rows From Table    mod_bam_bagroup_ba_relation
    Execute SQL String    INSERT INTO mod_bam_bagroup_ba_relation (id_ba, id_ba_group) VALUES (1, 456)

    Start Broker    True
    Start Engine
    Wait Until Created    /tmp/all_lua_event.log    30s
    FOR    ${index}    IN RANGE    10
        ${grep_res}    Grep File
        ...    /tmp/all_lua_event.log
        ...    "_type":393239, "category":6, "element":23, "ba_id":1, "bv_id":456
        Sleep    1s
        IF    len("""${grep_res}""") > 0    BREAK
    END

    Should Not Be Empty    ${grep_res}    event not found

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    @{query_results}    Query    SELECT bv_id FROM mod_bam_reporting_relations_ba_bv WHERE bv_id=456 and ba_id=1

    Should Be True    len(@{query_results}) >= 1    We should have one line in mod_bam_reporting_relations_ba_bv table

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    ${True}

BEPB_DIMENSION_TIMEPERIOD
    [Documentation]    use of pb_dimension_timeperiod message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Clear Commands Status
    Clear Retention

    Remove File    /tmp/all_lua_event.log
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config BBDO3    ${1}
    Broker Config Log    central    bam    trace
    Broker Config Log    central    lua    trace
    Broker Config Log    central    core    trace
    broker_config_source_log    central    1
    Config Broker Sql Output    central    unified_sql

    Clone Engine Config To DB
    # Add Bam Config To Engine
    Add Bam Config To Broker    central

    Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-log-all-event.lua

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String
    ...    INSERT INTO timeperiod (tp_id, tp_name, tp_sunday, tp_monday, tp_tuesday, tp_wednesday, tp_thursday, tp_friday, tp_saturday) VALUES (732, "ezizae", "sunday_value", "monday_value", "tuesday_value", "wednesday_value", "thursday_value", "friday_value", "saturday_value")

    Start Broker    True
    Start Engine
    Wait Until Created    /tmp/all_lua_event.log    30s
    FOR    ${index}    IN RANGE    10
        ${grep_res}    Grep File
        ...    /tmp/all_lua_event.log
        ...    "_type":393240, "category":6, "element":24, "id":732, "name":"ezizae", "monday":"monday_value", "tuesday":"tuesday_value", "wednesday":"wednesday_value", "thursday":"thursday_value", "friday":"friday_value", "saturday":"saturday_value", "sunday":"sunday_value"
        Sleep    1s
        IF    len("""${grep_res}""") > 0    BREAK
    END

    Should Not Be Empty    ${grep_res}    event not found

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    ${True}

BEPB_DIMENSION_KPI_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_kpi_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Clear Commands Status
    Clear Retention

    Remove File    /tmp/all_lua_event.log
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config BBDO3    ${1}
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    trace
    Config Broker Sql Output    central    unified_sql
    broker_config_source_log    central    1

    Clone Engine Config To DB
    Add Bam Config To Broker    central
    Add Bam Config To Engine

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${baid_svcid}    create_ba_with_services    test    worst    ${svc}

    add_boolean_kpi    ${baid_svcid[0]}    {host_16 service_302} {IS} {OK}    False    100

    Start Broker    True
    Start Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${expected}    Catenate    (('bool test',    ${baid_svcid[0]}
    ${expected}    Catenate
    ...    SEPARATOR=
    ...    ${expected}
    ...    , 'test', 0, '', 0, '', 1, 'bool test'), ('host_16 service_314',
    ${expected}    Catenate    ${expected}    ${baid_svcid[0]}
    ${expected}    Catenate    SEPARATOR=    ${expected}    , 'test', 16, 'host_16', 314, 'service_314', 0, ''))
    FOR    ${index}    IN RANGE    10
        ${output}    Query
        ...    SELECT kpi_name, ba_id, ba_name, host_id, host_name, service_id, service_description, boolean_id, boolean_name FROM mod_bam_reporting_kpi order by kpi_name
        Sleep    1s
        IF    ${output} == ${expected}    BREAK
    END

    Should Be Equal As Strings    ${output}    ${expected}    mod_bam_reporting_kpi not filled

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    ${True}

BEPB_KPI_STATUS
    [Documentation]    bbdo_version 3 use kpi_status message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Clear Commands Status
    Clear Retention

    Remove File    /tmp/all_lua_event.log
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config BBDO3    ${1}
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    trace
    Config Broker Sql Output    central    unified_sql
    broker_config_source_log    central    1

    Clone Engine Config To DB
    Add Bam Config To Broker    central
    Add Bam Config To Engine

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    create_ba_with_services    test    worst    ${svc}

    Start Broker    True
    Start Engine

    ${start}    Get Current Date    result_format=epoch

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314
    ${result}    Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    10
        ${output}    Query    SELECT current_status, state_type FROM mod_bam_kpi WHERE host_id=16 and service_id= 314
        Sleep    1s
        IF    ${output} == ((2, '1'),)    BREAK
    END

    Should Be Equal As Strings    ${output}    ((2, '1'),)    mod_bam_kpi not filled

    ${output}    Query    SELECT last_state_change FROM mod_bam_kpi WHERE host_id=16 and service_id= 314
    ${output}    Fetch From Right    "${output}"    (
    ${output}    Fetch From Left    ${output}    ,

    Should Be True    (${output} + 0.999) >= ${start}

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    ${True}

BEPB_BA_DURATION_EVENT
    [Documentation]    use of pb_ba_duration_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Clear Commands Status
    Clear Retention

    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config BBDO3    ${1}
    Broker Config Log    central    bam    trace
    Broker Config Log    central    core    trace
    broker_config_source_log    central    1
    Config Broker Sql Output    central    unified_sql

    Clone Engine Config To DB
    Add Bam Config To Broker    central
    Add Bam Config To Engine

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    create_ba_with_services    test    worst    ${svc}

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String
    ...    INSERT INTO timeperiod (tp_id, tp_name, tp_sunday, tp_monday, tp_tuesday, tp_wednesday, tp_thursday, tp_friday, tp_saturday) VALUES (1, "ezizae", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59")
    Execute SQL String    DELETE FROM mod_bam_relations_ba_timeperiods

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM mod_bam_reporting_ba_events_durations

    Start Broker    True
    Start Engine

    # KPI set to critical
    # as GetCurrent Date floor milliseconds to upper or lower integer, we substract 1s
    ${start_event}    get_round_current_date
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314
    ${result}    Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected
    Sleep    2s
    Process Service Check Result    host_16    service_314    0    output ok for 314
    ${result}    Check Service Status With Timeout    host_16    service_314    0    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not OK as expected
    ${end_event}    Get Current Date    result_format=epoch

    FOR    ${index}    IN RANGE    10
        ${output}    Query
        ...    SELECT start_time, end_time, duration, sla_duration, timeperiod_is_default FROM mod_bam_reporting_ba_events_durations WHERE ba_event_id = 1
        Sleep    1s
        IF    ${output} and len(${output[0]}) >= 5    BREAK
    END

    Should Be True    ${output[0][2]} == ${output[0][1]} - ${output[0][0]}
    Should Be True    ${output[0][3]} == ${output[0][1]} - ${output[0][0]}
    Should Be True    ${output[0][4]} == 1
    Should Be True    ${output[0][1]} > ${output[0][0]}
    Should Be True    ${output[0][0]} >= ${start_event}
    Should Be True    ${output[0][1]} <= ${end_event}

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    ${True}

BEPB_DIMENSION_BA_TIMEPERIOD_RELATION
    [Documentation]    use of pb_dimension_ba_timeperiod_relation message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Clear Commands Status
    Clear Retention

    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config BBDO3    ${1}
    Broker Config Log    central    bam    trace
    Broker Config Log    central    core    trace
    broker_config_source_log    central    1
    Config Broker Sql Output    central    unified_sql

    Clone Engine Config To DB
    Add Bam Config To Broker    central
    Add Bam Config To Engine

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    create_ba_with_services    test    worst    ${svc}

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String
    ...    INSERT INTO timeperiod (tp_id, tp_name, tp_sunday, tp_monday, tp_tuesday, tp_wednesday, tp_thursday, tp_friday, tp_saturday) VALUES (732, "ezizae", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59")
    Execute SQL String    DELETE FROM mod_bam_relations_ba_timeperiods
    Execute SQL String    INSERT INTO mod_bam_relations_ba_timeperiods (ba_id, tp_id) VALUES (1,732)

    Start Broker    True
    Start Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    10
        ${output}    Query
        ...    SELECT ba_id FROM mod_bam_reporting_relations_ba_timeperiods WHERE ba_id=1 and timeperiod_id=732 and is_default=0
        Sleep    1s
        IF    len("""${output}""") > 5    BREAK
    END

    Should Be True
    ...    len("""${output}""") > 5
    ...    "centreon_storage.mod_bam_reporting_relations_ba_timeperiods not updated"

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    ${True}

BEPB_DIMENSION_TRUNCATE_TABLE
    [Documentation]    use of pb_dimension_timeperiod message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Clear Commands Status
    Clear Retention

    Remove File    /tmp/all_lua_event.log
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config BBDO3    ${1}
    Broker Config Log    central    bam    trace
    Broker Config Log    central    lua    trace
    Broker Config Log    central    core    trace
    broker_config_source_log    central    1
    Config Broker Sql Output    central    unified_sql

    Clone Engine Config To DB
    Add Bam Config To Broker    central

    Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-log-all-event.lua

    Start Broker    True
    Start Engine
    Wait Until Created    /tmp/all_lua_event.log    30s
    FOR    ${index}    IN RANGE    10
        ${grep_res}    Grep File
        ...    /tmp/all_lua_event.log
        ...    "_type":393246, "category":6, "element":30, "update_started":true
        Sleep    1s
        IF    len("""${grep_res}""") > 0    BREAK
    END

    Should Not Be Empty    ${grep_res}    event not found
    ${grep_res}    Grep File
    ...    /tmp/all_lua_event.log
    ...    "_type":393246, "category":6, "element":30, "update_started":false
    Should Not Be Empty    ${grep_res}    event not found

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    ${True}

BA_RATIO_NUMBER_BA_4_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
    [Tags]    broker    engine    bam
    Clear Commands Status
    Clear Retention
    Config Broker    module
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    trace
    Broker Config Source Log    central    1
    Config BBDO3    ${1}
    Config Engine    ${1}
    # This is to avoid parasite status.
    Set Services Passive    ${0}    service_30.

    Clone Engine Config To DB
    Add Bam Config To Engine
    Add Bam Config To Broker    central

    ${id_ba__sid}    create_ba    test    ratio_number    2    1
    Add Service KPI    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Add Service KPI    host_16    service_303    ${id_ba__sid[0]}    40    30    20
    Add Service KPI    host_16    service_304    ${id_ba__sid[0]}    40    30    20
    Add Service KPI    host_16    service_304    ${id_ba__sid[0]}    40    30    20

    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # all serv ok => ba ok
    ${result}    check_ba_status_with_timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected

    # one serv critical => ba warning
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    check_service_status_with_timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    check_ba_status_with_timeout    test    1    30
    Should Be True    ${result}    The BA test is not WARNING as expected

    # two services critical => ba ok
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    check_service_status_with_timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    check_ba_status_with_timeout    test    2    30
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    # all serv ok => ba ok
    Process Service Check Result    host_16    service_302    0    output ok for service_302
    ${result}    check_service_status_with_timeout    host_16    service_302    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not OK as expected
    Process Service Check Result    host_16    service_303    0    output ok for service_303
    ${result}    check_service_status_with_timeout    host_16    service_303    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not OK as expected
    ${result}    check_ba_status_with_timeout    test    0    30
    Should Be True    ${result}    The BA test is not OK as expected

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BA_RATIO_PERCENT_BA_4_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
    [Tags]    broker    engine    bam
    Clear Commands Status
    Clear Retention
    Config Broker    module
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    trace
    Broker Config Source Log    central    1
    Config BBDO3    ${1}
    Config Engine    ${1}
    # This is to avoid parasite status.
    Set Services Passive    ${0}    service_30.

    Clone Engine Config To DB
    Add Bam Config To Engine
    Add Bam Config To Broker    central

    ${id_ba__sid}    create_ba    test    ratio_percent    50    25
    Add Service KPI    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Add Service KPI    host_16    service_303    ${id_ba__sid[0]}    40    30    20
    Add Service KPI    host_16    service_304    ${id_ba__sid[0]}    40    30    20
    Add Service KPI    host_16    service_305    ${id_ba__sid[0]}    40    30    20

    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # all serv ok => ba ok
    ${result}    check_ba_status_with_timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected

    # one serv critical => ba warning
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    check_service_status_with_timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    check_ba_status_with_timeout    test    1    30
    Should Be True    ${result}    The BA test is not WARNING as expected

    # two services critical => ba ok
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    check_service_status_with_timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    check_ba_status_with_timeout    test    2    30
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    # all serv ok => ba ok
    Process Service Check Result    host_16    service_302    0    output ok for service_302
    ${result}    check_service_status_with_timeout    host_16    service_302    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not OK as expected
    Process Service Check Result    host_16    service_303    0    output ok for service_303
    ${result}    check_service_status_with_timeout    host_16    service_303    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not OK as expected
    ${result}    check_ba_status_with_timeout    test    0    30
    Should Be True    ${result}    The BA test is not OK as expected

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker


*** Keywords ***
BAM Setup
    Stop Processes
    Connect To Database    pymysql    ${DBName}    ${DBUserRoot}    ${DBPassRoot}    ${DBHost}    ${DBPort}
    Execute SQL String    SET GLOBAL FOREIGN_KEY_CHECKS=0
    Execute SQL String    DELETE FROM mod_bam_reporting_kpi
    Execute SQL String    DELETE FROM mod_bam_reporting_timeperiods
    Execute SQL String    DELETE FROM mod_bam_reporting_relations_ba_timeperiods
    Execute SQL String    DELETE FROM mod_bam_reporting_ba_events
    Execute SQL String    ALTER TABLE mod_bam_reporting_ba_events AUTO_INCREMENT = 1
    Execute SQL String    SET GLOBAL FOREIGN_KEY_CHECKS=1

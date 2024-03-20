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

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn BAM Setup
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BAPBSTATUS
    [Documentation]    With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. We also check stats output
    [Tags]    broker    downtime    engine    bam
    Ctn Clear Commands Status
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Config BBDO3    ${1}
    Ctn Config Engine    ${1}

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}
    Ctn Add Bam Config To Broker    central
    # Command of service_314 is set to critical
    ${cmd_1}    Ctn Get Command Id    314
    Log To Console    service_314 has command id ${cmd_1}
    Ctn Set Command Status    ${cmd_1}    2
    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # KPI set to critical
    Repeat Keyword    3 times    Ctn Process Service Check Result    host_16    service_314    2    output critical for 314

    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Query
    ...    SELECT acknowledged, downtime, in_downtime, current_status FROM mod_bam WHERE name='test'
    Should Be Equal As Strings    ${output}    ((0.0, 0.0, 0, 2),)

    # Little check of the GetBa gRPC command
    ${result}    Run Keyword And Return Status    File Should Exist    /tmp/output
    Run Keyword If    ${result} is True    Remove File    /tmp/output
    Ctn Broker Get Ba    51001    1    /tmp/output
    Wait Until Created    /tmp/output
    ${result}    Grep File    /tmp/output    digraph
    Should Not Be Empty    ${result}    /tmp/output does not contain the word 'digraph'

    # check broker stats
    ${res}    Ctn Get Broker Stats    central    1: 127.0.0.1:[0-9]+    10    endpoint central-broker-master-input    peers
    Should Be True    ${res}    no central-broker-master-input.peers found in broker stat output

    ${res}    Ctn Get Broker Stats    central    listening    10    endpoint central-broker-master-input    state
    Should Be True    ${res}    central-broker-master-input not listening

    ${res}    Ctn Get Broker Stats    central    connected    10    endpoint centreon-bam-monitoring    state
    Should Be True    ${res}    central-bam-monitoring not connected

    ${res}    Ctn Get Broker Stats    central    connected    10    endpoint centreon-bam-reporting    state
    Should Be True    ${res}    central-bam-reporting not connected

    Ctn Reload Engine
    Ctn Reload Broker

    # check broker stats
    ${res}    Ctn Get Broker Stats    central    1: 127.0.0.1:[0-9]+    10    endpoint central-broker-master-input    peers
    Should Be True    ${res}    no central-broker-master-input.peers found in broker stat output

    ${res}    Ctn Get Broker Stats    central    listening    10    endpoint central-broker-master-input    state
    Should Be True    ${res}    central-broker-master-input not listening

    ${res}    Ctn Get Broker Stats    central    connected    10    endpoint centreon-bam-monitoring    state
    Should Be True    ${res}    central-bam-monitoring not connected

    ${res}    Ctn Get Broker Stats    central    connected    10    endpoint centreon-bam-reporting    state
    Should Be True    ${res}    central-bam-reporting not connected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

BABEST_SERVICE_CRITICAL
    [Documentation]    With bbdo version 3.0.1, a BA of type 'best' with 2 serv, ba is critical only if the 2 services are critical
    [Tags]    broker    engine    bam
    Ctn Clear Commands Status
    Ctn Clear Retention
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Config BBDO3    ${1}
    Ctn Config Engine    ${1}

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine

    @{svc}    Set Variable    ${{ [("host_16", "service_314"), ("host_16", "service_303")] }}
    Ctn Create Ba With Services    test    best    ${svc}
    Ctn Add Bam Config To Broker    central
    # Command of service_314 is set to critical
    ${cmd_1}    Ctn Get Command Id    314
    Log To Console    service_314 has command id ${cmd_1}
    Ctn Set Command Status    ${cmd_1}    2
    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # KPI set to critical
    Repeat Keyword    3 times    Ctn Process Service Check Result    host_16    service_314    2    output critical for 314

    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    # The BA should remain OK
    Sleep    2s
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA ba_1 is not OK as expected

    # KPI set to critical
    Repeat Keyword    3 times    Ctn Process Service Check Result    host_16    service_303    2    output critical for 314

    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected

    # The BA should become critical
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected

    # KPI set to OK
    Ctn Process Service Check Result    host_16    service_314    0    output ok for 314

    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    0    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not OK as expected

    # The BA should become OK
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA ba_1 is not OK as expected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

BA_IMPACT_2KPI_SERVICES
    [Documentation]    With bbdo version 3.0.1, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
    [Tags]    broker    engine    bam
    Ctn Clear Commands Status
    Ctn Clear Retention
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Config BBDO3    ${1}
    Ctn Config Engine    ${1}
    # This is to avoid parasite status.
    Ctn Set Services Passive    ${0}    service_30.

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine
    Ctn Add Bam Config To Broker    central

    ${id_ba__sid}    Ctn Create Ba    test    impact    20    35
    Ctn Add Service Kpi    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_303    ${id_ba__sid[0]}    40    30    20

    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # service_302 critical service_303 warning => ba warning 30%
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_303
    ...    1
    ...    output warning for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not WARNING as expected
    ${result}    Ctn Check Ba Status With Timeout    test    1    60
    Should Be True    ${result}    The BA ba_1 is not WARNING as expected

    # service_302 critical service_303 critical => ba critical 80%
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected

    # service_302 ok => ba ok
    Ctn Process Service Check Result    host_16    service_302    0    output ok for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    0    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not OK as expected
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA ba_1 is not OK as expected

    # both warning => ba ok
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_302
    ...    1
    ...    output warning for service_302
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_303
    ...    1
    ...    output warning for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not WARNING as expected
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not WARNING as expected
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA ba_1 is not OK as expected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

BA_RATIO_PERCENT_BA_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio percent' with 2 serv an 1 ba with one service
    [Tags]    broker    engine    bam
    Ctn Clear Commands Status
    Ctn Clear Retention
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config BBDO3    ${1}
    Ctn Config Engine    ${1}
    # This is to avoid parasite status.
    Ctn Set Services Passive    ${0}    service_30.

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine
    Ctn Add Bam Config To Broker    central

    ${id_ba__sid}    Ctn Create Ba    test    ratio_percent    67    49
    Ctn Add Service Kpi    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_303    ${id_ba__sid[0]}    40    30    20

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${id_ba__sid__child}    Ctn Create Ba With Services    test_child    worst    ${svc}
    Ctn Add Ba Kpi    ${id_ba__sid__child[0]}    ${id_ba__sid[0]}    1    2    3

    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # one serv critical => ba ok
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    sleep    2s
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected

    # two serv critical => ba warning
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    1    30
    Should Be True    ${result}    The BA test is not WARNING as expected

    # two serv critical and child ba critical => mother ba critical
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_314
    ...    2
    ...    output critical for service_314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test_child    2    30
    Should Be True    ${result}    The BA test_child is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    2    30
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

BA_RATIO_NUMBER_BA_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio number' with 2 services and one ba with 1 service
    [Tags]    broker    engine    bam
    Ctn Clear Commands Status
    Ctn Clear Retention
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config BBDO3    ${1}
    Ctn Config Engine    ${1}
    # This is to avoid parasite status.
    Ctn Set Services Passive    ${0}    service_30.

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine
    Ctn Add Bam Config To Broker    central

    ${id_ba__sid}    Ctn Create Ba    test    ratio_number    3    2
    Ctn Add Service Kpi    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_303    ${id_ba__sid[0]}    40    30    20

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${id_ba__sid__child}    Ctn Create Ba With Services    test_child    worst    ${svc}
    Ctn Add Ba Kpi    ${id_ba__sid__child[0]}    ${id_ba__sid[0]}    1    2    3

    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # One service CRITICAL => The BA is still OK
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected

    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected

    # Two services CRITICAL => The BA passes to WARNING
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    1    60
    Should Be True    ${result}    The test BA is not in WARNING as expected

    # Two services CRITICAL and also the child BA => The mother BA passes to CRITICAL
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_314
    ...    2
    ...    output critical for service_314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test_child    2    30
    Should Be True    ${result}    The BA test_child is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

BA_BOOL_KPI
    [Documentation]    With bbdo version 3.0.1, a BA of type 'worst' with 1 boolean kpi
    [Tags]    broker    engine    bam
    Ctn Clear Commands Status
    Ctn Clear Retention
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config BBDO3    ${1}
    Ctn Config Engine    ${1}

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine
    Ctn Add Bam Config To Broker    central

    ${id_ba__sid}    Ctn Create Ba    test    worst    100    100
    Ctn Add Boolean Kpi
    ...    ${id_ba__sid[0]}
    ...    {host_16 service_302} {IS} {OK} {OR} ( {host_16 service_303} {IS} {OK} {AND} {host_16 service_314} {NOT} {UNKNOWN} )
    ...    False
    ...    100

    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # 302 warning and 303 critical    => ba critical
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_302
    ...    1
    ...    output warning for service_302
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    Ctn Process Service Check Result    host_16    service_314    0    output OK for service_314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    1    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not WARNING as expected
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not OK as expected

#    Ctn Schedule Forced Svc Check    _Module_BAM_1    ba_1
    ${result}    Ctn Check Ba Status With Timeout    test    2    30
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

BEPB_DIMENSION_BV_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_bv_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn Clear Commands Status
    Ctn Clear Retention

    Remove File    /tmp/all_lua_event.log
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker Sql Output    central    unified_sql

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Broker    central

    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-log-all-event.lua

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM mod_bam_ba_groups
    Execute SQL String
    ...    INSERT INTO mod_bam_ba_groups (id_ba_group, ba_group_name, ba_group_description) VALUES (574, 'virsgtr', 'description_grtmxzo')

    Ctn Start Broker    True
    Ctn Start Engine
    Wait Until Created    /tmp/all_lua_event.log    30s
    FOR    ${index}    IN RANGE    10
        ${grep_res}    Grep File
        ...    /tmp/all_lua_event.log
        ...    "_type":393238, "category":6, "element":22, "bv_id":574, "bv_name":"virsgtr", "bv_description":"description_grtmxzo"
        Sleep    1s
        IF    len("""${grep_res}""") > 0    BREAK
    END

    Should Not Be Empty    ${grep_res}    event not found

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker    ${True}

BEPB_DIMENSION_BA_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_ba_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn Clear Commands Status
    Ctn Clear Retention

    Remove File    /tmp/all_lua_event.log
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker Sql Output    central    unified_sql

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Broker    central
    Ctn Add Bam Config To Engine

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}

    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-log-all-event.lua

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    SET FOREIGN_KEY_CHECKS=0
    Execute SQL String
    ...    UPDATE mod_bam set description='fdpgvo75', sla_month_percent_warn=1.23, sla_month_percent_crit=4.56, sla_month_duration_warn=852, sla_month_duration_crit=789, id_reporting_period=741

    Ctn Start Broker    True
    Ctn Start Engine
    Wait Until Created    /tmp/all_lua_event.log    30s
    FOR    ${index}    IN RANGE    10
        ${grep_res}    Grep File
        ...    /tmp/all_lua_event.log
        ...    "_type":393241, "category":6, "element":25, "ba_id":1, "ba_name":"test", "ba_description":"fdpgvo75", "sla_month_percent_crit":4.56, "sla_month_percent_warn":1.23, "sla_duration_crit":789, "sla_duration_warn":852
        Sleep    1s
        IF    len("""${grep_res}""") > 0    BREAK
    END

    Should Not Be Empty    ${grep_res}    event not found

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker    ${True}

BEPB_DIMENSION_BA_BV_RELATION_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_ba_bv_relation_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn Clear Commands Status
    Ctn Clear Retention

    Remove File    /tmp/all_lua_event.log
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker Sql Output    central    unified_sql

    Ctn Clear Db    mod_bam_reporting_relations_ba_bv
    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine
    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}

    Ctn Add Bam Config To Broker    central

    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-log-all-event.lua

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Delete All Rows From Table    mod_bam_bagroup_ba_relation
    Execute SQL String    INSERT INTO mod_bam_bagroup_ba_relation (id_ba, id_ba_group) VALUES (1, 456)

    Ctn Start Broker    True
    Ctn Start Engine
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

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker    ${True}

BEPB_DIMENSION_TIMEPERIOD
    [Documentation]    use of pb_dimension_timeperiod message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn Clear Commands Status
    Ctn Clear Retention

    Remove File    /tmp/all_lua_event.log
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    lua    trace
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config Broker Sql Output    central    unified_sql

    Ctn Clone Engine Config To Db
    # Add Bam Config To Engine
    Ctn Add Bam Config To Broker    central

    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-log-all-event.lua

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String
    ...    INSERT INTO timeperiod (tp_id, tp_name, tp_sunday, tp_monday, tp_tuesday, tp_wednesday, tp_thursday, tp_friday, tp_saturday) VALUES (732, "ezizae", "sunday_value", "monday_value", "tuesday_value", "wednesday_value", "thursday_value", "friday_value", "saturday_value")

    Ctn Start Broker    True
    Ctn Start Engine
    Wait Until Created    /tmp/all_lua_event.log    30s
    FOR    ${index}    IN RANGE    10
        ${grep_res}    Grep File
        ...    /tmp/all_lua_event.log
        ...    "_type":393240, "category":6, "element":24, "id":732, "name":"ezizae", "monday":"monday_value", "tuesday":"tuesday_value", "wednesday":"wednesday_value", "thursday":"thursday_value", "friday":"friday_value", "saturday":"saturday_value", "sunday":"sunday_value"
        Sleep    1s
        IF    len("""${grep_res}""") > 0    BREAK
    END

    Should Not Be Empty    ${grep_res}    event not found

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker    ${True}

BEPB_DIMENSION_KPI_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_kpi_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn Clear Commands Status
    Ctn Clear Retention

    Remove File    /tmp/all_lua_event.log
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Source Log    central    1

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Broker    central
    Ctn Add Bam Config To Engine

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${baid_svcid}    Ctn Create Ba With Services    test    worst    ${svc}

    Ctn Add Boolean Kpi    ${baid_svcid[0]}    {host_16 service_302} {IS} {OK}    False    100

    Ctn Start Broker    True
    Ctn Start Engine

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

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker    ${True}

BEPB_KPI_STATUS
    [Documentation]    bbdo_version 3 use kpi_status message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn Clear Commands Status
    Ctn Clear Retention

    Remove File    /tmp/all_lua_event.log
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Source Log    central    1

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Broker    central
    Ctn Add Bam Config To Engine

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}

    Ctn Start Broker    True
    Ctn Start Engine

    ${start}    Get Current Date    result_format=epoch

    # KPI set to critical
    Repeat Keyword    3 times    Ctn Process Service Check Result    host_16    service_314    2    output critical for 314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    60    HARD
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

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker    ${True}

BEPB_BA_DURATION_EVENT
    [Documentation]    use of pb_ba_duration_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn Clear Commands Status
    Ctn Clear Retention

    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config Broker Sql Output    central    unified_sql

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Broker    central
    Ctn Add Bam Config To Engine

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String
    ...    INSERT INTO timeperiod (tp_id, tp_name, tp_sunday, tp_monday, tp_tuesday, tp_wednesday, tp_thursday, tp_friday, tp_saturday) VALUES (1, "ezizae", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59")
    Execute SQL String    DELETE FROM mod_bam_relations_ba_timeperiods

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM mod_bam_reporting_ba_events_durations

    Ctn Start Broker    True
    Ctn Start Engine

    # KPI set to critical
    # as GetCurrent Date floor milliseconds to upper or lower integer, we substract 1s
    ${start_event}    Ctn Get Round Current Date
    Repeat Keyword    3 times    Ctn Process Service Check Result    host_16    service_314    2    output critical for 314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected
    Sleep    2s
    Ctn Process Service Check Result    host_16    service_314    0    output ok for 314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    0    60    HARD
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

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker    ${True}

BEPB_DIMENSION_BA_TIMEPERIOD_RELATION
    [Documentation]    use of pb_dimension_ba_timeperiod_relation message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn Clear Commands Status
    Ctn Clear Retention

    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config Broker Sql Output    central    unified_sql

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Broker    central
    Ctn Add Bam Config To Engine

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String
    ...    INSERT INTO timeperiod (tp_id, tp_name, tp_sunday, tp_monday, tp_tuesday, tp_wednesday, tp_thursday, tp_friday, tp_saturday) VALUES (732, "ezizae", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59")
    Execute SQL String    DELETE FROM mod_bam_relations_ba_timeperiods
    Execute SQL String    INSERT INTO mod_bam_relations_ba_timeperiods (ba_id, tp_id) VALUES (1,732)

    Ctn Start Broker    True
    Ctn Start Engine

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

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker    ${True}

BEPB_DIMENSION_TRUNCATE_TABLE
    [Documentation]    use of pb_dimension_timeperiod message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn Clear Commands Status
    Ctn Clear Retention

    Remove File    /tmp/all_lua_event.log
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    lua    trace
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config Broker Sql Output    central    unified_sql

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Broker    central

    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-log-all-event.lua

    Ctn Start Broker    True
    Ctn Start Engine
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

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker    ${True}

BA_RATIO_NUMBER_BA_4_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
    [Tags]    broker    engine    bam
    Ctn Clear Commands Status
    Ctn Clear Retention
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config BBDO3    ${1}
    Ctn Config Engine    ${1}
    # This is to avoid parasite status.
    Ctn Set Services Passive    ${0}    service_30.

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine
    Ctn Add Bam Config To Broker    central

    ${id_ba__sid}    Ctn Create Ba    test    ratio_number    2    1
    Ctn Add Service Kpi    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_303    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_304    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_304    ${id_ba__sid[0]}    40    30    20

    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # all serv ok => ba ok
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected

    # one serv critical => ba warning
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    1    30
    Should Be True    ${result}    The BA test is not WARNING as expected

    # two services critical => ba ok
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    2    30
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    # all serv ok => ba ok
    Ctn Process Service Check Result    host_16    service_302    0    output ok for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not OK as expected
    Ctn Process Service Check Result    host_16    service_303    0    output ok for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not OK as expected
    ${result}    Ctn Check Ba Status With Timeout    test    0    30
    Should Be True    ${result}    The BA test is not OK as expected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

BA_RATIO_PERCENT_BA_4_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
    [Tags]    broker    engine    bam
    Ctn Clear Commands Status
    Ctn Clear Retention
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config BBDO3    ${1}
    Ctn Config Engine    ${1}
    # This is to avoid parasite status.
    Ctn Set Services Passive    ${0}    service_30.

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine
    Ctn Add Bam Config To Broker    central

    ${id_ba__sid}    Ctn Create Ba    test    ratio_percent    50    25
    Ctn Add Service Kpi    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_303    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_304    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_305    ${id_ba__sid[0]}    40    30    20

    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # all serv ok => ba ok
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected

    # one serv critical => ba warning
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    1    30
    Should Be True    ${result}    The BA test is not WARNING as expected

    # two services critical => ba ok
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    2    30
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    # all serv ok => ba ok
    Ctn Process Service Check Result    host_16    service_302    0    output ok for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not OK as expected
    Ctn Process Service Check Result    host_16    service_303    0    output ok for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not OK as expected
    ${result}    Ctn Check Ba Status With Timeout    test    0    30
    Should Be True    ${result}    The BA test is not OK as expected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker


BA_CHANGED
    [Documentation]    A BA of type worst is configured with one service kpi.
    ...                Then it is modified so that the service kpi is replaced
    ...                by a boolean rule kpi. When cbd is reloaded, the BA is
    ...                well updated.
    [Tags]    MON-34895
    Ctn Bam Init

    @{svc}    Set Variable    ${{ [("host_16", "service_302")] }}
    ${ba}    Ctn Create Ba With Services    test    worst    ${svc}

    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # Both services ${state} => The BA parent is ${state}
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    0
    ...    output OK for service 302
        
    ${result}    Ctn Check Ba Status With Timeout    test    0    30
    Ctn Dump Ba On Error    ${result}    ${ba[0]}
    Should Be True    ${result}    The BA test is not OK as expected

    Ctn Remove Service Kpi    ${ba[0]}    host_16    service_302
    Ctn Add Boolean Kpi
    ...    ${ba[0]}
    ...    {host_16 service_302} {IS} {OK}
    ...    False
    ...    100

    Ctn Reload Broker
    Remove File    /tmp/ba.dot
    Ctn Broker Get Ba    51001    ${ba[0]}    /tmp/ba.dot
    Wait Until Created    /tmp/ba.dot
    ${result}    Grep File    /tmp/ba.dot    Boolean exp
    Should Not Be Empty    ${result}

    Ctn Add Boolean Kpi
    ...    ${ba[0]}
    ...    {host_16 service_303} {IS} {WARNING}
    ...    False
    ...    100

    Ctn Reload Broker
    Remove File    /tmp/ba.dot
    Ctn Broker Get Ba    51001    ${ba[0]}    /tmp/ba.dot
    Wait Until Created    /tmp/ba.dot
    ${result}    Grep File    /tmp/ba.dot    BOOL Service (16, 303)
    Should Not Be Empty    ${result}
    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker


BA_IMPACT_IMPACT
    [Documentation]    A BA of type impact is defined with two BAs of type impact
    ...                as children. The first child has an impact of 90 and the
    ...                second one of 10. When they are impacting both, the
    ...                parent should be critical. When they are not impacting,
    ...                the parent should be ok.
    [Tags]    MON-34895
    Ctn Bam Init

    ${parent_ba}    Ctn Create Ba    parent    impact    20    99
    @{svc1}    Set Variable    ${{ [("host_16", "service_302")] }}
    ${child1_ba}    Ctn Create Ba    child1    impact    20    99
    Ctn Add Service Kpi    host_16    service_302    ${child1_ba[0]}    100    2    3
    ${child2_ba}    Ctn Create Ba    child2    impact    20    99
    Ctn Add Service Kpi    host_16    service_303    ${child2_ba[0]}    100    2    3

    Ctn Add Ba Kpi    ${child1_ba[0]}    ${parent_ba[0]}    90    2    3
    Ctn Add Ba Kpi    ${child2_ba[0]}    ${parent_ba[0]}    10    2    3

    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    FOR    ${state}    ${value}    IN
    ...    OK          0
    ...    CRITICAL    2
    ...    OK          0
    ...    CRITICAL    2
        # Both services ${state} => The BA parent is ${state}
        Ctn Process Service Result Hard
        ...    host_16
        ...    service_302
        ...    ${value}
        ...    output ${state} for service 302
        
        Ctn Process Service Result Hard
        ...    host_16
        ...    service_303
        ...    ${value}
        ...    output ${state} for service 302

        ${result}    Ctn Check Service Status With Timeout    host_16    service_302    ${value}    60    HARD
        Should Be True    ${result}    The service (host_16,service_302) is not ${state} as expected
        ${result}    Ctn Check Service Status With Timeout    host_16    service_303    ${value}    60    HARD
        Should Be True    ${result}    The service (host_16,service_303) is not ${state} as expected

        ${result}    Ctn Check Ba Status With Timeout    child1    ${value}    30
        Ctn Dump Ba On Error    ${result}    ${child1_ba[0]}
        Should Be True    ${result}    The BA child1 is not ${state} as expected

        ${result}    Ctn Check Ba Status With Timeout    child2    ${value}    30
        Ctn Dump Ba On Error    ${result}    ${child2_ba[0]}
        Should Be True    ${result}    The BA child2 is not ${state} as expected

        ${result}    Ctn Check Ba Status With Timeout    parent    ${value}    30
        Ctn Dump Ba On Error    ${result}    ${parent_ba[0]}
        Should Be True    ${result}    The BA parent is not ${state} as expected

        Remove Files    /tmp/parent1.dot    /tmp/parent2.dot
        Ctn Broker Get Ba    51001    ${parent_ba[0]}    /tmp/parent1.dot
        Wait Until Created    /tmp/parent1.dot

        ${start}    Get Current Date
        Ctn Reload Broker
        ${content}    Create List    Inherited downtimes and BA states restored
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
        Should Be True    ${result}    It seems that no cache has been restored into BAM.

        Ctn Broker Get Ba    51001    ${parent_ba[0]}    /tmp/parent2.dot
        Wait Until Created    /tmp/parent2.dot

        ${result}    Ctn Compare Dot Files    /tmp/parent1.dot    /tmp/parent2.dot
        Should Be True    ${result}    The BA changed during Broker reload.
    END

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker


BA_CHANGED
    [Documentation]    A BA of type worst is configured with one service kpi.
    ...                Then it is modified so that the service kpi is replaced
    ...                by a boolean rule kpi. When cbd is reloaded, the BA is
    ...                well updated.
    [Tags]    MON-34895
    Bam Init

    @{svc}    Set Variable    ${{ [("host_16", "service_302")] }}
    ${ba}    Create Ba With Services    test    worst    ${svc}

    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # Both services ${state} => The BA parent is ${state}
    Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    0
    ...    output OK for service 302
        
    ${result}    Check Ba Status With Timeout    test    0    30
    Dump Ba On Error    ${result}    ${ba[0]}
    Should Be True    ${result}    The BA test is not OK as expected

    Remove Service Kpi    ${ba[0]}    host_16    service_302
    Add Boolean Kpi
    ...    ${ba[0]}
    ...    {host_16 service_302} {IS} {OK}
    ...    False
    ...    100

    Reload Broker
    Remove File    /tmp/ba.dot
    Broker Get Ba    51001    ${ba[0]}    /tmp/ba.dot
    Wait Until Created    /tmp/ba.dot
    ${result}    Grep File    /tmp/ba.dot    Boolean exp
    Should Not Be Empty    ${result}

    Add Boolean Kpi
    ...    ${ba[0]}
    ...    {host_16 service_303} {IS} {WARNING}
    ...    False
    ...    100

    Reload Broker
    Remove File    /tmp/ba.dot
    Broker Get Ba    51001    ${ba[0]}    /tmp/ba.dot
    Wait Until Created    /tmp/ba.dot
    ${result}    Grep File    /tmp/ba.dot    BOOL Service (16, 303)
    Should Not Be Empty    ${result}
    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker


*** Keywords ***
Ctn BAM Setup
    Ctn Stop Processes
    Connect To Database    pymysql    ${DBName}    ${DBUserRoot}    ${DBPassRoot}    ${DBHost}    ${DBPort}
    Execute SQL String    SET GLOBAL FOREIGN_KEY_CHECKS=0
    Execute SQL String    DELETE FROM mod_bam_reporting_kpi
    Execute SQL String    DELETE FROM mod_bam_reporting_timeperiods
    Execute SQL String    DELETE FROM mod_bam_reporting_relations_ba_timeperiods
    Execute SQL String    DELETE FROM mod_bam_reporting_ba_events
    Execute SQL String    ALTER TABLE mod_bam_reporting_ba_events AUTO_INCREMENT = 1
    Execute SQL String    SET GLOBAL FOREIGN_KEY_CHECKS=1

Ctn Bam Init
    Ctn Clear Commands Status
    Ctn Clear Retention
    Ctn Clear Db Conf    mod_bam
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    config    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config BBDO3    ${1}
    Ctn Config Engine    ${1}
    # This is to avoid parasite status.
    Ctn Set Services Passive    ${0}    service_30.

    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine
    Ctn Add Bam Config To Broker    central

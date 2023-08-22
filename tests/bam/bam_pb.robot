*** Settings ***
Documentation       Centreon Broker and BAM with bbdo version 3.0.1

Resource            ../resources/resources.resource
Library             Process
Library             DatabaseLibrary
Library             DateTime
Library             OperatingSystem
Library             String
Library             ../resources/Broker.py
Library             ../resources/Engine.py
Library             Telnet

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          BAM Setup
Test Teardown       Save logs If Failed


*** Test Cases ***
BAWORST
    [Documentation]    With bbdo version 3.0.1, a BA of type 'worst' with two services is configured.
    [Tags]    broker    downtime    engine    bam
    BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314"), ("host_16", "service_303")] }}
    Create BA With Services    test    worst    ${svc}
    Start Broker
    ${start}    Get Current Date
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected

    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is OK - All KPIs are in an OK state
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to unknown
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_303    3    output unknown for 303

    ${result}    Check Service Status With Timeout    host_16    service_303    3    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not UNKNOWN as expected

    # The BA should become unknown
    ${result}    Check Ba Status With Timeout    test    3    60
    Should Be True    ${result}    The BA test is not UNKNOWN as expected

    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is UNKNOWN - At least one KPI is in an UNKNOWN state: KPI2 is in UNKNOWN state
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to warning
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_303    1    output warning for 303

    ${result}    Check Service Status With Timeout    host_16    service_303    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not WARNING as expected

    # The BA should become warning
    ${result}    Check Ba Status With Timeout    test    1    60
    Should Be True    ${result}    The BA test is not WARNING as expected

    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is WARNING - At least one KPI is in a WARNING state: KPI2 is in WARNING state
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314

    ${result}    Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    ${result}    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Query
    ...    SELECT current_level, acknowledged, downtime, in_downtime, current_status FROM mod_bam WHERE name='test'
    Should Be Equal As Strings    ${output}    ((100.0, 0.0, 0.0, 0, 2),)

    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is CRITICAL - At least one KPI is in a CRITICAL state: KPI2 is in WARNING state, KPI1 is in CRITICAL state
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BABEST_SERVICE_CRITICAL
    [Documentation]    With bbdo version 3.0.1, a BA of type 'best' with 2 serv, ba is critical only if the 2 services are critical
    [Tags]    broker    engine    bam
    BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314"), ("host_16", "service_303")] }}
    Create BA With Services    test    best    ${svc}
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

    ${result}    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected

    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is OK - At least one KPI is in an OK state
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314

    ${result}    Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    # The BA should remain OK
    Sleep    2s
    ${result}    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is OK - At least one KPI is in an OK state
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to unknown
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_303    3    output unknown for 303

    ${result}    Check Service Status With Timeout    host_16    service_303    3    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not UNKNOWN as expected

    # The BA should become warning
    ${result}    Check Ba Status With Timeout    test    3    60
    Should Be True    ${result}    The BA test is not UNKNOWN as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is UNKNOWN - All KPIs are in an UNKNOWN state or worse (WARNING or CRITICAL)
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to warning
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_303    1    output warning for 303

    ${result}    Check Service Status With Timeout    host_16    service_303    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not WARNING as expected

    # The BA should become warning
    ${result}    Check Ba Status With Timeout    test    1    60
    Should Be True    ${result}    The BA test is not WARNING as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is WARNING - All KPIs are in a WARNING state or worse (CRITICAL)
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_303    2    output critical for 303

    ${result}    Check Service Status With Timeout    host_16    service_303    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected

    # The BA should become critical
    ${result}    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA test is not CRITICAL as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is CRITICAL - All KPIs are in a CRITICAL state
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to OK
    Process Service Check Result    host_16    service_314    0    output ok for 314

    ${result}    Check Service Status With Timeout    host_16    service_314    0    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not OK as expected

    # The BA should become OK
    ${result}    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BA_IMPACT_2KPI_SERVICES
    [Documentation]    With bbdo version 3.0.1, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
    [Tags]    broker    engine    bam
    BAM Init

    ${id_ba__sid}    Create Ba    test    impact    20    35
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
    ${result}    Check Service Status With Timeout    host_16    service_302    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is OK - Level = 60 (warn: 35 - crit: 20) - 1 KPI out of 2 impacts the BA: KPI1 (impact: 40)|BA_Level=60;35;20;0;100
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    1
    ...    output warning for service_303
    ${result}    Check Service Status With Timeout    host_16    service_303    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not WARNING as expected
    ${result}    Check Ba Status With Timeout    test    1    60
    Should Be True    ${result}    The BA ba_1 is not WARNING as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is WARNING - Level = 30 - 2 KPIs out of 2 impact the BA for 70 points - KPI2 (impact: 30), KPI1 (impact: 40)|BA_Level=30;35;20;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

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
    ${result}    Check Service Status With Timeout    host_16    service_303    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is CRITICAL - Level = 20 - 2 KPIs out of 2 impact the BA for 80 points - KPI2 (impact: 40), KPI1 (impact: 40)|BA_Level=20;35;20;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    # service_302 ok => ba ok
    Process Service Check Result    host_16    service_302    0    output ok for service_302
    ${result}    Check Service Status With Timeout    host_16    service_302    0    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not OK as expected
    ${result}    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA ba_1 is not OK as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is OK - Level = 60 (warn: 35 - crit: 20) - 1 KPI out of 2 impacts the BA: KPI2 (impact: 40)|BA_Level=60;35;20;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

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
    ${result}    Check Service Status With Timeout    host_16    service_302    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not WARNING as expected
    ${result}    Check Service Status With Timeout    host_16    service_303    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not WARNING as expected
    ${result}    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is OK - Level = 40 (warn: 35 - crit: 20) - 2 KPIs out of 2 impact the BA: KPI2 (impact: 30), KPI1 (impact: 30)|BA_Level=40;35;20;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BA_RATIO_PERCENT_BA_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio percent' with 2 serv an 1 ba with one service
    [Tags]    broker    engine    bam
    BAM Init

    ${id_ba__sid}    Create Ba    test    ratio_percent    67    49
    Add Service KPI    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Add Service KPI    host_16    service_303    ${id_ba__sid[0]}    40    30    20

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${id_ba__sid__child}    Create Ba With Services    test_child    worst    ${svc}
    add_ba_kpi    ${id_ba__sid__child[0]}    ${id_ba__sid[0]}    1    2    3

    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is OK - 0% of KPIs are in a CRITICAL state (warn: 49 - crit: 67)|BA_Level=0%;49;67;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    # one serv critical => ba ok
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    Check Service Status With Timeout    host_16    service_302    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    sleep    2s
    ${result}    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is OK - 33% of KPIs are in a CRITICAL state (warn: 49 - crit: 67)|BA_Level=33%;49;67;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

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
    ${result}    Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Check Ba Status With Timeout    test    1    30
    Should Be True    ${result}    The BA test is not WARNING as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is WARNING - 66% of KPIs are in a CRITICAL state (warn: 49 - crit: 67)|BA_Level=66%;49;67;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

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
    ${result}    Check Service Status With Timeout    host_16    service_314    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected
    ${result}    Check Ba Status With Timeout    test_child    2    30
    Should Be True    ${result}    The BA test_child is not CRITICAL as expected
    ${result}    Check Ba Status With Timeout    test    2    30
    Should Be True    ${result}    The BA test is not CRITICAL as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is CRITICAL - 100% of KPIs are in a CRITICAL state (warn: 49 - crit: 67)|BA_Level=100%;49;67;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BA_RATIO_NUMBER_BA_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio number' with 2 services and one ba with 1 service
    [Tags]    broker    engine    bam
    BAM Init

    ${id_ba__sid}    Create Ba    test    ratio_number    3    2
    Add Service KPI    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Add Service KPI    host_16    service_303    ${id_ba__sid[0]}    40    30    20

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${id_ba__sid__child}    Create Ba With Services    test_child    worst    ${svc}
    Add BA KPI    ${id_ba__sid__child[0]}    ${id_ba__sid[0]}    1    2    3

    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is OK - 0 out of 3 KPIs are in a CRITICAL state (warn: 2 - crit: 3)|BA_Level=0;2;3;0;3
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    # One service CRITICAL => The BA is still OK
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    Check Service Status With Timeout    host_16    service_302    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected

    ${result}    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected

    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is OK - 1 out of 3 KPIs are in a CRITICAL state (warn: 2 - crit: 3)|BA_Level=1;2;3;0;3
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

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
    ${result}    Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Check Ba Status With Timeout    test    1    60
    Should Be True    ${result}    The test BA is not in WARNING as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is WARNING - 2 out of 3 KPIs are in a CRITICAL state (warn: 2 - crit: 3)|BA_Level=2;2;3;0;3
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

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
    ${result}    Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Check Service Status With Timeout    host_16    service_314    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected
    ${result}    Check Ba Status With Timeout    test_child    2    30
    Should Be True    ${result}    The BA test_child is not CRITICAL as expected
    ${result}    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA test is not CRITICAL as expected
    ${result}    Check Ba Output With Timeout
    ...    test
    ...    Status is CRITICAL - 3 out of 3 KPIs are in a CRITICAL state (warn: 2 - crit: 3)|BA_Level=3;2;3;0;3
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BA_BOOL_KPI
    [Documentation]    With bbdo version 3.0.1, a BA of type 'worst' with 1 boolean kpi
    [Tags]    broker    engine    bam
    BAM Init

    ${id_ba__sid}    Create Ba    test    worst    100    100
    Add Boolean Kpi
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
    ${result}    Check Service Status With Timeout    host_16    service_302    1    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not WARNING as expected
    ${result}    Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Check Service Status With Timeout    host_16    service_314    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not OK as expected

#    schedule_forced_svc_check    _Module_BAM_1    ba_1
    ${result}    Check Ba Status With Timeout    test    2    30
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BEPB_DIMENSION_BV_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_bv_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    BAM Init

    Create Ba    test    worst    100    100

    Remove File    /tmp/all_lua_event.log

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

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    True

BEPB_DIMENSION_BA_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_ba_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    BAM Init

    Remove File    /tmp/all_lua_event.log

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

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    True

BEPB_DIMENSION_BA_BV_RELATION_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_ba_bv_relation_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    BAM Init

    Remove File    /tmp/all_lua_event.log

    Clear Db    mod_bam_reporting_relations_ba_bv
    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}

    Create BA With Services    test    worst    ${svc}

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
    BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Create BA With Services    test    worst    ${svc}

    Remove File    /tmp/all_lua_event.log

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

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    True

BEPB_DIMENSION_KPI_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_kpi_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${baid_svcid}    Create Ba With Services    test    worst    ${svc}

    Add Boolean Kpi    ${baid_svcid[0]}    {host_16 service_302} {IS} {OK}    False    100

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

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    True

BEPB_KPI_STATUS
    [Documentation]    bbdo_version 3 use kpi_status message.
    [Tags]    broker    engine    protobuf    bam    bbdo

    BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Create Ba With Services    test    worst    ${svc}

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

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    True

BEPB_BA_DURATION_EVENT
    [Documentation]    use of pb_ba_duration_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Create Ba With Services    test    worst    ${svc}

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
        IF    ${output} and len(${output}) >= 1 and len(${output[0]}) >= 5
            BREAK
        END
    END

    Should Be True    ${output[0][2]} == ${output[0][1]} - ${output[0][0]}
    Should Be True    ${output[0][3]} == ${output[0][1]} - ${output[0][0]}
    Should Be True    ${output[0][4]} == 1
    Should Be True    ${output[0][1]} > ${output[0][0]}
    Should Be True    ${output[0][0]} >= ${start_event}
    Should Be True    ${output[0][1]} <= ${end_event}

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    True

BEPB_DIMENSION_BA_TIMEPERIOD_RELATION
    [Documentation]    use of pb_dimension_ba_timeperiod_relation message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Create Ba With Services    test    worst    ${svc}

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

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    True

BEPB_DIMENSION_TRUNCATE_TABLE
    [Documentation]    use of pb_dimension_timeperiod message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Create Ba With Services    test    worst    ${svc}

    Remove File    /tmp/all_lua_event.log
    Broker Config Log    central    lua    trace

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

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker    True

BA_RATIO_NUMBER_BA_4_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
    [Tags]    broker    engine    bam
    BAM Init

    ${id_ba__sid}    Create Ba    test    ratio_number    2    1
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
    ${result}    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected

    # one serv critical => ba warning
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Check Ba Status With Timeout    test    1    30
    Should Be True    ${result}    The BA test is not WARNING as expected

    # two services critical => ba ok
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Check Ba Status With Timeout    test    2    30
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    # all serv ok => ba ok
    Process Service Check Result    host_16    service_302    0    output ok for service_302
    ${result}    Check Service Status With Timeout    host_16    service_302    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not OK as expected
    Process Service Check Result    host_16    service_303    0    output ok for service_303
    ${result}    Check Service Status With Timeout    host_16    service_303    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not OK as expected
    ${result}    Check Ba Status With Timeout    test    0    30
    Should Be True    ${result}    The BA test is not OK as expected

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BA_RATIO_PERCENT_BA_4_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
    [Tags]    broker    engine    bam
    BAM Init

    ${id_ba__sid}    Create Ba    test    ratio_percent    50    25
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
    ${result}    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA test is not OK as expected

    # one serv critical => ba warning
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Check Ba Status With Timeout    test    1    30
    Should Be True    ${result}    The BA test is not WARNING as expected

    # two services critical => ba ok
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Check Ba Status With Timeout    test    2    30
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    # all serv ok => ba ok
    Process Service Check Result    host_16    service_302    0    output ok for service_302
    ${result}    Check Service Status With Timeout    host_16    service_302    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not OK as expected
    Process Service Check Result    host_16    service_303    0    output ok for service_303
    ${result}    Check Service Status With Timeout    host_16    service_303    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not OK as expected
    ${result}    Check Ba Status With Timeout    test    0    30
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

BAM Init
    Clear Commands Status
    Clear Retention
    Clear Db Conf    mod_bam
    Config Broker    module
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    trace
    Broker Config Log    central    config    trace
    Broker Config Source Log    central    1
    Config BBDO3    ${1}
    Config Engine    ${1}
    # This is to avoid parasite status.
    Set Services Passive    ${0}    service_30.

    Config Broker Sql Output    central    unified_sql
    Clone Engine Config To DB
    Add Bam Config To Engine
    Add Bam Config To Broker    central

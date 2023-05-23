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
    [Documentation]    With bbdo version 3.0.0, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service.
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
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.

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

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}=    Query
    ...    SELECT current_level, acknowledged, downtime, in_downtime, current_status FROM mod_bam WHERE name='test'
    Should Be Equal As Strings    ${output}    ((100.0, 0.0, 0.0, 0, 2),)

    Stop Engine
    Kindly Stop Broker

BABEST_SERVICE_CRITICAL
    [Documentation]    With bbdo version 3.0.0, a BA of type 'best' with 2 serv, ba is critical only if the 2 services are critical
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

    @{svc}=    Set Variable    ${{ [("host_16", "service_314"), ("host_16", "service_303")] }}
    Create BA With Services    test    best    ${svc}
    Add Bam Config To Broker    central
    # Command of service_314 is set to critical
    ${cmd_1}=    Get Command Id    314
    Log To Console    service_314 has command id ${cmd_1}
    Set Command Status    ${cmd_1}    2
    Start Broker
    ${start}=    Get Current Date
    Start Engine
    # Let's wait for the initial service states.
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_314    2    output critical for 314

    ${result}=    Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    msg=The service (host_16,service_314) is not CRITICAL as expected

    # The BA should remain OK
    Sleep    2s
    ${result}=    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    msg=The BA ba_1 is not OK as expected

    # KPI set to critical
    Repeat Keyword    3 times    Process Service Check Result    host_16    service_303    2    output critical for 314

    ${result}=    Check Service Status With Timeout    host_16    service_303    2    60    HARD
    Should Be True    ${result}    msg=The service (host_16,service_303) is not CRITICAL as expected

    # The BA should become critical
    ${result}=    Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    msg=The BA ba_1 is not CRITICAL as expected

    # KPI set to OK
    Process Service Check Result    host_16    service_314    0    output ok for 314

    ${result}=    Check Service Status With Timeout    host_16    service_314    0    60    HARD
    Should Be True    ${result}    msg=The service (host_16,service_314) is not OK as expected

    # The BA should become OK
    ${result}=    Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    msg=The BA ba_1 is not OK as expected

    Stop Engine
    Kindly Stop Broker

BA_IMPACT_2KPI_SERVICES
    [Documentation]    With bbdo version 3.0.0, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
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
    Add Bam Config To Broker    central

    ${id_ba__sid}=    create_ba    test    impact    20    35
    add_service_kpi    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    add_service_kpi    host_16    service_303    ${id_ba__sid[0]}    40    30    20

    Start Broker
    ${start}=    Get Current Date
    Start Engine
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.

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
    ${result}=    check_service_status_with_timeout    host_16    service_302    2    60    HARD
    Should Be True    ${result}    msg=The service (host_16,service_302) is not CRITICAL as expected
    ${result}=    check_service_status_with_timeout    host_16    service_303    1    60    HARD
    Should Be True    ${result}    msg=The service (host_16,service_303) is not WARNING as expected
    ${result}=    check_ba_status_with_timeout    test    1    60
    Should Be True    ${result}    msg=The BA ba_1 is not WARNING as expected

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
    ${result}=    check_service_status_with_timeout    host_16    service_303    2    60    HARD
    Should Be True    ${result}    msg=The service (host_16,service_303) is not CRITICAL as expected
    ${result}=    check_ba_status_with_timeout    test    2    60
    Should Be True    ${result}    msg=The BA ba_1 is not CRITICAL as expected

    # service_302 ok => ba ok
    Process Service Check Result    host_16    service_302    0    output ok for service_302
    ${result}=    check_service_status_with_timeout    host_16    service_302    0    60    HARD
    Should Be True    ${result}    msg=The service (host_16,service_302) is not OK as expected
    ${result}=    check_ba_status_with_timeout    test    0    60
    Should Be True    ${result}    msg=The BA ba_1 is not OK as expected

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
    ${result}=    check_service_status_with_timeout    host_16    service_302    1    60    HARD
    Should Be True    ${result}    msg=The service (host_16,service_302) is not WARNING as expected
    ${result}=    check_service_status_with_timeout    host_16    service_303    1    60    HARD
    Should Be True    ${result}    msg=The service (host_16,service_303) is not WARNING as expected
    ${result}=    check_ba_status_with_timeout    test    0    60
    Should Be True    ${result}    msg=The BA ba_1 is not OK as expected

    Stop Engine
    Kindly Stop Broker

BA_BOOL_KPI
    [Documentation]    With bbdo version 3.0.0, a BA of type 'worst' with 1 boolean kpi
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

    ${id_ba__sid}=    create_ba    test    worst    100    100
    add_boolean_kpi
    ...    ${id_ba__sid[0]}
    ...    {host_16 service_302} {IS} {OK} {OR} ( {host_16 service_303} {IS} {OK} {AND} {host_16 service_314} {NOT} {UNKNOWN} )
    ...    100

    Start Broker
    ${start}=    Get Current Date
    Start Engine
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.

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
    ${result}=    check_service_status_with_timeout    host_16    service_302    1    30    HARD
    Should Be True    ${result}    msg=The service (host_16,service_302) is not WARNING as expected
    ${result}=    check_service_status_with_timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    msg=The service (host_16,service_303) is not CRITICAL as expected
    ${result}=    check_service_status_with_timeout    host_16    service_314    0    30    HARD
    Should Be True    ${result}    msg=The service (host_16,service_314) is not OK as expected

#    schedule_forced_svc_check    _Module_BAM_1    ba_1
    ${result}=    check_ba_status_with_timeout    test    2    30
    Should Be True    ${result}    msg=The BA test is not CRITICAL as expected

    Stop Engine
    Kindly Stop Broker

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

    ${id_ba__sid}=    create_ba    test    ratio_number    2    1
    Add Service KPI    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Add Service KPI    host_16    service_303    ${id_ba__sid[0]}    40    30    20
    Add Service KPI    host_16    service_304    ${id_ba__sid[0]}    40    30    20
    Add Service KPI    host_16    service_304    ${id_ba__sid[0]}    40    30    20

    Start Broker
    ${start}=    Get Current Date
    Start Engine
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.

    # All services are OK => The BA is OK
    ${result}=    check_ba_status_with_timeout    test    0    60
    Should Be True    ${result}    msg=The BA test is not OK as expected

    # One service is CRITICAL => The BA is in WARNING
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}=    check_service_status_with_timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    msg=The service (host_16,service_302) is not CRITICAL as expected
    ${result}=    check_ba_status_with_timeout    test    1    60
    Should Be True    ${result}    msg=The BA test is not WARNING as expected

    # Two services are CRITICAL => The BA is CRITICAL
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}=    check_service_status_with_timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    msg=The service (host_16,service_303) is not CRITICAL as expected
    ${result}=    check_ba_status_with_timeout    test    2    60
    Should Be True    ${result}    msg=The BA test is not CRITICAL as expected

    # All the services are OK => The BA is OK
    Process Service Check Result    host_16    service_302    0    output ok for service_302
    ${result}=    check_service_status_with_timeout    host_16    service_302    0    30    HARD
    Should Be True    ${result}    msg=The service (host_16,service_302) is not OK as expected
    Process Service Check Result    host_16    service_303    0    output ok for service_303
    ${result}=    check_service_status_with_timeout    host_16    service_303    0    30    HARD
    Should Be True    ${result}    msg=The service (host_16,service_303) is not OK as expected
    ${result}=    check_ba_status_with_timeout    test    0    60
    Should Be True    ${result}    msg=The BA test is not OK as expected

    Stop Engine
    Kindly Stop Broker

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

    Clone Engine Config To DB
    Add Bam Config To Engine
    Add Bam Config To Broker    central

    ${id_ba__sid}=    create_ba    test    ratio_percent    50    25
    add_service_kpi    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    add_service_kpi    host_16    service_303    ${id_ba__sid[0]}    40    30    20
    add_service_kpi    host_16    service_304    ${id_ba__sid[0]}    40    30    20
    add_service_kpi    host_16    service_305    ${id_ba__sid[0]}    40    30    20

    Start Broker
    ${start}=    Get Current Date
    Start Engine
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.

    # all serv ok => ba ok
    ${result}=    check_ba_status_with_timeout    test    0    60
    Should Be True    ${result}    msg=The BA test is not OK as expected

    # one serv critical => ba warning
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}=    check_service_status_with_timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    msg=The service (host_16,service_302) is not CRITICAL as expected
    ${result}=    check_ba_status_with_timeout    test    1    30
    Should Be True    ${result}    msg=The BA test is not WARNING as expected

    # two services critical => ba ok
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}=    check_service_status_with_timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    msg=The service (host_16,service_303) is not CRITICAL as expected
    ${result}=    check_ba_status_with_timeout    test    2    30
    Should Be True    ${result}    msg=The BA test is not CRITICAL as expected

    # all serv ok => ba ok
    Process Service Check Result    host_16    service_302    0    output ok for service_302
    ${result}=    check_service_status_with_timeout    host_16    service_302    0    30    HARD
    Should Be True    ${result}    msg=The service (host_16,service_302) is not OK as expected
    Process Service Check Result    host_16    service_303    0    output ok for service_303
    ${result}=    check_service_status_with_timeout    host_16    service_303    0    30    HARD
    Should Be True    ${result}    msg=The service (host_16,service_303) is not OK as expected
    ${result}=    check_ba_status_with_timeout    test    0    30
    Should Be True    ${result}    msg=The BA test is not OK as expected

    Stop Engine
    Kindly Stop Broker


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

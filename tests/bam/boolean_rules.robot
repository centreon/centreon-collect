*** Settings ***
Documentation       Centreon Broker and BAM with bbdo version 3.0.1 on boolean rules

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
BABOO
    [Documentation]    With bbdo version 3.0.1, a BA of type 'worst' with 2 child services and another BA of type impact with a boolean rule returning if one of its two services are critical are created. These two BA are built from the same services and should have a similar behavior
    [Tags]    broker    engine    bam    boolean_expression
    Clear Commands Status
    Clear Retention
    Config Broker    module
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    core    error
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    error
    Broker Config Flush Log    central    0
    Broker Config Source Log    central    1
    Config BBDO3    ${1}
    Config Engine    ${1}

    Clone Engine Config To DB
    Add Bam Config To Engine
    Add Bam Config To Broker    central
    Set Services Passive    ${0}    service_302
    Set Services Passive    ${0}    service_303

    ${id_ba__sid}=    create_ba    ba-worst    worst    70    80
    Add Service KPI    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Add Service KPI    host_16    service_303    ${id_ba__sid[0]}    40    30    20

    ${id_ba__sid}=    create_ba    boolean-ba    impact    70    80
    add_boolean_kpi
    ...    ${id_ba__sid[0]}
    ...    {host_16 service_302} {IS} {CRITICAL} {OR} {host_16 service_303} {IS} {CRITICAL}
    ...    True
    ...    100

    Start Broker
    ${start}=    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.

    # 393 is set to ok.
    Process Service Check Result    host_16    service_303    0    output ok for service_303

    FOR    ${i}    IN RANGE    10
        Log To Console    @@@@@@@@@@@@@@ Step ${i} @@@@@@@@@@@@@@
        # 302 is set to critical => the two ba become critical
        Repeat Keyword
        ...    3 times
        ...    Process Service Check Result
        ...    host_16
        ...    service_302
        ...    2
        ...    output critical for service_302

        ${result}=    check_ba_status_with_timeout    ba-worst    2    30
        Should Be True    ${result}    msg=The 'ba-worst' BA is not CRITICAL as expected
        ${result}=    check_ba_status_with_timeout    boolean-ba    2    30
        Should Be True    ${result}    msg=The 'boolean-ba' BA is not CRITICAL as expected

        Process Service Check Result    host_16    service_302    0    output ok for service_302
        ${result}=    check_ba_status_with_timeout    ba-worst    0    30
        Should Be True    ${result}    msg=The 'ba-worst' BA is not OK as expected
        ${result}=    check_ba_status_with_timeout    boolean-ba    0    30
        Should Be True    ${result}    msg=The 'boolean-ba' BA is not OK as expected
    END

    Stop Engine
    Kindly Stop Broker

BABOOOR
    [Documentation]    With bbdo version 3.0.1, a BA of type 'worst' with 2 child services and another BA of type impact with a boolean rule returning if one of its two services are critical are created. These two BA are built from the same services and should have a similar behavior
    [Tags]    broker    engine    bam    boolean_expression
    Clear Commands Status
    Clear Retention
    Config Broker    module
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    core    error
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    error
    Broker Config Flush Log    central    0
    Broker Config Source Log    central    1
    Config BBDO3    ${1}
    Config Engine    ${1}

    Clone Engine Config To DB
    Add Bam Config To Engine
    Add Bam Config To Broker    central
    Set Services Passive    ${0}    service_302
    Set Services Passive    ${0}    service_303

    ${id_ba__sid}=    create_ba    boolean-ba    impact    70    80
    add_boolean_kpi
    ...    ${id_ba__sid[0]}
    ...    {host_16 service_302} {IS} {CRITICAL} {OR} {host_16 service_303} {IS} {CRITICAL}
    ...    True
    ...    100

    Start Broker
    ${start}=    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.
    # 303 is unknown but since the boolean operator is OR, if 302 result is true, we should have already a result.

    # 302 is set to critical => the two ba become critical
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302

    ${result}=    check_ba_status_with_timeout    boolean-ba    2    30
    Should Be True    ${result}    msg=The 'boolean-ba' BA is not CRITICAL as expected

    Stop Engine
    Kindly Stop Broker

BABOOAND
    [Documentation]    With bbdo version 3.0.1, a BA of type impact with a boolean rule returning if both of its two services are ok is created. When one condition is false, the and operator returns false as a result even if the other child is unknown.
    [Tags]    broker    engine    bam    boolean_expression
    Clear Commands Status
    Clear Retention
    Config Broker    module
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    core    error
    Broker Config Log    central    bam    trace
    Broker Config Log    central    sql    error
    Broker Config Flush Log    central    0
    Broker Config Source Log    central    1
    Config BBDO3    ${1}
    Config Engine    ${1}

    Clone Engine Config To DB
    Add Bam Config To Engine
    Add Bam Config To Broker    central
    Set Services Passive    ${0}    service_302
    Set Services Passive    ${0}    service_303

    ${id_ba__sid}=    create_ba    boolean-ba    impact    70    80
    add_boolean_kpi
    ...    ${id_ba__sid[0]}
    ...    {host_16 service_302} {IS} {OK} {AND} {host_16 service_303} {IS} {OK}
    ...    False
    ...    100

    Start Broker
    ${start}=    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.
    # 303 is unknown but since the boolean operator is AND, if 302 result is false, we should have already a result.

    # 302 is set to critical => the two ba become critical
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302

    ${result}=    check_ba_status_with_timeout    boolean-ba    2    30
    Should Be True    ${result}    msg=The 'boolean-ba' BA is not CRITICAL as expected

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

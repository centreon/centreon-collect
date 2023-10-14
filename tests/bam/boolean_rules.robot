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

    ${id_ba_worst__sid}    Create Ba    ba-worst    worst    70    80
    Add Service KPI    host_16    service_302    ${id_ba_worst__sid[0]}    40    30    20
    Add Service KPI    host_16    service_303    ${id_ba_worst__sid[0]}    40    30    20

    ${id_boolean_ba__sid}    Create Ba    boolean-ba    impact    70    80
    Add Boolean Kpi
    ...    ${id_boolean_ba__sid[0]}
    ...    {host_16 service_302} {IS} {CRITICAL} {OR} {host_16 service_303} {IS} {CRITICAL}
    ...    True
    ...    100

    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

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

        ${result}    Check Service Resource Status With Timeout    host_16    service_302    2    30    HARD
        Should Be True    ${result}    The service (host_16:service_302) should be CRITICAL.
        ${result}    Check Ba Status With Timeout    ba-worst    2    30
        Dump Ba On Error    ${result}    ${id_ba_worst__sid[0]}
        Should Be True    ${result}    The 'ba-worst' BA is not CRITICAL as expected
        ${result}    Check Ba Status With Timeout    boolean-ba    2    30
        Dump Ba On Error    ${result}    ${id_boolean_ba__sid[0]}
        Should Be True    ${result}    The 'boolean-ba' BA is not CRITICAL as expected

        Process Service Check Result    host_16    service_302    0    output ok for service_302
        ${result}    Check Ba Status With Timeout    ba-worst    0    30
        Dump Ba On Error    ${result}    ${id_ba_worst__sid[0]}
        Should Be True    ${result}    The 'ba-worst' BA is not OK as expected
        ${result}    Check Service Resource Status With Timeout    host_16    service_302    0    30    HARD
        Should Be True    ${result}    The service (host_16:service_302) should be OK.

        ${result}    Check Ba Status With Timeout    boolean-ba    0    30
        Dump Ba On Error    ${result}    ${id_boolean_ba__sid[0]}
        Should Be True    ${result}    The 'boolean-ba' BA is not OK as expected
    END

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

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

    ${id_ba__sid}    Create Ba    boolean-ba    impact    70    80
    Add Boolean Kpi
    ...    ${id_ba__sid[0]}
    ...    {host_16 service_302} {IS} {CRITICAL} {OR} {host_16 service_303} {IS} {CRITICAL}
    ...    True
    ...    100

    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.
    # 303 is unknown but since the boolean operator is OR, if 302 result is true, we should have already a result.

    # 302 is set to critical => the two ba become critical
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302

    ${result}    Check Ba Status With Timeout    boolean-ba    2    30
    Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'boolean-ba' BA is not CRITICAL as expected

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

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

    ${id_ba__sid}    Create Ba    boolean-ba    impact    70    80
    Add Boolean Kpi
    ...    ${id_ba__sid[0]}
    ...    {host_16 service_302} {IS} {OK} {AND} {host_16 service_303} {IS} {OK}
    ...    False
    ...    100

    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.
    # 303 is unknown but since the boolean operator is AND, if 302 result is false, we should have already a result.

    # 302 is set to critical => the two ba become critical
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302

    ${result}    Check Ba Status With Timeout    boolean-ba    2    30
    Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'boolean-ba' BA is not CRITICAL as expected

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BABOOORREL
    [Documentation]    With bbdo version 3.0.1, a BA of type impact with a boolean rule returning if one of its two services is ok is created. One of the two underlying services must change of state to change the ba state. For this purpose, we change the service state and reload cbd. So the rule is something like "False OR True" which is equal to True. And to pass from True to False, we change the second service.
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
    Set Services Passive    ${0}    service_304

    ${id_ba__sid}    Create Ba    boolean-ba    impact    70    80
    ${id_bool}    Add Boolean Kpi
    ...    ${id_ba__sid[0]}
    ...    {host_16 service_302} {IS} {OK} {OR} {host_16 service_303} {IS} {OK}
    ...    False
    ...    100

    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.
    # 302 is set to critical => {host_16 service_302} {IS} {OK} is then False
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL/HARD as expected

    # 303 is set to critical => {host_16 service_303} {IS} {OK} is then False
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL/HARD as expected

    # 304 is set to ok => {host_16 service_304} {IS} {OK} is then True
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    host_16
    ...    service_304
    ...    0
    ...    output ok for service_304
    ${result}    Check Service Status With Timeout    host_16    service_304    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_304) is not OK/HARD as expected

    ${result}    Check Ba Status With Timeout    boolean-ba    2    30
    Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'boolean-ba' BA is not CRITICAL as expected

    Update Boolean Rule
    ...    ${id_bool}
    ...    {host_16 service_302} {IS} {OK} {OR} {host_16 service_304} {IS} {OK}

    Restart Engine
    Reload Broker

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.
    ${result}    Check Ba Status With Timeout    boolean-ba    0    30
    Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'boolean-ba' BA is not OK as expected

    Update Boolean Rule
    ...    ${id_bool}
    ...    {host_16 service_302} {IS} {OK} {OR} {host_16 service_303} {IS} {OK}

    Restart Engine
    Reload Broker

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.
    ${result}    Check Ba Status With Timeout    boolean-ba    2    30
    Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'boolean-ba' BA is not CRITICAL as expected

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

BABOOCOMPL
    [Documentation]    With bbdo version 3.0.1, a BA of type impact with a complex boolean rule is configured. We check its correct behaviour following service updates.
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
    # Services 1 to 21 are passive now.
    FOR    ${i}    IN RANGE    ${1}    ${21}
        Set Services Passive    ${0}    service_${i}
    END

    ${id_ba__sid}    Create Ba    boolean-ba    impact    70    80
    ${id_bool}    Add Boolean Kpi
    ...    ${id_ba__sid[0]}
    ...    ({host_1 service_1} {IS} {OK} {OR} {host_1 service_2} {IS} {OK}) {AND} ({host_1 service_3} {IS} {OK} {OR} {host_1 service_4} {IS} {OK}) {AND} ({host_1 service_5} {IS} {OK} {OR} {host_1 service_6} {IS} {OK}) {AND} ({host_1 service_7} {IS} {OK} {OR} {host_1 service_8} {IS} {OK}) {AND} ({host_1 service_9} {IS} {OK} {OR} {host_1 service_10} {IS} {OK}) {AND} ({host_1 service_11} {IS} {OK} {OR} {host_1 service_12} {IS} {OK}) {AND} ({host_1 service_13} {IS} {OK} {OR} {host_1 service_14} {IS} {OK}) {AND} ({host_1 service_15} {IS} {OK} {OR} {host_1 service_16} {IS} {OK}) {AND} ({host_1 service_17} {IS} {OK} {OR} {host_1 service_18} {IS} {OK}) {AND} ({host_1 service_19} {IS} {OK} {OR} {host_1 service_20} {IS} {OK})
    ...    False
    ...    100

    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.
    FOR    ${i}    IN RANGE    ${1}    ${21}
        Repeat Keyword
        ...    3 times
        ...    Process Service Check Result
        ...    host_1
        ...    service_${i}
        ...    2
        ...    output critical for service_${i}
    END

    FOR    ${i}    IN RANGE    ${1}    ${21}    ${2}
        ${result}    Check Ba Status With Timeout    boolean-ba    2    30
        Dump Ba On Error    ${result}    ${id_ba__sid[0]}
        Should Be True    ${result}    Step${i}: The 'boolean-ba' BA is not CRITICAL as expected
        Repeat Keyword
        ...    3 times
        ...    Process Service Check Result
        ...    host_1
        ...    service_${i}
        ...    0
        ...    output ok for service_${i}
    END

    ${result}    Check Ba Status With Timeout    boolean-ba    0    30
    Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'boolean-ba' BA is not OK as expected

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

Dump Ba On Error
    [Arguments]    ${result}    ${ba_id}
    IF    not ${result}
        Save Logs
        Dump Ba    51001    ${ba_id}    failed/${Test Name}/ba_${ba_id}.dot
    END

*** Settings ***
Documentation       Engine/Broker tests on severities with centralized configuration.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CBEUHSEV1
    [Documentation]    Given four hosts with a severity added,
    ...    When we remove the severity from host 1
    ...    And we change severity 10 to severity 8 for host 3,
    ...    Then host 2 should still have severity_id=10
    ...    And host 4 should still have severity_id=10
    ...    And host 3 should have severity_id=8
    ...    And host 1 should have no severity.
    [Tags]    broker    engine    protobuf    bbdo    severities
    Ctn Config Centralized Engine    ${1}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Add Severity To Hosts    0    10    [1, 2, 3, 4]
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Start Broker    newGeneration=True

    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.
    ${result}    Ctn Check Host Severity With Timeout    1    10    60
    Should Be True    ${result}    Host 1 should have severity_id=10

    Ctn Remove Severities From Hosts    ${0}
    Ctn Add Severity To Hosts    0    10    [2, 4]
    Ctn Add Severity To Hosts    0    8    [3]

    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.
    ${result}    Ctn Check Host Severity With Timeout    2    10    60
    Should Be True    ${result}    Host 2 should have severity_id=10
    ${result}    Ctn Check Host Severity With Timeout    4    10    60
    Should Be True    ${result}    Host 4 should have severity_id=10
    ${result}    Ctn Check Host Severity With Timeout    3    8    60
    Should Be True    ${result}    Host 3 should have severity_id=8
    ${result}    Ctn Check Host Severity With Timeout    1    None    60
    Should Be True    ${result}    Host 1 should have no severity

    ${result}    Ctn Check Severity Db Id In Cache With Timeout    51001    10    1    60
    Should Be True    ${result}    Severity 10 (host) should have a non-zero db_id in broker cache
    ${result}    Ctn Check Severity Db Id In Cache With Timeout    51001    8    1    60
    Should Be True    ${result}    Severity 8 (host) should have a non-zero db_id in broker cache

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CBEUHSEV2
    [Documentation]    Given seven hosts configured with severities on two pollers,
    ...    When we remove severities from hosts on the first poller
    ...    And we change host 28's severity from 16 to 14 on the second poller,
    ...    Then host 26 should still have severity_id=18
    ...    And host 27 should still have severity_id=18
    ...    And host 28 should have severity_id=14
    ...    And hosts 3, 4 and 5 on the first poller should have no severity.
    [Tags]    broker    engine    protobuf    bbdo    severities
    Ctn Config Centralized Engine    ${2}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Create Severities File    ${1}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Engine Add Cfg File    ${1}    severities.cfg
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Engine Config Set Value    ${1}    log_level_config    debug
    Ctn Add Severity To Hosts    0    18    [2, 4]
    Ctn Add Severity To Hosts    0    16    [3, 5]
    Ctn Add Severity To Hosts    1    18    [26, 27]
    Ctn Add Severity To Hosts    1    16    [28]
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${2}
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    module1    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Start Broker    newGeneration=True

    Ctn Notify Broker Of Engine Config Change    0
    Ctn Notify Broker Of Engine Config Change    1
    ${content}    Create List    received diff state ack from poller 1    received diff state ack from poller 2
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from each poller.

    ${result}    Ctn Check Host Severity With Timeout    2    18    60
    Should Be True    ${result}    First step: Host 2 should have severity_id=18

    ${result}    Ctn Check Host Severity With Timeout    4    18    60
    Should Be True    ${result}    First step: Host 4 should have severity_id=18

    ${result}    Ctn Check Host Severity With Timeout    26    18    60
    Should Be True    ${result}    First step: Host 26 should have severity_id=18

    ${result}    Ctn Check Host Severity With Timeout    27    18    60
    Should Be True    ${result}    First step: Host 27 should have severity_id=18

    ${result}    Ctn Check Host Severity With Timeout    3    16    60
    Should Be True    ${result}    First step: Host 3 should have severity_id=16

    ${result}    Ctn Check Host Severity With Timeout    5    16    60
    Should Be True    ${result}    First step: Host 5 should have severity_id=16

    ${result}    Ctn Check Host Severity With Timeout    28    16    60
    Should Be True    ${result}    First step: Host 28 should have severity_id=16

    Ctn Remove Severities From Hosts    ${0}
    Ctn Remove Severities From Hosts    ${1}
    Ctn Create Severities File    ${0}    ${18}
    Ctn Create Severities File    ${1}    ${18}
    Ctn Add Severity To Hosts    1    18    [26, 27]
    Ctn Add Severity To Hosts    1    14    [28]

    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    Ctn Notify Broker Of Engine Config Change    1
    ${content}    Create List    received diff state ack from poller 1    received diff state ack from poller 2
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from both pollers.

    ${result}    Ctn Check Host Severity With Timeout    26    18    60
    Should Be True    ${result}    Second step: Host 26 should have severity_id=18
    ${result}    Ctn Check Host Severity With Timeout    27    18    60
    Should Be True    ${result}    Second step: Host 27 should have severity_id=18
    ${result}    Ctn Check Host Severity With Timeout    28    14    60
    Should Be True    ${result}    Second step: Host 28 should have severity_id=14

    ${result}    Ctn Check Host Severity With Timeout    4    None    60
    Should Be True    ${result}    Second step: Host 4 should have no severity

    ${result}    Ctn Check Host Severity With Timeout    3    None    60
    Should Be True    ${result}    Second step: Host 3 should have no severity

    ${result}    Ctn Check Host Severity With Timeout    5    None    60
    Should Be True    ${result}    Second step: Host 5 should have no severity

    ${result}    Ctn Check Severity Db Id In Cache With Timeout    51001    18    1    60
    Should Be True    ${result}    Severity 18 (host) should have a non-zero db_id in broker cache
    ${result}    Ctn Check Severity Db Id In Cache With Timeout    51001    14    1    60
    Should Be True    ${result}    Severity 14 (host) should have a non-zero db_id in broker cache

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CBETUHSEV1
    [Documentation]    Given hosts on two pollers using templates that define severities
    ...    (template_1: severity 2 on poller 0, severity 6 on poller 1;
    ...    template_2: severity 4 on poller 0, severity 10 on poller 1),
    ...    When the engine and broker are started with centralized configuration,
    ...    Then host 2 and host 4 should have severity_id=2
    ...    And host 5 should have severity_id=4
    ...    And host 31 should have severity_id=6
    ...    And host 33 should have severity_id=10.
    [Tags]    broker    engine    protobuf    bbdo    severities
    Ctn Clear Prot Files
    Ctn Config Centralized Engine    ${2}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Create Severities File    ${1}    ${20}
    Ctn Create Template File    ${0}    host    severity    [2, 4]
    Ctn Create Template File    ${1}    host    severity    [6, 10]

    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Engine Add Cfg File    ${1}    severities.cfg
    Ctn Config Engine Add Cfg File    ${0}    hostTemplates.cfg
    Ctn Config Engine Add Cfg File    ${1}    hostTemplates.cfg
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Engine Config Set Value    ${1}    log_level_config    debug
    Ctn Add Template To Hosts    0    host_template_1    [1, 2, 3, 4]
    Ctn Add Template To Hosts    0    host_template_2    [5, 6, 7, 8]
    Ctn Add Template To Hosts    1    host_template_1    [31, 32]
    Ctn Add Template To Hosts    1    host_template_2    [33, 34]
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${2}
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    module1    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Start Broker    newGeneration=True
    Sleep    5s
    # We need to wait a little before reloading Engine
    ${result}    Ctn Check Host Severity With Timeout    2    2    60
    Should Be True    ${result}    First step: Host 2 should have severity_id=2

    ${result}    Ctn Check Host Severity With Timeout    4    2    60
    Should Be True    ${result}    First step: Host 4 should have severity_id=2

    ${result}    Ctn Check Host Severity With Timeout    5    4    60
    Should Be True    ${result}    First step: Host 5 should have severity_id=4

    ${result}    Ctn Check Host Severity With Timeout    31    6    60
    Should Be True    ${result}    First step: Host 31 should have severity_id=6

    ${result}    Ctn Check Host Severity With Timeout    33    10    60
    Should Be True    ${result}    First step: Host 33 should have severity_id=10

    Ctn Stop Engine
    Ctn Kindly Stop Broker

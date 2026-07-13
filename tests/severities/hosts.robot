*** Settings ***
Documentation       Engine/Broker tests on severities.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
BEUHSEV1
    [Documentation]    Four hosts have a severity added. Then we remove the severity from host 1. Then we change severity 10 to severity8 for host 3.
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear Db    severities
    Ctn Config Engine    ${1}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Add Severity To Hosts    0    10    [1, 2, 3, 4]
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    1
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker
    Sleep    2s

    ${result}    Ctn Check Host Severity With Timeout    1    10    60
    Should Be True    ${result}    Host 1 should have severity_id=10

    Ctn Remove Severities From Hosts    ${0}
    Ctn Add Severity To Hosts    0    10    [2, 4]
    Ctn Add Severity To Hosts    0    8    [3]
    Ctn Reload Engine
    Ctn Reload Broker
    ${result}    Ctn Check Host Severity With Timeout    3    8    60
    Should Be True    ${result}    Host 3 should have severity_id=8
    ${result}    Ctn Check Host Severity With Timeout    1    None    60
    Should Be True    ${result}    Host 1 should have no severity

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUHSEV2
    [Documentation]    Seven hosts are configured with a severity on two pollers. Then we remove severities from the first and second hosts of the first poller but only the severity from the first host of the second poller.
    [Tags]    broker    engine    protobuf    bbdo    severities
    Ctn Config Engine    ${2}
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
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    2
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    module1    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker
    Sleep    5s
    # We need to wait a little before reloading Engine
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
    Ctn Create Severities File    ${0}    ${18}
    Ctn Create Severities File    ${1}    ${18}
    Ctn Add Severity To Hosts    1    17    [28]
    Ctn Reload Engine
    Ctn Reload Broker
    Sleep    3s
    ${result}    Ctn Check Host Severity With Timeout    28    16    60
    Should Be True    ${result}    Second step: Host 28 should have severity_id=16

    ${result}    Ctn Check Host Severity With Timeout    4    None    60
    Should Be True    ${result}    Second step: Host 4 should have severity_id=None

    ${result}    Ctn Check Host Severity With Timeout    3    None    60
    Should Be True    ${result}    Second step: Host 3 should have severity_id=17

    ${result}    Ctn Check Host Severity With Timeout    5    None    60
    Should Be True    ${result}    Second step: Host 5 should have severity_id=17

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BETUHSEV1
    [Documentation]    Hosts have severities provided by templates.
    [Tags]    broker    engine    protobuf    bbdo    severities
    Ctn Config Engine    ${2}
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
    Ctn Config BBDO3    2
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    module1    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker
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

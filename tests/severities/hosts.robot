*** Settings ***
Documentation       Engine/Broker tests on severities.

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes


*** Test Cases ***
BEUHSEV1
    [Documentation]    Four hosts have a severity added. Then we remove the severity from host 1. Then we change severity 10 to severity8 for host 3.
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear DB    severities
    Config Engine    ${1}
    Create Severities File    ${0}    ${20}
    Config Engine Add Cfg File    ${0}    severities.cfg
    Add Severity To Hosts    0    10    [1, 2, 3, 4]
    Init Config
    Config Broker    module
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    module0    neb    debug
    Broker Config Log    central    sql    trace
    Clear Retention
    ${start}=    Get Current Date
    Start Engine
    Start Broker
    Sleep    2s

    ${result}=    check host severity With Timeout    1    10    60
    Should Be True    ${result}    msg=Host 1 should have severity_id=10

    Remove Severities From Hosts    ${0}
    Add Severity To Hosts    0    10    [2, 4]
    Add Severity To Hosts    0    8    [3]
    Reload Engine
    Reload Broker
    ${result}=    check host severity With Timeout    3    8    60
    Should Be True    ${result}    msg=Host 3 should have severity_id=8
    ${result}=    check host severity With Timeout    1    None    60
    Should Be True    ${result}    msg=Host 1 should have no severity

    [Teardown]    Stop Centreon

BEUHSEV2
    [Documentation]    Seven hosts are configured with a severity on two pollers. Then we remove severities from the first and second hosts of the first poller but only the severity from the first host of the second poller.
    [Tags]    broker    engine    protobuf    bbdo    severities
    Config Engine    ${2}
    Create Severities File    ${0}    ${20}
    Create Severities File    ${1}    ${20}
    Config Engine Add Cfg File    ${0}    severities.cfg
    Config Engine Add Cfg File    ${1}    severities.cfg
    Engine Config Set Value    ${0}    log_level_config    debug
    Engine Config Set Value    ${1}    log_level_config    debug
    Add Severity To Hosts    0    18    [2, 4]
    Add Severity To Hosts    0    16    [3, 5]
    Add Severity To Hosts    1    18    [26, 27]
    Add Severity To Hosts    1    16    [28]
    Init Config
    Config Broker    module    ${2}
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    module1    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    module0    neb    debug
    Broker Config Log    module1    neb    debug
    Broker Config Log    central    sql    trace
    Clear Retention
    ${start}=    Get Current Date
    Start Engine
    Start Broker
    Sleep    5s
    # We need to wait a little before reloading Engine
    ${result}=    check host severity With Timeout    2    18    60
    Should Be True    ${result}    msg=First step: Host 2 should have severity_id=18

    ${result}=    check host severity With Timeout    4    18    60
    Should Be True    ${result}    msg=First step: Host 4 should have severity_id=18

    ${result}=    check host severity With Timeout    26    18    60
    Should Be True    ${result}    msg=First step: Host 26 should have severity_id=18

    ${result}=    check host severity With Timeout    27    18    60
    Should Be True    ${result}    msg=First step: Host 27 should have severity_id=18

    ${result}=    check host severity With Timeout    3    16    60
    Should Be True    ${result}    msg=First step: Host 3 should have severity_id=16

    ${result}=    check host severity With Timeout    5    16    60
    Should Be True    ${result}    msg=First step: Host 5 should have severity_id=16

    ${result}=    check host severity With Timeout    28    16    60
    Should Be True    ${result}    msg=First step: Host 28 should have severity_id=16

    Remove Severities From Hosts    ${0}
    Create Severities File    ${0}    ${18}
    Create Severities File    ${1}    ${18}
    Add Severity To Hosts    1    17    [28]
    Reload Engine
    Reload Broker
    Sleep    3s
    ${result}=    check host severity With Timeout    28    16    60
    Should Be True    ${result}    msg=Second step: Host 28 should have severity_id=16

    ${result}=    check host severity With Timeout    4    None    60
    Should Be True    ${result}    msg=Second step: Host 4 should have severity_id=None

    ${result}=    check host severity With Timeout    3    None    60
    Should Be True    ${result}    msg=Second step: Host 3 should have severity_id=17

    ${result}=    check host severity With Timeout    5    None    60
    Should Be True    ${result}    msg=Second step: Host 5 should have severity_id=17

    [Teardown]    Stop Centreon

BETUHSEV1
    [Documentation]    Hosts have severities provided by templates.
    [Tags]    broker    engine    protobuf    bbdo    severities
    Config Engine    ${2}
    Create Severities File    ${0}    ${20}
    Create Severities File    ${1}    ${20}
    Create Template File    ${0}    host    severity    [2, 4]
    Create Template File    ${1}    host    severity    [6, 10]

    Config Engine Add Cfg File    ${0}    severities.cfg
    Config Engine Add Cfg File    ${1}    severities.cfg
    Config Engine Add Cfg File    ${0}    hostTemplates.cfg
    Config Engine Add Cfg File    ${1}    hostTemplates.cfg
    Engine Config Set Value    ${0}    log_level_config    debug
    Engine Config Set Value    ${1}    log_level_config    debug
    Add Template To Hosts    0    host_template_1    [1, 2, 3, 4]
    Add Template To Hosts    0    host_template_2    [5, 6, 7, 8]
    Add Template To Hosts    1    host_template_1    [31, 32]
    Add Template To Hosts    1    host_template_2    [33, 34]
    Init Config
    Config Broker    module    ${2}
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    module1    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    module0    neb    debug
    Broker Config Log    module1    neb    debug
    Broker Config Log    central    sql    trace
    Clear Retention
    ${start}=    Get Current Date
    Start Engine
    Start Broker
    Sleep    5s
    # We need to wait a little before reloading Engine
    ${result}=    check host severity With Timeout    2    2    60
    Should Be True    ${result}    msg=First step: Host 2 should have severity_id=2

    ${result}=    check host severity With Timeout    4    2    60
    Should Be True    ${result}    msg=First step: Host 4 should have severity_id=2

    ${result}=    check host severity With Timeout    5    4    60
    Should Be True    ${result}    msg=First step: Host 5 should have severity_id=4

    ${result}=    check host severity With Timeout    31    6    60
    Should Be True    ${result}    msg=First step: Host 31 should have severity_id=6

    ${result}=    check host severity With Timeout    33    10    60
    Should Be True    ${result}    msg=First step: Host 33 should have severity_id=10

    [Teardown]    Stop Centreon

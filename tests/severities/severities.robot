*** Settings ***
Documentation       Engine/Broker tests on severities.

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save logs If Failed


*** Test Cases ***
BESEV1
    [Documentation]    Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear DB    severities
    Config Engine    ${1}
    Create Severities File    ${0}    ${20}
    Config Engine Add Cfg File    ${0}    severities.cfg
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Broker Config Log    module0    neb    debug
    Broker Config Log    central    sql    debug
    Clear Retention
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${result}=    check severity With Timeout    severity20    5    1    30
    Should Be True    ${result}    msg=severity20 should be of level 5 with icon_id 1
    ${result}=    check severity With Timeout    severity1    1    5    30
    Should Be True    ${result}    msg=severity1 should be of level 1 with icon_id 5
    Stop Engine
    Kindly Stop Broker

BESEV2
    [Documentation]    Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear DB    severities
    Config Engine    ${1}
    Create Severities File    ${0}    ${20}
    Config Engine Add Cfg File    ${0}    severities.cfg
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Broker Config Log    module0    neb    debug
    Broker Config Log    central    sql    debug
    Clear Retention
    ${start}=    Get Current Date
    Start Engine
    Sleep    1s
    Start Broker
    ${result}=    check severity With Timeout    severity20    5    1    30
    Should Be True    ${result}    msg=severity20 should be of level 5 with icon_id 1
    ${result}=    check severity With Timeout    severity1    1    5    30
    Should Be True    ${result}    msg=severity1 should be of level 1 with icon_id 5
    Stop Engine
    Kindly Stop Broker

BEUSEV1
    [Documentation]    Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear DB    severities
    Config Engine    ${1}
    Create Severities File    ${0}    ${20}
    Config Engine Add Cfg File    ${0}    severities.cfg
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    module0    neb    debug
    Broker Config Log    central    sql    debug
    Clear Retention
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${result}=    check severity With Timeout    severity20    5    1    30
    Should Be True    ${result}    msg=severity20 should be of level 5 with icon_id 1
    ${result}=    check severity With Timeout    severity1    1    5    30
    Should Be True    ${result}    msg=severity1 should be of level 1 with icon_id 5
    Stop Engine
    Kindly Stop Broker

BEUSEV2
    [Documentation]    Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear DB    severities
    Config Engine    ${1}
    Create Severities File    ${0}    ${20}
    Config Engine Add Cfg File    ${0}    severities.cfg
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    module0    neb    debug
    Broker Config Log    central    sql    debug
    Clear Retention
    ${start}=    Get Current Date
    Start Engine
    Sleep    1s
    Start Broker
    ${result}=    check severity With Timeout    severity20    5    1    30
    Should Be True    ${result}    msg=severity20 should be of level 5 with icon_id 1
    ${result}=    check severity With Timeout    severity1    1    5    30
    Should Be True    ${result}    msg=severity1 should be of level 1 with icon_id 5
    Stop Engine
    Kindly Stop Broker

BEUSEV3
    [Documentation]    Four services have a severity added. Then we remove the severity from service 1. Then we change severity 11 to severity7 for service 3.
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear DB    severities
    Config Engine    ${1}
    Create Severities File    ${0}    ${20}
    Config Engine Add Cfg File    ${0}    severities.cfg
    Add Severity To Services    0    11    [1, 2, 3, 4]
    Config Broker    central
    Config Broker    rrd
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

    ${result}=    check service severity With Timeout    1    1    11    60
    Should Be True    ${result}    msg=Service (1, 1) should have severity_id=11

    Remove Severities From Services    ${0}
    Add Severity To Services    0    11    [2, 4]
    Add Severity To Services    0    7    [3]
    Reload Engine
    Reload Broker
    ${result}=    check service severity With Timeout    1    3    7    60
    Should Be True    ${result}    msg=Service (1, 3) should have severity_id=7
    ${result}=    check service severity With Timeout    1    1    None    60
    Should Be True    ${result}    msg=Service (1, 1) should have no severity

    Stop Engine
    Kindly Stop Broker

BEUSEV4
    [Documentation]    Seven services are configured with a severity on two pollers. Then we remove severities from the first and second services of the first poller but only the severity from the first service of the second poller. Then only severities no more used should be removed from the database.
    [Tags]    broker    engine    protobuf    bbdo    severities
    Config Engine    ${2}
    Create Severities File    ${0}    ${20}
    Create Severities File    ${1}    ${20}
    Config Engine Add Cfg File    ${0}    severities.cfg
    Config Engine Add Cfg File    ${1}    severities.cfg
    Engine Config Set Value    ${0}    log_level_config    debug
    Engine Config Set Value    ${1}    log_level_config    debug
    Add Severity To Services    0    19    [2, 4]
    Add Severity To Services    0    17    [3, 5]
    Add Severity To Services    1    19    [501, 502]
    Add Severity To Services    1    17    [503]
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    2
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    module1    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    module0    neb    debug
    Broker Config Log    central    sql    trace
    Clear Retention
    ${start}=    Get Current Date
    Start Engine
    Start Broker
    Sleep    5s
    # We need to wait a little before reloading Engine
    ${result}=    check_service_severity_With_Timeout    1    2    19    60
    Should Be True    ${result}    msg=First step: Service (1, 2) should have severity_id=19

    ${result}=    check service severity With Timeout    1    4    19    60
    Should Be True    ${result}    msg=First step: Service (1, 4) should have severity_id=19

    ${result}=    check service severity With Timeout    26    501    19    60
    Should Be True    ${result}    msg=First step: Service (26, 501) should have severity_id=19

    ${result}=    check service severity With Timeout    26    502    19    60
    Should Be True    ${result}    msg=First step: Service (26, 502) should have severity_id=19

    ${result}=    check service severity With Timeout    1    3    17    60
    Should Be True    ${result}    msg=First step: Service (1, 3) should have severity_id=17

    ${result}=    check service severity With Timeout    1    5    17    60
    Should Be True    ${result}    msg=First step: Service (1, 5) should have severity_id=17

    ${result}=    check service severity With Timeout    26    503    17    60
    Should Be True    ${result}    msg=First step: Service (26, 503) should have severity_id=17

    Remove Severities From Services    ${0}
    Create Severities File    ${0}    ${18}
    Create Severities File    ${1}    ${18}
    Add Severity To Services    1    17    [503]
    Reload Engine
    Reload Broker
    Sleep    3s
    ${result}=    check service severity With Timeout    26    503    17    60
    Should Be True    ${result}    msg=Second step: Service (26, 503) should have severity_id=17

    ${result}=    check service severity With Timeout    1    4    None    60
    Should Be True    ${result}    msg=Second step: Service (1, 4) should have severity_id=None

    ${result}=    check service severity With Timeout    1    3    None    60
    Should Be True    ${result}    msg=Second step: Service (1, 3) should have severity_id=17

    ${result}=    check service severity With Timeout    1    5    None    60
    Should Be True    ${result}    msg=Second step: Service (1, 5) should have severity_id=17

    Stop Engine
    Kindly Stop Broker

BETUSEV1
    [Documentation]    Services have severities provided by templates.
    [Tags]    broker    engine    protobuf    bbdo    severities
    Config Engine    ${2}
    Create Severities File    ${0}    ${20}
    Create Severities File    ${1}    ${20}
    Create Template File    ${0}    service    severity    [1, 3]
    Create Template File    ${1}    service    severity    [3, 5]

    Config Engine Add Cfg File    ${0}    severities.cfg
    Config Engine Add Cfg File    ${1}    severities.cfg
    Config Engine Add Cfg File    ${0}    serviceTemplates.cfg
    Config Engine Add Cfg File    ${1}    serviceTemplates.cfg
    Engine Config Set Value    ${0}    log_level_config    debug
    Engine Config Set Value    ${1}    log_level_config    debug
    Add Template To Services    0    service_template_1    [1, 2, 3, 4]
    Add Template To Services    0    service_template_2    [5, 6, 7, 8]
    Add Template To Services    1    service_template_1    [501, 502]
    Add Template To Services    1    service_template_2    [503, 504]
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    module1    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    module0    neb    debug
    Broker Config Log    central    sql    trace
    Clear Retention
    ${start}=    Get Current Date
    Start Engine
    Start Broker
    Sleep    5s
    # We need to wait a little before reloading Engine
    ${result}=    check service severity With Timeout    1    2    1    60
    Should Be True    ${result}    msg=First step: Service (1, 2) should have severity_id=1

    ${result}=    check service severity With Timeout    1    4    1    60
    Should Be True    ${result}    msg=First step: Service (1, 4) should have severity_id=1

    ${result}=    check service severity With Timeout    1    5    3    60
    Should Be True    ${result}    msg=First step: Service (1, 5) should have severity_id=3

    ${result}=    check service severity With Timeout    26    502    3    60
    Should Be True    ${result}    msg=First step: Service (26, 502) should have severity_id=3

    ${result}=    check service severity With Timeout    26    503    5    60
    Should Be True    ${result}    msg=First step: Service (26, 503) should have severity_id=5

    Stop Engine
    Kindly Stop Broker

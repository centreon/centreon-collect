*** Settings ***
Documentation       Engine/Broker tests on severities.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BESEV1
    [Documentation]    Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear Db    severities
    Ctn Config Engine    ${1}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Severity With Timeout    severity20    5    1    30
    Should Be True    ${result}    severity20 should be of level 5 with icon_id 1
    ${result}    Ctn Check Severity With Timeout    severity1    1    5    30
    Should Be True    ${result}    severity1 should be of level 1 with icon_id 5
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BESEV2
    [Documentation]    Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear Db    severities
    Ctn Config Engine    ${1}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Sleep    1s
    Ctn Start Broker
    ${result}    Ctn Check Severity With Timeout    severity20    5    1    30
    Should Be True    ${result}    severity20 should be of level 5 with icon_id 1
    ${result}    Ctn Check Severity With Timeout    severity1    1    5    30
    Should Be True    ${result}    severity1 should be of level 1 with icon_id 5
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUSEV1
    [Documentation]    Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear Db    severities
    Ctn Config Engine    ${1}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    1
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Severity With Timeout    severity20    5    1    30
    Should Be True    ${result}    severity20 should be of level 5 with icon_id 1
    ${result}    Ctn Check Severity With Timeout    severity1    1    5    30
    Should Be True    ${result}    severity1 should be of level 1 with icon_id 5
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUSEV2
    [Documentation]    Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear Db    severities
    Ctn Config Engine    ${1}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    1
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Sleep    1s
    Ctn Start Broker
    ${result}    Ctn Check Severity With Timeout    severity20    5    1    30
    Should Be True    ${result}    severity20 should be of level 5 with icon_id 1
    ${result}    Ctn Check Severity With Timeout    severity1    1    5    30
    Should Be True    ${result}    severity1 should be of level 1 with icon_id 5
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUSEV3
    [Documentation]    Four services have a severity added. Then we remove the severity from service 1. Then we change severity 11 to severity7 for service 3.
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear Db    severities
    Ctn Config Engine    ${1}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Add Severity To Services    0    11    [1, 2, 3, 4]
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

    ${result}    Ctn Check Service Severity With Timeout    1    1    11    60
    Should Be True    ${result}    Service (1, 1) should have severity_id=11

    Ctn Remove Severities From Services    ${0}
    Ctn Add Severity To Services    0    11    [2, 4]
    Ctn Add Severity To Services    0    7    [3]
    Ctn Reload Engine
    Ctn Reload Broker
    ${result}    Ctn Check Service Severity With Timeout    1    3    7    60
    Should Be True    ${result}    Service (1, 3) should have severity_id=7
    ${result}    Ctn Check Service Severity With Timeout    1    1    None    60
    Should Be True    ${result}    Service (1, 1) should have no severity

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUSEV4
    [Documentation]    Seven services are configured with a severity on two pollers. Then we remove severities from the first and second services of the first poller but only the severity from the first service of the second poller. Then only severities no more used should be removed from the database.
    [Tags]    broker    engine    protobuf    bbdo    severities
    Ctn Config Engine    ${2}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Create Severities File    ${1}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Engine Add Cfg File    ${1}    severities.cfg
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Engine Config Set Value    ${1}    log_level_config    debug
    Ctn Add Severity To Services    0    19    [2, 4]
    Ctn Add Severity To Services    0    17    [3, 5]
    Ctn Add Severity To Services    1    19    [501, 502]
    Ctn Add Severity To Services    1    17    [503]
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    2
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    2
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker
    Sleep    5s
    # We need to wait a little before reloading Engine
    ${result}    Ctn Check Service Severity With Timeout    1    2    19    60
    Should Be True    ${result}    First step: Service (1, 2) should have severity_id=19

    ${result}    Ctn Check Service Severity With Timeout    1    4    19    60
    Should Be True    ${result}    First step: Service (1, 4) should have severity_id=19

    ${result}    Ctn Check Service Severity With Timeout    26    501    19    60
    Should Be True    ${result}    First step: Service (26, 501) should have severity_id=19

    ${result}    Ctn Check Service Severity With Timeout    26    502    19    60
    Should Be True    ${result}    First step: Service (26, 502) should have severity_id=19

    ${result}    Ctn Check Service Severity With Timeout    1    3    17    60
    Should Be True    ${result}    First step: Service (1, 3) should have severity_id=17

    ${result}    Ctn Check Service Severity With Timeout    1    5    17    60
    Should Be True    ${result}    First step: Service (1, 5) should have severity_id=17

    ${result}    Ctn Check Service Severity With Timeout    26    503    17    60
    Should Be True    ${result}    First step: Service (26, 503) should have severity_id=17

    Ctn Remove Severities From Services    ${0}
    Ctn Create Severities File    ${0}    ${18}
    Ctn Create Severities File    ${1}    ${18}
    Ctn Add Severity To Services    1    17    [503]
    Ctn Reload Engine
    Ctn Reload Broker
    Sleep    3s
    ${result}    Ctn Check Service Severity With Timeout    26    503    17    60
    Should Be True    ${result}    Second step: Service (26, 503) should have severity_id=17

    ${result}    Ctn Check Service Severity With Timeout    1    4    None    60
    Should Be True    ${result}    Second step: Service (1, 4) should have severity_id=None

    ${result}    Ctn Check Service Severity With Timeout    1    3    None    60
    Should Be True    ${result}    Second step: Service (1, 3) should have severity_id=17

    ${result}    Ctn Check Service Severity With Timeout    1    5    None    60
    Should Be True    ${result}    Second step: Service (1, 5) should have severity_id=17

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BETUSEV1
    [Documentation]    Services have severities provided by templates.
    [Tags]    broker    engine    protobuf    bbdo    severities
    Ctn Config Engine    ${2}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Create Severities File    ${1}    ${20}
    Ctn Create Template File    ${0}    service    severity    [1, 3]
    Ctn Create Template File    ${1}    service    severity    [3, 5]

    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Engine Add Cfg File    ${1}    severities.cfg
    Ctn Config Engine Add Cfg File    ${0}    serviceTemplates.cfg
    Ctn Config Engine Add Cfg File    ${1}    serviceTemplates.cfg
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Engine Config Set Value    ${1}    log_level_config    debug
    Ctn Add Template To Services    0    service_template_1    [1, 2, 3, 4]
    Ctn Add Template To Services    0    service_template_2    [5, 6, 7, 8]
    Ctn Add Template To Services    1    service_template_1    [501, 502]
    Ctn Add Template To Services    1    service_template_2    [503, 504]
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    2
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker
    Sleep    5s
    # We need to wait a little before reloading Engine
    ${result}    Ctn Check Service Severity With Timeout    1    2    1    60
    Should Be True    ${result}    First step: Service (1, 2) should have severity_id=1

    ${result}    Ctn Check Service Severity With Timeout    1    4    1    60
    Should Be True    ${result}    First step: Service (1, 4) should have severity_id=1

    ${result}    Ctn Check Service Severity With Timeout    1    5    3    60
    Should Be True    ${result}    First step: Service (1, 5) should have severity_id=3

    ${result}    Ctn Check Service Severity With Timeout    26    502    3    60
    Should Be True    ${result}    First step: Service (26, 502) should have severity_id=3

    ${result}    Ctn Check Service Severity With Timeout    26    503    5    60
    Should Be True    ${result}    First step: Service (26, 503) should have severity_id=5

    Ctn Stop Engine
    Ctn Kindly Stop Broker

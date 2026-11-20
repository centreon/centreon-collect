*** Settings ***
Documentation       Engine/Broker tests on severities with centralized configuration.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CBESEV1
    [Documentation]    Scenario: Severities stored in database when Broker starts first (centralized)
    ...    Given Engine is configured with centralized setup and 20 severities
    ...    And Broker components (central, rrd, module) are configured
    ...    And retention data is cleared
    ...    When Broker is started before Engine
    ...    Then severity20 should be of level 5 with icon_id 1
    ...    And severity1 should be of level 1 with icon_id 5
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear Db    severities
    Ctn Config Centralized Engine    ${1}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    Ctn Wait For Engine Configuration To Be Applied    ${start}    0

    ${result}    Ctn Check Severity With Timeout    severity20    5    1    30
    Should Be True    ${result}    severity20 should be of level 5 with icon_id 1
    ${result}    Ctn Check Severity With Timeout    severity1    1    5    30
    Should Be True    ${result}    severity1 should be of level 1 with icon_id 5
    ${result}    Ctn Check Severity In Cache With Timeout    51001    20    1    5    30
    Should Be True    ${result}    severity20 (host, level=5) should be present in broker cache
    ${result}    Ctn Check Severity In Cache With Timeout    51001    1    0    1    30
    Should Be True    ${result}    severity1 (service, level=1) should be present in broker cache
    Ctn Stop Engine
    Ctn Kindly Stop Broker

CBESEV2
    [Documentation]    Scenario: Severities stored in database when Engine starts first (centralized)
    ...    Given Engine is configured with centralized setup and 20 severities
    ...    And Broker components (central, rrd, module) are configured
    ...    And retention data is cleared
    ...    When Engine is started before Broker
    ...    Then severity20 should be of level 5 with icon_id 1
    ...    And severity1 should be of level 1 with icon_id 5
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear Db    severities
    Ctn Config Centralized Engine    ${1}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Start Broker    newGeneration=True

    Ctn Wait For Engine Configuration To Be Applied    ${start}    0

    ${result}    Ctn Check Severity With Timeout    severity20    5    1    30
    Should Be True    ${result}    severity20 should be of level 5 with icon_id 1
    ${result}    Ctn Check Severity With Timeout    severity1    1    5    30
    Should Be True    ${result}    severity1 should be of level 1 with icon_id 5
    ${result}    Ctn Check Severity In Cache With Timeout    51001    20    1    5    30
    Should Be True    ${result}    severity20 (host, level=5) should be present in broker cache
    ${result}    Ctn Check Severity In Cache With Timeout    51001    1    0    1    30
    Should Be True    ${result}    severity1 (service, level=1) should be present in broker cache
    Ctn Stop Engine
    Ctn Kindly Stop Broker

CBEUSEV1
    [Documentation]    Scenario: Severities stored via unified SQL when Broker starts first (centralized)
    ...    Given Engine is configured with centralized setup and 20 severities
    ...    And Broker is configured with unified SQL output and BBDO3
    ...    And retention data is cleared
    ...    When Broker is started before Engine
    ...    Then severity20 should be of level 5 with icon_id 1
    ...    And severity1 should be of level 1 with icon_id 5
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear Db    severities
    Ctn Config Centralized Engine    ${1}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    Ctn Wait For Engine Configuration To Be Applied    ${start}    0

    ${result}    Ctn Check Severity With Timeout    severity20    5    1    30
    Should Be True    ${result}    severity20 should be of level 5 with icon_id 1
    ${result}    Ctn Check Severity With Timeout    severity1    1    5    30
    Should Be True    ${result}    severity1 should be of level 1 with icon_id 5
    ${result}    Ctn Check Severity In Cache With Timeout    51001    20    1    5    30
    Should Be True    ${result}    severity20 (host, level=5) should be present in broker cache
    ${result}    Ctn Check Severity In Cache With Timeout    51001    1    0    1    30
    Should Be True    ${result}    severity1 (service, level=1) should be present in broker cache
    ${result}    Ctn Check Severity Db Id In Cache With Timeout    51001    20    1    30
    Should Be True    ${result}    severity20 (host) should have a non-zero db_id in broker cache
    ${result}    Ctn Check Severity Db Id In Cache With Timeout    51001    1    0    30
    Should Be True    ${result}    severity1 (service) should have a non-zero db_id in broker cache
    Ctn Stop Engine
    Ctn Kindly Stop Broker

CBEUSEV2
    [Documentation]    Scenario: Severities stored via unified SQL when Engine starts first (centralized)
    ...    Given Engine is configured with centralized setup and 20 severities
    ...    And Broker is configured with unified SQL output and BBDO3
    ...    And retention data is cleared
    ...    When Engine is started before Broker
    ...    Then severity20 should be of level 5 with icon_id 1
    ...    And severity1 should be of level 1 with icon_id 5
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear Db    severities
    Ctn Config Centralized Engine    ${1}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Start Broker    newGeneration=True

    Ctn Wait For Engine Configuration To Be Applied    ${start}    0

    ${result}    Ctn Check Severity With Timeout    severity20    5    1    30
    Should Be True    ${result}    severity20 should be of level 5 with icon_id 1
    ${result}    Ctn Check Severity With Timeout    severity1    1    5    30
    Should Be True    ${result}    severity1 should be of level 1 with icon_id 5
    ${result}    Ctn Check Severity In Cache With Timeout    51001    20    1    5    30
    Should Be True    ${result}    severity20 (host, level=5) should be present in broker cache
    ${result}    Ctn Check Severity In Cache With Timeout    51001    1    0    1    30
    Should Be True    ${result}    severity1 (service, level=1) should be present in broker cache
    ${result}    Ctn Check Severity Db Id In Cache With Timeout    51001    20    1    30
    Should Be True    ${result}    severity20 (host) should have a non-zero db_id in broker cache
    ${result}    Ctn Check Severity Db Id In Cache With Timeout    51001    1    0    30
    Should Be True    ${result}    severity1 (service) should have a non-zero db_id in broker cache
    Ctn Stop Engine
    Ctn Kindly Stop Broker

CBEUSEV3
    [Documentation]    Scenario: Service severity removal and change via unified SQL (centralized)
    ...    Given Engine is configured with centralized setup and 20 severities
    ...    And Broker is configured with unified SQL output and BBDO3
    ...    And severity 11 is assigned to services 1, 2, 3 and 4
    ...    When Engine and Broker are started
    ...    Then service (1, 1) should have severity_id=11
    ...    When severity is removed from all services and reassigned (11 to services 2,4 and 7 to service 3)
    ...    And Engine and Broker are reloaded
    ...    Then service (1, 3) should have severity_id=7
    ...    And service (1, 1) should have no severity
    [Tags]    broker    engine    protobuf    bbdo    severities
    # Clear Db    severities
    Ctn Config Centralized Engine    ${1}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Add Severity To Services    0    11    [1, 2, 3, 4]
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Start Broker    newGeneration=True

    Ctn Wait For Engine Configuration To Be Applied    ${start}    0
    ${result}    Ctn Check Service Severity With Timeout    1    1    11    60
    Should Be True    ${result}    Service (1, 1) should have severity_id=11
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    1    1    11    30
    Should Be True    ${result}    Service (1, 1) should have severity_id=11 in broker cache

    Ctn Remove Severities From Services    ${0}
    Ctn Add Severity To Services    0    11    [2, 4]
    Ctn Add Severity To Services    0    7    [3]

    ${start}    Ctn Get Round Current Date
    Ctn Wait For Engine Configuration To Be Applied    ${start}    0

    ${result}    Ctn Check Service Severity With Timeout    1    2    11    60
    Should Be True    ${result}    Service (1, 2) should have severity_id=11
    ${result}    Ctn Check Service Severity With Timeout    1    3    7    60
    Should Be True    ${result}    Service (1, 3) should have severity_id=7
    ${result}    Ctn Check Service Severity With Timeout    1    1    None    60
    Should Be True    ${result}    Service (1, 1) should have no severity
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    1    2    11    30
    Should Be True    ${result}    Service (1, 2) should have severity_id=11 in broker cache
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    1    3    7    30
    Should Be True    ${result}    Service (1, 3) should have severity_id=7 in broker cache
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    1    1    0    30
    Should Be True    ${result}    Service (1, 1) should have no severity in broker cache

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CBEUSEV4
    [Documentation]    Scenario: Severity removal across two pollers via unified SQL (centralized)
    ...    Given Engine is configured with centralized setup across 2 pollers and 20 severities each
    ...    And severity 19 is assigned to services 2,4 on poller 1 and services 501,502 on poller 2
    ...    And severity 17 is assigned to services 3,5 on poller 1 and service 503 on poller 2
    ...    And Broker is configured with unified SQL output and BBDO3
    ...    When Engine and Broker are started
    ...    Then all services should have their expected severity_id values
    ...    When severities are removed from poller 1 services and severity files reduced to 18
    ...    And severity 17 is kept on service 503 of poller 2
    ...    And Engine and Broker are reloaded
    ...    Then service (26, 503) should still have severity_id=17
    ...    And services on poller 1 that lost their severity should have severity_id=None
    [Tags]    broker    engine    protobuf    bbdo    severities
    Ctn Config Centralized Engine    ${2}
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
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Start Broker    newGeneration=True

    Ctn Wait For Engine Configuration To Be Applied    ${start}    0    2

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

    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    1    2    19    30
    Should Be True    ${result}    First step: Service (1, 2) should have severity_id=19 in broker cache
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    1    4    19    30
    Should Be True    ${result}    First step: Service (1, 4) should have severity_id=19 in broker cache
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    26    501    19    30
    Should Be True    ${result}    First step: Service (26, 501) should have severity_id=19 in broker cache
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    26    502    19    30
    Should Be True    ${result}    First step: Service (26, 502) should have severity_id=19 in broker cache
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    1    3    17    30
    Should Be True    ${result}    First step: Service (1, 3) should have severity_id=17 in broker cache
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    1    5    17    30
    Should Be True    ${result}    First step: Service (1, 5) should have severity_id=17 in broker cache
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    26    503    17    30
    Should Be True    ${result}    First step: Service (26, 503) should have severity_id=17 in broker cache

    Ctn Remove Severities From Services    ${0}
    Ctn Create Severities File    ${0}    ${18}
    Ctn Create Severities File    ${1}    ${18}
    Ctn Add Severity To Services    1    17    [503]

    ${start}    Ctn Get Round Current Date
    Ctn Wait For Engine Configuration To Be Applied    ${start}    0

    ${result}    Ctn Check Service Severity With Timeout    26    503    17    60
    Should Be True    ${result}    Second step: Service (26, 503) should have severity_id=17

    ${result}    Ctn Check Service Severity With Timeout    1    4    None    60
    Should Be True    ${result}    Second step: Service (1, 4) should have severity_id=None

    ${result}    Ctn Check Service Severity With Timeout    1    3    None    60
    Should Be True    ${result}    Second step: Service (1, 3) should have severity_id=None

    ${result}    Ctn Check Service Severity With Timeout    1    5    None    60
    Should Be True    ${result}    Second step: Service (1, 5) should have severity_id=None

    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    26    503    17    30
    Should Be True    ${result}    Second step: Service (26, 503) should have severity_id=17 in broker cache
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    1    4    0    30
    Should Be True    ${result}    Second step: Service (1, 4) should have no severity in broker cache
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    1    3    0    30
    Should Be True    ${result}    Second step: Service (1, 3) should have no severity in broker cache
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    1    5    0    30
    Should Be True    ${result}    Second step: Service (1, 5) should have no severity in broker cache

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CBETUSEV1
    [Documentation]    Scenario: Service severities inherited from templates via unified SQL (centralized)
    ...    Given Engine is configured with centralized setup across 2 pollers and 20 severities each
    ...    And service templates with severity assignments are configured
    ...    And Broker is configured with unified SQL output and BBDO3
    ...    When Engine and Broker are started
    ...    Then services inheriting template_1 on poller 1 should have severity_id=1
    ...    And services inheriting template_2 on poller 1 should have severity_id=3
    ...    And services inheriting template_1 on poller 2 should have severity_id=3
    ...    And services inheriting template_2 on poller 2 should have severity_id=5
    [Tags]    broker    engine    protobuf    bbdo    severities
    Ctn Config Centralized Engine    ${2}
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
    Ctn Config Broker    module    2
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Start Broker    newGeneration=True

    Ctn Wait For Engine Configuration To Be Applied    ${start}    0

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

    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    1    2    1    30
    Should Be True    ${result}    Service (1, 2) should have severity_id=1 in broker cache
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    1    4    1    30
    Should Be True    ${result}    Service (1, 4) should have severity_id=1 in broker cache
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    1    5    3    30
    Should Be True    ${result}    Service (1, 5) should have severity_id=3 in broker cache
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    26    502    3    30
    Should Be True    ${result}    Service (26, 502) should have severity_id=3 in broker cache
    ${result}    Ctn Check Service Severity In Cache With Timeout    51001    26    503    5    30
    Should Be True    ${result}    Service (26, 503) should have severity_id=5 in broker cache

    Ctn Stop Engine
    Ctn Kindly Stop Broker

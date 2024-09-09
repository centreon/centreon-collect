*** Settings ***
Documentation       Centreon Broker and Engine start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BENCV
    [Documentation]    Engine is configured with hosts/services. The first host has no customvariable.
    ...    Then we add a customvariable to the first host and we reload engine.
    ...    Then the host should have this new customvariable defined and centengine should not crash.
    [Tags]    broker    engine    MON-147499
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Engine Config Set Value In Hosts    ${0}    host_1    _CV    cv_value
    ${start}    Get Current Date
    Ctn Reload Engine
    ${content}    Create List    new custom variable 'CV' on host 1
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The host should have a new customvariable named _CV

    Ctn Kindly Stop Broker
    Ctn Stop Engine

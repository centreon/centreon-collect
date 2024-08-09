*** Settings ***
Documentation       Centreon Broker and Engine start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BENegEngConf
    [Documentation]    During the negotiation, Engine sends its configuration version to Broker.
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Engine Add Engine Conf Dir    ${1}
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    Engine configuration directory set to '${ETC_ROOT}/centreon-engine/config0'
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    A message telling that The Engine configuration directory is set should be available in logs.
    ${result}    Ctn Check Connections
    Should Be True    ${result}
    Ctn Kindly Stop Broker
    Ctn Stop Engine

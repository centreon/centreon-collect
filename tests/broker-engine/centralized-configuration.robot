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
    Ctn Config BBDO3    ${1}
    Ctn Config Engine Add Engine Conf Dir    ${1}
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    Engine configuration directory set to '${ETC_ROOT}/centreon-engine/config0'
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    A message telling that The Engine configuration directory is set should be available in logs.
    ${result}    Ctn Check Connections
    Should Be True    ${result}    It seems that Broker and Engine are not connected.
    Ctn Kindly Stop Broker
    Ctn Stop Engine

BENegPollersConf
    [Documentation]    During the negotiation, when Broker knows the php cache directory for the pollers configurations,
    ...    it also creates a directory to copy them with also a subdirectory 'working'.
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    ${1}
    Ctn Config Engine Add Engine Conf Dir    ${1}
    Ctn Broker Config Add Item    central    pollers_conf_dir    ${VAR_ROOT}/cache/centreon/config/engine
    Remove Directory    ${VAR_ROOT}/lib/centreon-broker/pollers    True
    Remove Directory    ${VAR_ROOT}/cache/centreon/config/engine/1    True
    Create Directory    ${VAR_ROOT}/cache/centreon/config/engine/1
    Copy Files    ${ETC_ROOT}/centreon-engine/config0/*.cfg    ${VAR_ROOT}/cache/centreon/config/engine/1
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine

    ${content}    Create List    Pollers configuration php cache detected at '${VAR_ROOT}/cache/centreon/config/engine/'
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    A message telling that The pollers configuration php cache has been detected should be available in logs.
    ${result}    Ctn Check Connections
    Should Be True    ${result}

    # There are two important directories here:
    # * pollers that contains the various configurations.
    # * working that is a temporary directory when we read a poller configuration.
    Directory Should Exist    ${VAR_ROOT}/lib/centreon-broker/pollers/working

    Ctn Kindly Stop Broker
    Ctn Stop Engine

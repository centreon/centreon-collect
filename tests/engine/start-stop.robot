*** Settings ***
Documentation       Centreon Engine only start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
ESS1
    [Documentation]    Start-Stop (0s between start/stop) 5 times one instance of engine and no coredump
    [Tags]    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    Repeat Keyword    5 times    Ctn Start Stop Instances    0

ESS2
    [Documentation]    Start-Stop (300ms between start/stop) 5 times one instance of engine and no coredump
    [Tags]    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    Repeat Keyword    5 times    Ctn Start Stop Instances    300ms

ESS3
    [Documentation]    Start-Stop (0s between start/stop) 5 times three instances of engine and no coredump
    [Tags]    engine    start-stop
    Ctn Config Engine    ${3}
    Ctn Config Broker    module    ${3}
    Repeat Keyword    5 times    Ctn Start Stop Instances    300ms

ESS4
    [Documentation]    Start-Stop (300ms between start/stop) 5 times three instances of engine and no coredump
    [Tags]    engine    start-stop
    Ctn Config Engine    ${3}
    Ctn Config Broker    module    ${3}
    Repeat Keyword    5 times    Ctn Start Stop Instances    300ms

ESSCTO
    [Documentation]    Scenario: Engine services timeout due to missing Perl connector
    ...    Given the Engine is configured as usual without the Perl connector
    ...    When the Engine executes its service commands
    ...    Then the commands take too long and reach the timeout
    ...    And the Engine starts and stops two times as a result
    [Tags]    engine    start-stop    MON-168055
    Ctn Config Engine    ${1}
    Ctn Engine Command Add Arg    ${0}    *    --duration 1000
    Ctn Engine Command Remove Connector    ${0}    *
    Ctn Config Broker    module
    Repeat Keyword    4 times    Ctn Start Stop Instances    20s

ESSCTOWC
    [Documentation]    Scenario: Engine services timeout due to missing Perl connector
    ...    Given the Engine is configured as usual with some command using the Perl connector
    ...    When the Engine executes its service commands
    ...    Then the commands take too long and reach the timeout
    ...    And the Engine starts and stops two times as a result
    [Tags]    engine    start-stop    MON-168055
    Ctn Config Engine    ${1}
    Ctn Engine Command Add Arg    ${0}    *    --duration 1000
    Ctn Config Broker    module
    Repeat Keyword    4 times    Ctn Start Stop Instances    20s

*** Keywords ***
Ctn Start Stop Instances
    [Arguments]    ${interval}
    Ctn Start engine
    Sleep    ${interval}
    Ctn Stop engine

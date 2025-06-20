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

E_FD_LIMIT
    [Documentation]    Engine here is started with a low file descriptor limit.
    ...    The engine should not crash and limit should be set.
    [Tags]    engine    start-stop    MON-37938
    Ctn Config Engine    ${1}
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    max_file_descriptors    1048576    True

    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${pid}    Get Process Id    e0
    ${limits}    Ctn Get Process Limit    ${pid}    Max open files

    Should Be Equal As Numbers    ${limits[0]}    1048576    Engine should have 1048576 file descriptors

    Ctn Stop Engine

ESSCTO
    [Documentation]    Scenario: Engine services timeout due to missing Perl connector
    ...    Given the Engine is configured as usual without the Perl connector
    ...    When the Engine executes its service commands
    ...    Then the commands take too long and reach the timeout
    ...    And the Engine starts and stops two times as a result
    [Tags]    engine    start-stop    MON-167816
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
    [Tags]    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Engine Command Add Arg    ${0}    *    --duration 1000
    Ctn Config Broker    module
    Repeat Keyword    4 times    Ctn Start Stop Instances    20s

ESSOCWNV
    [Documentation]    Scenario: Engine is started with a valid old configuration (concerning cbmod)
    ...    Given the Engine is configured with a valid old configuration
    ...    When the Engine is started
    ...    Then the Engine starts correctly
    ...    And the Engine stops correctly
    [Tags]    engine    start-stop    MON-173354
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/plugins/centreon-broker/cbmod.so ${ETC_ROOT}/centreon-broker/central-module0.json    True    True
    Ctn Engine Config Delete Key    ${0}    broker_module_cfg_file
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    ${content}    Create List    is deprecated and will be removed in future versions.
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    ${60}
    Should Be True    ${result}    The engine should log the deprecation message but also use its value

    ${content}    Create List    Parsing the configuration file '/tmp/etc/centreon-broker/central-module0.json' of the 'cbmod' module to still be able to use it.
    ${result}    Ctn Find In Log With Timeout    ${VAR_ROOT}/log/centreon-engine/config0/centengine-stdout.log    ${start}    ${content}    ${60}
    Should Be True    ${result}    The engine should log the use of the old cbmod configuration.
    Sleep    10s
    Ctn Stop Engine

*** Keywords ***
Ctn Start Stop Instances
    [Arguments]    ${interval}
    Ctn Start Engine
    Sleep    ${interval}
    Ctn Stop Engine

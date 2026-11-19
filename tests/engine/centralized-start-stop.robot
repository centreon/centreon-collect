*** Settings ***
Documentation       Centreon Engine only start/stop tests with centralized configuration

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed

*** Test Cases ***
CESS1
    [Documentation]    Given one Engine instance is configured with a module broker
    ...    When the Engine is started and stopped 5 times with no delay
    ...    Then no coredump is produced
    [Tags]    engine    start-stop
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Start Broker    newGeneration=True
    Repeat Keyword    5 times    Ctn Start Stop Instances    0

CESS2
    [Documentation]    Given one Engine instance is configured with a module broker
    ...    When the Engine is started and stopped 5 times with 300ms delay
    ...    Then no coredump is produced
    [Tags]    engine    start-stop
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Start Broker    newGeneration=True
    Repeat Keyword    5 times    Ctn Start Stop Instances    300ms

CESS3
    [Documentation]    Given three Engine instances are configured with a module broker
    ...    When the Engine is started and stopped 5 times with no delay
    ...    Then no coredump is produced
    [Tags]    engine    start-stop
    Ctn Config Centralized Engine    ${3}
    Ctn Config Broker    module    ${3}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Start Broker    newGeneration=True
    Repeat Keyword    5 times    Ctn Start Stop Instances    300ms

CESS4
    [Documentation]    Given three Engine instances are configured with a module broker
    ...    When the Engine is started and stopped 5 times with 300ms delay
    ...    Then no coredump is produced
    [Tags]    engine    start-stop
    Ctn Config Centralized Engine    ${3}
    Ctn Config Broker    module    ${3}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Start Broker    newGeneration=True
    Repeat Keyword    5 times    Ctn Start Stop Instances    300ms

CE_FD_LIMIT
    [Documentation]    Given the Engine is configured with a low file descriptor limit
    ...    When the Engine is started
    ...    Then the Engine should not crash
    ...    And the file descriptor limit should be set correctly
    [Tags]    engine    start-stop    MON-37938
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    module    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    bbdo    info
    Ctn Engine Config Set Value    ${0}    max_file_descriptors    1048576    True

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${pid}    Get Process Id    e0
    ${limits}    Ctn Get Process Limit    ${pid}    Max open files
    ${expected_fds}    Evaluate    min(1048576, ${limits[1]})

    Should Be Equal As Numbers    ${limits[0]}    ${expected_fds}    Engine should have ${expected_fds} file descriptors

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CESSCTO
    [Documentation]    Given the Engine is configured without the Perl connector
    ...    When the Engine executes its service commands
    ...    Then the commands take too long and reach the timeout
    ...    And the Engine starts and stops as a result
    [Tags]    engine    start-stop    MON-167816
    Ctn Config Centralized Engine    ${1}
    Ctn Engine Command Add Arg    ${0}    *    --duration 1000
    Ctn Engine Command Remove Connector    ${0}    *
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Start Broker    newGeneration=True
    Repeat Keyword    4 times    Ctn Start Stop Instances    20s

CESSCTOWC
    [Documentation]    Given the Engine is configured with some commands using the Perl connector
    ...    When the Engine executes its service commands
    ...    Then the commands take too long and reach the timeout
    ...    And the Engine starts and stops as a result
    [Tags]    engine    start-stop
    Ctn Config Centralized Engine    ${1}
    Ctn Engine Command Add Arg    ${0}    *    --duration 1000
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Start Broker    newGeneration=True
    Repeat Keyword    4 times    Ctn Start Stop Instances    20s

CESS_STATS
    [Documentation]    Given the Engine is started with centralized configuration
    ...    When we read the Engine's stats file
    ...    Then the Engine must not crash
    [Tags]    engine    MON-171621
    Ctn Config Centralized Engine    ${1}    ${2}    ${2}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Clear Retention
    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Wait Until Created    /tmp/var/lib/centreon-engine/central-module-master-stats.json
    ${result}    Grep File    /tmp/var/lib/centreon-engine/central-module-master-stats.json    "name":"/usr/share/centreon/lib/centreon-broker/15-stats.so"
    Ctn Stop Engine
    Ctn Kindly Stop Broker

CESSOCWNV
    [Documentation]    Given the Engine is configured with a valid old configuration concerning cbmod
    ...    When the Engine is started
    ...    Then the Engine starts correctly
    ...    And the Engine stops correctly
    [Tags]    engine    start-stop    MON-173354
    Ctn Config Centralized Engine    ${1}
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/plugins/centreon-broker/cbmod.so ${ETC_ROOT}/centreon-broker/central-module0.json    True    True
    Ctn Engine Config Delete Key    ${0}    broker_module_cfg_file
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    config    warning
    Ctn Clear Prot Files
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${content}    Create List    is deprecated and will be removed in future versions.
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    ${60}
    Should Be True    ${result}    The engine should log the deprecation message but also use its value

    ${content}    Create List    Parsing the configuration file '/tmp/etc/centreon-broker/central-module0.json' of the 'cbmod' module to still be able to use it.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    ${60}
    Should Be True    ${result}    The broker should log the use of the old cbmod configuration.
    Sleep    10s
    Ctn Stop Engine
    Ctn Kindly Stop Broker

*** Keywords ***
Ctn Start Stop Instances
    [Arguments]    ${interval}
    Ctn Start Engine    newGeneration=True
    Sleep    ${interval}
    Ctn Stop Engine

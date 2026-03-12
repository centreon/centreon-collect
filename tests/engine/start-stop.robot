*** Settings ***
Documentation       Centreon Engine only start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed

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
    Ctn Engine Config Set Value    0    log_level_commands    trace
    Ctn Config Broker    module
    Repeat Keyword    5 times    Ctn Start Stop Instances    300ms     ${True}

ESS3
    [Documentation]    Start-Stop (0s between start/stop) 5 times three instances of engine and no coredump
    [Tags]    engine    start-stop
    Ctn Config Engine    ${3}
    Ctn Engine Config Set Value    0    log_level_commands    trace
    Ctn Config Broker    module    ${3}
    Repeat Keyword    5 times    Ctn Start Stop Instances    300ms     ${True}

ESS4
    [Documentation]    Start-Stop (300ms between start/stop) 5 times three instances of engine and no coredump
    [Tags]    engine    start-stop
    Ctn Config Engine    ${3}
    Ctn Engine Config Set Value    0    log_level_commands    trace
    Ctn Config Broker    module    ${3}
    Repeat Keyword    5 times    Ctn Start Stop Instances    300ms     ${True}

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
    Ctn Engine Config Set Value    0    log_level_commands    trace
    Ctn Engine Command Add Arg    ${0}    *    --duration 1000
    Ctn Engine Command Remove Connector    ${0}    *
    Ctn Config Broker    module
    Repeat Keyword    4 times    Ctn Start Stop Instances    20s     ${True}

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
    Repeat Keyword    4 times    Ctn Start Stop Instances    20s     ${True}

ESS_STATS
    [Documentation]    Scenario: Reading the stats file after Engine has started
    ...    Given the Engine is started
    ...    When we read the Engine's stats file
    ...    Then Engine must not crash
    [Tags]    engine    MON-171621
    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Config Broker    module
    Ctn Config BBDO3    1    only_engine=True
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Clear Retention
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Wait Until Created    /tmp/var/lib/centreon-engine/central-module-master-stats.json
    ${result}    Grep File    /tmp/var/lib/centreon-engine/central-module-master-stats.json    "name":"/usr/share/centreon/lib/centreon-broker/15-stats.so"
    Ctn Stop Engine

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

NO_BROKER_LOG
    [Documentation]    Given a configuration without broker logs directory, engine must start correctly
    [Tags]    engine    start-stop    MON-187627
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    Remove Directory     ${BROKER_LOG}     recursive=${True}

    ${start}    Ctn Get Round Current Date
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Stop Engine

NO_HOST_CHECK_COMMAND
    [Documentation]     Given a host without check command, engine should not crash
    [Tags]     engine    MON-192949
    Ctn Config Engine    ${1}     ${1}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1

    Ctn Engine Config Del Value In Hosts    ${0}    host_1    check_command    

    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Start Broker

    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database

    [Teardown]    Ctn Stop Engine Broker And Save Logs

*** Keywords ***
Ctn Start Stop Instances
    [Arguments]    ${interval}     ${check_raw_delete}=${False}
    Ctn Start Engine
    Sleep    ${interval}
    Ctn Stop Engine
    IF     ${check_raw_delete}
        Ctn Check Raw And Checker Delete
    END


Ctn Check Raw And Checker Delete
    ${nb_raw_create}    Run Process    grep    -c     create raw_v2     ${engineLog0}
    ${nb_raw_delete}    Run Process    grep    -c    delete raw_v2     ${engineLog0}
    ${nb_checker_delete}    Run Process    grep    -c    delete checker    ${engineLog0}
    Log To Console    nb raw_v2 created: ${nb_raw_create.stdout} nb raw_v2 deleted: ${nb_raw_delete.stdout}

    Should Be True     ${nb_raw_delete.stdout} == ${nb_raw_create.stdout}     some raw_v2 object have not been deleted
    Should Be True     ${nb_checker_delete.rc} == 0      checker has not been deleted
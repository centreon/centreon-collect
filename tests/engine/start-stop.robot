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
    [Tags]    engine    start-stop    MON-168054
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
    [Tags]    engine    start-stop    MON-168054
    Ctn Config Engine    ${1}
    Ctn Engine Command Add Arg    ${0}    *    --duration 1000
    Ctn Config Broker    module
    Repeat Keyword    4 times    Ctn Start Stop Instances    20s     ${True}

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

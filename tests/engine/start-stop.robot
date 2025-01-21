*** Settings ***
Documentation       Centreon Engine only start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes

Library    XML

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

ESS5
    [Documentation]    Engine here is started with cbmod configured with evoluated parameters.
    ...    The legacy way to start cbmod is cbmod /etc/centreon-broker/central-module.json.
    ...    Now we can also start it with cbmod -c /etc/centreon-broker/central-module.json -e /etc/centreon-engine.
    [Tags]    engine    start-stop    MON-15671
    Ctn Config Engine    ${1}
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/cbmod.so -c /etc/centreon-broker/central-module0.json -e /etc/centreon-engine    disambiguous=True
    Repeat Keyword    3 times    Ctn Start Stop Instances    2s

E_FD_LIMIT
    [Documentation]    Engine here is started with a low file descriptor limit.
    ...    The engine should not crash and limit should be set.
    [Tags]    engine    start-stop    MON-15671
    Ctn Config Engine    ${1}
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    max_file_descriptors    4096    True

    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    
    ${pid}    Get Process Id    e0
    ${limits}    Get Process Limit    ${pid}    Max open files
    
    Should Be Equal As Numbers    ${limits[0]}    4096    Engine should have 4096 file descriptors

    Ctn Stop Engine


*** Keywords ***
Ctn Start Stop Instances
    [Arguments]    ${interval}
    Ctn Start Engine
    Sleep    ${interval}
    Ctn Stop Engine

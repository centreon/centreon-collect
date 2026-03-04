*** Settings ***
Documentation       Centreon Engine many service checks tests with centralized configuration

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed

*** Test Cases ***

CENGINE_MANY_CHECKS
    [Documentation]    Given Engine is configured in centralized mode with many services and a unique check on each service with its own env variables
    ...    When Broker sends the configuration to Engine and all checks are executed
    ...    Then the correct check results are found in logs with expected args and service macros
    [Tags]    engine    MON-165488

    #10 hosts 20 services
    Ctn Config Centralized Engine    ${1}    ${10}    ${20}    ${True}
    #when this flag is on, engine env is replaced by engine macros
    Ctn Engine Config Set Value    0    enable_environment_macros    1    True
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    module0    neb    error

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker must receive a diff state ack from poller 1.

    #let all checks working (check interval = one minute)
    Sleep    70s

    Ctn Stop Engine
    Ctn Kindly Stop Broker

    # we have 200 services and checks of all these services must be found in logs
    # this is the purpose of the following function
    ${nb_check_ok}    Ctn Engine Check Sh Command Output

    Should Be Equal    ${nb_check_ok}    ${200}    we should have 200 services checked

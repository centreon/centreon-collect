*** Settings ***
Documentation       Centreon Engine many service checks tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed

*** Test Cases ***

ENGINE_MANY_CHECKS
    [Documentation]    Given a engine with many services and a unique check on each service with it's own env variables
    ...                We expect correct check result in logs and we checks returned args and service macros
    [Tags]    engine    MON-165488

    #10 hosts 20 services
    Ctn Config Engine    ${1}    ${10}    ${20}    ${True}
    #when this flag is on, engine env is replaced by engine macros
    Ctn Engine Config Set Value    0    enable_environment_macros    1    True
    Ctn Config Broker    module
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    module0    neb    error


    Ctn Start Engine

    #let all checks working (check interval = one minute)
    Sleep    70s 

    Ctn Stop Engine
    
    ${nb_check_ok}    Ctn Engine Check Sh Command Output

    Should Be Equal    ${nb_check_ok}    ${200}    we should have 200 services checked

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
    Ctn Config Engine    ${1}    ${10}    ${20}    check.sh
    #when this flag is on, engine env is replaced by engine macros
    Ctn Engine Config Set Value    0    enable_environment_macros    1    True
    Ctn Engine Config Set Value    0    log_level_commands    trace
    Ctn Config Broker    module
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    module0    neb    error


    Ctn Start Engine

    #let all checks working (check interval = one minute)
    Sleep    70s 

    Ctn Stop Engine
    
    # we have 200 services and checks of all these services must be found in logs
    # this is the purpose of the following function	
    ${nb_check_ok}    Ctn Engine Check Sh Command Output

    Should Be Equal    ${nb_check_ok}    ${200}    we should have 200 services checked

    ${nb_raw_create}    Run Process    grep    -c     create raw_v2     ${engineLog0}
    ${nb_raw_delete}    Run Process    grep    -c    delete raw_v2     ${engineLog0}
    ${nb_checker_delete}    Run Process    grep    -c    delete checker    ${engineLog0}
    Log To Console    nb raw_v2 created: ${nb_raw_create.stdout} nb raw_v2 deleted: ${nb_raw_delete.stdout}

    Should Be True     ${nb_raw_delete.stdout} == ${nb_raw_create.stdout}     some raw_v2 object have not been deleted
    Should Be True     ${nb_checker_delete.rc} == 0      checker has not been deleted

ENGINE_MANY_CHECK_OK
    [Documentation]    Given a engine with many services and a command shared between several services
    ...                We expect correct check result in logs and we checks returned args and service macros
    ...                All odd indexed services use perl connector to do checks
    [Tags]    engine    MON-177740

    #10 hosts of 50 services
    Ctn Config Engine    ${1}    ${10}    ${100}
    Ctn Config Broker    module
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    module0    neb    error
    Ctn Broker Config Log    module0    processing    error
    Ctn Engine Config Set Value    0    log_level_commands    trace
    Ctn Clear Retention

    FOR    ${i}    IN RANGE    ${1}    ${1001}
        ${serv_desc}    Catenate    SEPARATOR=    service_    ${i} 
        Ctn Engine Config Replace Value In Services    0    ${serv_desc}   check_interval    1
    END

    #create /tmp/states
    FOR    ${i}    IN RANGE    ${1}    ${51}
        Ctn Set Command Status    ${i}    0
    END

    Ctn Start Engine

    #let all checks working (check interval = one minute)
    Sleep    70s 

    Ctn Stop Engine
    
    # we have 1000 services and checks of all these services must be found in logs
    # this is the purpose of the following function	
    ${nb_check_ok}    Ctn Engine Check Command Output

    Should Be Equal    ${nb_check_ok}    ${1000}    we should have 1000 services checked


ENGINE_MANY_LONG_CHECKS
    [Documentation]    Given a engine with many services and a unique check on each service with it's own env variables and a variable check duration
    ...                We expect no memory leak in centengine despite checks timeouts
    [Tags]    engine    MON-191712

    #10 hosts 20 services
    Ctn Config Engine    ${1}    ${10}    ${20}    check_long.sh
    #when this flag is on, engine env is replaced by engine macros
    Ctn Engine Config Set Value    0    enable_environment_macros    1    True
    Ctn Engine Config Set Value    0    log_level_commands    trace
    Ctn Config Broker    module
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    module0    neb    error
    Ctn Clear Engine Logs


    Ctn Start Engine

    #let all checks working (check interval = one minute)
    Sleep    70s 

    Ctn Stop Engine
    
    # we have 200 services and checks of all these services must be found in logs
    # this is the purpose of the following function	
    ${nb_check_ok}    Ctn Engine Check Sh Command Output    15

    Should Be True    ${nb_check_ok} > 10    we should have 200 services checked

    ${nb_raw_create}    Run Process    grep    -c     create raw_v2     ${engineLog0}
    ${nb_raw_delete}    Run Process    grep    -c    delete raw_v2     ${engineLog0}
    ${nb_checker_delete}    Run Process    grep    -c    delete checker    ${engineLog0}
    Log To Console    nb raw_v2 created: ${nb_raw_create.stdout} nb raw_v2 deleted: ${nb_raw_delete.stdout}

    Should Be True     ${nb_raw_delete.stdout} == ${nb_raw_create.stdout}     some raw_v2 object have not been deleted
    Should Be True     ${nb_checker_delete.rc} == 0      checker has not been deleted


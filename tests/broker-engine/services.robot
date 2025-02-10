*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs 


*** Test Cases ***
SDER
    [Documentation]    The check attempts and the max check attempts of (host_1,service_1)
    ...    are changed to 280 thanks to the retention.dat file. Then Engine and Broker are started
    ...    and Broker should write these values in the services and resources tables.
    ...    We only test the services table because we need a resources table that allows bigger numbers
    ...    for these two attributes. But we see that Broker doesn't crash anymore.
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${1}    ${25}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    central    processing    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    module0    neb    trace
    Ctn Broker Config Log    module0    processing    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Engine Config Replace Value In Services    0    service_1    max_check_attempts    42

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Stop Engine

    Ctn Modify Retention Dat    0    host_1    service_1    current_attempt    280
    # modified attributes is a bit field. We must set the bit corresponding to MAX_ATTEMPTS to be allowed to change max_attempts. Otherwise it will be set to 3.
    Ctn Modify Retention Dat    0    host_1    service_1    modified_attributes    65535
    Ctn Modify Retention Dat    0    host_1    service_1    max_attempts    280

    Ctn Modify Retention Dat    0    host_1    service_1    current_state    2
    Ctn Modify Retention Dat    0    host_1    service_1    state_type    1
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}


    FOR    ${index}    IN RANGE    20
        Log To Console    SELECT check_attempt from services WHERE description='service_1'
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT check_attempt from services WHERE description='service_1'
        Log To Console    ${output}
        IF    "${output}" == "((280,),)"    BREAK
        Sleep    1s
    END

    Should Be Equal As Strings    ${output}    ((280,),)

*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
EBSNU1
    [Documentation]    New services with notes_url with more than 2000 characters
    [Tags]    broker    engine    services    protobuf
    Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Config Broker Sql Output    central    unified_sql
    ${nu}    Evaluate    2000*"X"
    Engine Config Set Value In Services    0    service_1    notes_url    ${nu}
    Clear Retention
    Ctn Start Broker
    Ctn Start Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT notes_url FROM services WHERE description='service_1'
        Sleep    1s
        IF    "${output}" == "(('${nu}',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('${nu}',),)
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT notes_url FROM resources WHERE name='service_1'
        Sleep    1s
        IF    "${output}" == "(('${nu}',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('${nu}',),)
    Ctn Stop Engine
    Ctn Kindly Stop Broker

EBSAU2
    [Documentation]    New services with action_url with more than 2000 characters
    [Tags]    broker    engine    services    protobuf
    Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Config Broker Sql Output    central    unified_sql
    ${au}    Evaluate    2000*"Y"
    Engine Config Set Value In Services    0    service_2    action_url    ${au}
    Clear Retention
    Ctn Start Broker
    Ctn Start Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT action_url FROM services WHERE description='service_2'
        Sleep    1s
        IF    "${output}" == "(('${au}',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('${au}',),)
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT action_url FROM resources WHERE name='service_2'
        Sleep    1s
        IF    "${output}" == "(('${au}',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('${au}',),)
    Ctn Stop Engine
    Ctn Kindly Stop Broker

EBSN3
    [Documentation]    New services with notes with more than 500 characters
    [Tags]    broker    engine    services    protobuf
    Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Config Broker Sql Output    central    unified_sql
    ${n}    Evaluate    500*"Z"
    Engine Config Set Value In Services    0    service_3    notes    ${n}
    Clear Retention
    Ctn Start Broker
    Ctn Start Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT notes FROM services WHERE description='service_3'
        Sleep    1s
        IF    "${output}" == "(('${n}',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('${n}',),)
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT notes FROM resources WHERE name='service_3'
        Sleep    1s
        IF    "${output}" == "(('${n}',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('${n}',),)
    Ctn Stop Engine
    Ctn Kindly Stop Broker

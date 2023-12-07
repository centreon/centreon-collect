*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Test Cases ***
EBSNU1
    [Documentation]    New services with notes_url with more than 2000 characters
    [Tags]    broker    engine    services    protobuf
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    sql    debug
    Broker Config Log    central    bbdo    debug
    Config Broker Sql Output    central    unified_sql
    ${nu}    Evaluate    2000*"X"
    Engine Config Set Value In Services    0    service_1    notes_url    ${nu}
    Clear Retention
    Start Broker
    Start Engine

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
    Stop Engine
    Kindly Stop Broker

EBSAU2
    [Documentation]    New services with action_url with more than 2000 characters
    [Tags]    broker    engine    services    protobuf
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    sql    debug
    Broker Config Log    central    bbdo    debug
    Config Broker Sql Output    central    unified_sql
    ${au}    Evaluate    2000*"Y"
    Engine Config Set Value In Services    0    service_2    action_url    ${au}
    Clear Retention
    Start Broker
    Start Engine

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
    Stop Engine
    Kindly Stop Broker

EBSN3
    [Documentation]    New services with notes with more than 500 characters
    [Tags]    broker    engine    services    protobuf
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    sql    debug
    Broker Config Log    central    bbdo    debug
    Config Broker Sql Output    central    unified_sql
    ${n}    Evaluate    500*"Z"
    Engine Config Set Value In Services    0    service_3    notes    ${n}
    Clear Retention
    Start Broker
    Start Engine

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
    Stop Engine
    Kindly Stop Broker

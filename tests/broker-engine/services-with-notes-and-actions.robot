*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
EBSNU1
    [Documentation]    New services with notes_url with more than 2000 characters
    [Tags]    broker    engine    services    protobuf
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Config Broker Sql Output    central    unified_sql
    ${nu}    Evaluate    2000*"X"
    Ctn Engine Config Set Value In Services    0    service_1    notes_url    ${nu}
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start engine

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
    Ctn Stop engine
    Ctn Kindly Stop Broker

EBSAU2
    [Documentation]    New services with action_url with more than 2000 characters
    [Tags]    broker    engine    services    protobuf
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Config Broker Sql Output    central    unified_sql
    ${au}    Evaluate    2000*"Y"
    Ctn Engine Config Set Value In Services    0    service_2    action_url    ${au}
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start engine

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
    Ctn Stop engine
    Ctn Kindly Stop Broker

EBSN3
    [Documentation]    New services with notes with more than 500 characters
    [Tags]    broker    engine    services    protobuf
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Config Broker Sql Output    central    unified_sql
    ${n}    Evaluate    500*"Z"
    Ctn Engine Config Set Value In Services    0    service_3    notes    ${n}
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start engine

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
    Ctn Stop engine
    Ctn Kindly Stop Broker

EBSIC0
    [Documentation]    Verify that the update icon_id for host/service in cfg is well propagated to the database
    [Tags]    broker    engine    service    MON-157503
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
    Ctn Clear Retention
    Ctn Clear Db    resources

    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_1
    ...    icon_id
    ...    1

    Ctn Engine Config Set Value In Services
    ...    0
    ...    service_1
    ...    icon_id
    ...    1

    ${start}    Get Current Date

    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}


    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT id,icon_id FROM resources WHERE name='service_1'
        Sleep    1s
        IF    "${output}" == "((1, 1),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((1, 1),)    the service_1 should have icon_id=1

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT id,icon_id FROM resources WHERE name='host_1'
        Sleep    1s
        IF    "${output}" == "((1, 1),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((1, 1),)    the host_1 should have icon_id=1

    Ctn Engine Config Replace Value In Hosts    0    host_1    icon_id    2
    Ctn Engine Config Replace Value In Services    0    service_1    icon_id    2
    
    Ctn Reload Engine

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT id,icon_id FROM resources WHERE name='service_1'
        Sleep    1s
        IF    "${output}" == "((1, 2),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((1, 2),)    the service_1 should have icon_id=2

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT id,icon_id FROM resources WHERE name='host_1'
        Sleep    1s
        IF    "${output}" == "((1, 2),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((1, 2),)    the host_1 should have icon_id=2

    Ctn Stop Engine
    Ctn Kindly Stop Broker
*** Settings ***
Documentation       Centreon Broker and Engine Creation of hosts with long action_url, notes and notes_url.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
EBSNU1
    [Documentation]    New hosts with notes_url with more than 2000 characters
    [Tags]    broker    engine    hosts    protobuf
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    ${nu}    Evaluate    2000*"X"
    Ctn Engine Config Set Value In Hosts    0    host_1    notes_url    ${nu}
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT notes_url FROM hosts WHERE name='host_1'
        Sleep    1s
        IF    "${output}" == "(('${nu}',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('${nu}',),)
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT notes_url FROM resources WHERE name='host_1'
        Sleep    1s
        IF    "${output}" == "(('${nu}',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('${nu}',),)
    Ctn Stop Engine
    Ctn Kindly Stop Broker

EBSAU2
    [Documentation]    New hosts with action_url with more than 2000 characters
    [Tags]    broker    engine    hosts    protobuf
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    ${au}    Evaluate    2000*"Y"
    Ctn Engine Config Set Value In Hosts    0    host_2    action_url    ${au}
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT action_url FROM hosts WHERE name='host_2'
        Sleep    1s
        IF    "${output}" == "(('${au}',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('${au}',),)
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT action_url FROM resources WHERE name='host_2'
        Sleep    1s
        IF    "${output}" == "(('${au}',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('${au}',),)
    Ctn Stop Engine
    Ctn Kindly Stop Broker

EBSN3
    [Documentation]    New hosts with notes with more than 500 characters
    [Tags]    broker    engine    hosts    protobuf
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    ${n}    Evaluate    500*"Z"
    Ctn Engine Config Set Value In Hosts    0    host_3    notes    ${n}
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT notes FROM hosts WHERE name='host_3'
        Sleep    1s
        IF    "${output}" == "(('${n}',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('${n}',),)
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT notes FROM resources WHERE name='host_3'
        Sleep    1s
        IF    "${output}" == "(('${n}',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('${n}',),)
    Ctn Stop Engine
    Ctn Kindly Stop Broker

EBSN4
    [Documentation]    New hosts with No Alias / Alias and have A Template
    [Tags]    broker    engine    hosts    MON-16261
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Create Tags File    ${0}    ${40}
    Ctn Create Template File    ${0}    host    group_tags    [2, 6]

    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${0}    hostTemplates.cfg

    Ctn Add Template To Hosts    0    host_template_1    [1,3]
    Ctn Add Template To Hosts    0    host_template_2    [2]
    Ctn Engine Config Set Value In Hosts    0    host_template_1    alias    alias_Template_1    hostTemplates.cfg

    Ctn Engine Config Delete Value In Hosts    0    host_1    alias
    Ctn Engine Config Delete Value In Hosts    0    host_2    alias

    Ctn Engine Config Replace Value In Hosts    0    host_3    alias    alias_host_3
    Ctn Engine Config Replace Value In Hosts    0    host_4    alias    alias_host_4

    Sleep    1s

    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    # use case Host1 doesn't have alias and use template 1 who has alias => Host_1 alias take the host name(Host_1)
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT alias FROM resources WHERE name = 'host_1';
        Sleep    1s
        IF    "${output}" == "(('host_1',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('host_1',),)

    # use case Host2 doesn't have alias and use template 2 who doesn't alias => Host_2 alias take the host name(Host_2)
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT alias FROM resources WHERE name = 'host_2';
        Sleep    1s
        IF    "${output}" == "(('host_2',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('host_2',),)

    # use case Host3 have alias and use template 1 who has alias => Host_3 alias take the alias(Host_3)
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT alias FROM resources WHERE name = 'host_3';
        Sleep    1s
        IF    "${output}" == "(('alias_host_3',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('alias_host_3',),)

    # use case Host4 have alias and use template 2 who doesn't alias => alias Host_4 take the alias(Host_4)
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT alias FROM resources WHERE name = 'host_4';
        Sleep    1s
        IF    "${output}" == "(('alias_host_4',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('alias_host_4',),)

    Ctn Stop Engine
    Ctn Kindly Stop Broker


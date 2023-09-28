*** Settings ***
Documentation       Centreon Broker and Engine add Hostgroup

Resource            ../resources/resources.robot
Library             DatabaseLibrary
Library             Process
Library             OperatingSystem
Library             DateTime
Library             Collections
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Test Cases ***
EBNHG1
    [Documentation]    New host group with several pollers and connections to DB
    [Tags]    broker    engine    hostgroup
    Config Engine    ${3}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}

    Broker Config Log    central    sql    info
    Broker Config Output Set    central    central-broker-master-sql    connections_count    5
    Broker Config Output Set    central    central-broker-master-perfdata    connections_count    5
    ${start}    Get Current Date
    Start Broker
    Start Engine
    Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]

    Sleep    3s
    Reload Broker
    Reload Engine

    ${content}    Create List
    ...    enabling membership of host 3 to host group 1 on instance 1
    ...    enabling membership of host 2 to host group 1 on instance 1
    ...    enabling membership of host 1 to host group 1 on instance 1

    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new host groups not found in logs.
    Stop Engine
    Kindly Stop Broker

EBNHGU1
    [Documentation]    New host group with several pollers and connections to DB with broker configured with unified_sql
    [Tags]    broker    engine    hostgroup    unified_sql
    Config Engine    ${3}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}

    Broker Config Log    central    sql    info
    Config Broker Sql Output    central    unified_sql
    Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    ${start}    Get Current Date
    Start Broker
    Start Engine
    Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]

    Sleep    3s
    Reload Broker
    Reload Engine

    ${content}    Create List
    ...    enabling membership of host 3 to host group 1 on instance 1
    ...    enabling membership of host 2 to host group 1 on instance 1
    ...    enabling membership of host 1 to host group 1 on instance 1

    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new host groups not found in logs.
    Stop Engine
    Kindly Stop Broker

EBNHGU2
    [Documentation]    New host group with several pollers and connections to DB with broker configured with unified_sql
    [Tags]    broker    engine    hostgroup    unified_sql
    Config Engine    ${3}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}

    Broker Config Log    central    sql    info
    Config Broker Sql Output    central    unified_sql
    Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    Config BBDO3    3
    ${start}    Get Current Date
    Start Broker
    Start Engine
    Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]

    Sleep    3s
    Reload Broker
    Reload Engine

    ${content}    Create List
    ...    enabling membership of host 3 to host group 1 on instance 1
    ...    enabling membership of host 2 to host group 1 on instance 1

    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new host groups not found in logs.
    Stop Engine
    Kindly Stop Broker

EBNHGU3
    [Documentation]    New host group with several pollers and connections to DB with broker configured with unified_sql
    [Tags]    broker    engine    hostgroup    unified_sql
    Config Engine    ${4}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${4}

    Broker Config Log    central    sql    info
    Config Broker Sql Output    central    unified_sql
    Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    Config BBDO3    4
    Broker Config Log    central    sql    debug

    ${start}    Get Current Date
    Start Broker
    Start Engine
    Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]
    Add Host Group    ${1}    ${1}    ["host_21", "host_22", "host_23"]
    Add Host Group    ${2}    ${1}    ["host_31", "host_32", "host_33"]
    Add Host Group    ${3}    ${1}    ["host_41", "host_42", "host_43"]

    Sleep    3s
    Reload Broker
    Reload Engine

    ${result}    Check Number Of Relations Between Hostgroup And Hosts    1    12    30
    Should Be True    ${result}    We should have 12 hosts members of host 1.

    Config Engine Remove Cfg File    ${0}    hostgroups.cfg

    Sleep    3s
    Reload Broker
    Reload Engine
    ${result}    Check Number Of Relations Between Hostgroup And Hosts    1    9    30
    Should Be True    ${result}    We should have 12 hosts members of host 1.

    Stop Engine
    Kindly Stop Broker

EBNHG4
    [Documentation]    New host group with several pollers and connections to DB with broker and rename this hostgroup
    [Tags]    broker    engine    hostgroup
    Config Engine    ${3}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}

    Broker Config Log    central    sql    info
    Broker Config Output Set    central    central-broker-master-sql    connections_count    5
    Broker Config Output Set    central    central-broker-master-perfdata    connections_count    5
    ${start}    Get Current Date
    Start Broker
    Start Engine
    Sleep    3s
    Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]

    Reload Broker
    Reload Engine

    ${content}    Create List
    ...    enabling membership of host 3 to host group 1 on instance 1
    ...    enabling membership of host 2 to host group 1

    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new host groups not found in logs.

    Rename Host Group    ${0}    ${1}    test    ["host_1", "host_2", "host_3"]

    Sleep    10s
    ${start}    Get Current Date
    Log To Console    Step-1
    Reload Broker
    Log To Console    Step0
    Reload Engine

    Log To Console    Step1
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Log To Console    Step1
    FOR    ${index}    IN RANGE    60
        Log To Console    SELECT name FROM hostgroups WHERE hostgroup_id = ${1}
        ${output}    Query    SELECT name FROM hostgroups WHERE hostgroup_id = ${1}
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "(('hostgroup_test',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('hostgroup_test',),)

    Stop Engine
    Kindly Stop Broker

EBNHGU4
    [Documentation]    New host group with several pollers and connections to DB with broker and rename this hostgroup
    [Tags]    broker    engine    hostgroup
    Config Engine    ${3}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}

    Broker Config Log    central    sql    info
    Config Broker Sql Output    central    unified_sql
    Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    ${start}    Get Current Date
    Start Broker
    Start Engine
    Sleep    3s
    Add Host Group    ${0}    ${1}    ["host_1", "host_2", "host_3"]

    Reload Broker
    Reload Engine

    ${content}    Create List
    ...    enabling membership of host 3 to host group 1 on instance 1
    ...    enabling membership of host 2 to host group 1

    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    45
    Should Be True    ${result}    One of the new host groups not found in logs.

    Rename Host Group    ${0}    ${1}    test    ["host_1", "host_2", "host_3"]

    Sleep    10s
    ${start}    Get Current Date
    Log To Console    Step-1
    Reload Broker
    Log To Console    Step0
    Reload Engine

    Log To Console    Step1
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Log To Console    Step1
    FOR    ${index}    IN RANGE    60
        Log To Console    SELECT name FROM hostgroups WHERE hostgroup_id = ${1}
        ${output}    Query    SELECT name FROM hostgroups WHERE hostgroup_id = ${1}
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "(('hostgroup_test',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('hostgroup_test',),)

    Stop Engine
    Kindly Stop Broker

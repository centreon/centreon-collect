*** Settings ***
Documentation       Centreon Broker centralized configuration MariaDB access tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CBEDB1
    [Documentation]    Given the broker and engine are started in new generation mode
    ...    When MariaDB is started after them
    ...    Then the connection to the database should be established
    [Tags]    broker    sql    start-stop
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info
    Ctn Config Centralized Engine    ${1}
    ${start}    Ctn Get Round Current Date
    Ctn Stop Mysql
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    Ctn Wait For Engine Configuration To Be Applied    ${start}    0
    ${content}    Create List    error while starting connection
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Should Be True    ${result}    Message about the disconnection between cbd and the database is missing
    Ctn Start Mysql
    ${result}    Ctn Check Broker Stats Exist    central    mysql manager    waiting tasks in connection 0    60
    Should Be True    ${result}    Message about the connection to the database is missing.
    Ctn Kindly Stop Broker
    Ctn Stop Engine

CBEDB2
    [Documentation]    Feature: SQL Connections via gRPC API
    ...    Scenario: Start broker and engine, stop MariaDB, then start it again
    ...    Given the broker and engine are running
    ...    When MariaDB is stopped and then started again
    ...    Then the gRPC API should provide information about SQL connections
    [Tags]    broker    sql    start-stop    grpc
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info
    Ctn Config Centralized Engine    ${1}
    ${start}    Ctn Get Round Current Date
    Ctn Start Mysql
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    Ctn Wait For Engine Configuration To Be Applied    ${start}    0

    FOR    ${t}    IN RANGE    60
        ${result}    Ctn Check Sql Connections Count With Grpc    51001    ${3}
        IF    ${result}    BREAK
    END
    Should Be True    ${result}    gRPC does not return 3 connections as expected
    Ctn Stop Mysql
    FOR    ${t}    IN RANGE    60
        ${result}    Ctn Check All Sql Connections Down With Grpc    51001
        IF    ${result}    BREAK
    END
    Should Be True    ${result}    Connections are not all down.

    Ctn Start Mysql
    FOR    ${t}    IN RANGE    60
        ${result}    Ctn Check Sql Connections Count With Grpc    51001    ${3}
        IF    ${result}    BREAK
    END
    Should Be True    ${result}    gRPC does not return 3 connections as expected
    Ctn Kindly Stop Broker
    Ctn Stop Engine

CBEDB3
    [Documentation]    Feature: SQL Connections via gRPC API
    ...    Scenario: Start broker and engine, then stop MariaDB and then start it again
    ...    Given broker and engine are running
    ...    When MariaDB is stopped and then started again
    ...    Then the gRPC API should provide information about SQL connections
    [Tags]    broker    sql    start-stop    grpc
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info
    Ctn Config Centralized Engine    ${1}
    ${start}    Ctn Get Round Current Date
    Ctn Stop Mysql
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    Ctn Wait For Engine Configuration To Be Applied    ${start}    0

    FOR    ${t}    IN RANGE    60
        ${result}    Ctn Check All Sql Connections Down With Grpc    51001
        IF    ${result}    BREAK
    END
    Should Be True    ${result}    Connections are not all down.

    Ctn Start Mysql
    FOR    ${t}    IN RANGE    60
        ${result}    Ctn Check Sql Connections Count With Grpc    51001    ${3}
        IF    ${result}    BREAK
    END
    Should Be True    ${result}    gRPC does not return 3 connections as expected
    Ctn Kindly Stop Broker
    Ctn Stop Engine

CBDBM1
    [Documentation]    Scenario: Broker reconnects to MariaDB after startup with configurable connection count
    ...    Given the broker and engine are started in new generation mode before MariaDB
    ...    When MariaDB is started after them with connections_count set to 1 then 3
    ...    Then the broker reconnects with the configured number of connections each time
    [Tags]    broker    sql    start-stop
    @{lst}    Create List    1    3
    FOR    ${c}    IN    @{lst}
        Ctn Config Broker    central
        Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    ${c}
        Ctn Config Broker    rrd
        Ctn Config Broker    module
	Ctn Broker Config Log    central    bbdo    info
        Ctn Config Centralized Engine    ${1}
        Ctn Broker Config Log    central    sql    debug
        ${start}    Ctn Get Round Current Date
        Ctn Stop Mysql
        Ctn Start Broker    newGeneration=True
        Ctn Start Engine    newGeneration=True

	Ctn Wait For Engine Configuration To Be Applied    ${start}    0

        ${content}    Create List    error while starting connection
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
        Should Be True    ${result}    Message about the disconnection between cbd and the database is missing
        Ctn Start Mysql
        ${expected}    Evaluate    ${c} + 1
        ${result}    Ctn Get Broker Stats Size    central    mysql manager    ${expected}    ${60}
        Should Be True
        ...    ${result} >= ${expected}
        ...    The stats file should contain at least ${c} + 1 connections to the database and not ${result}.
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

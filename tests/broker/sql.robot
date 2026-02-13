*** Settings ***
Documentation       Centreon Broker Mariadb access

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BDB1
    [Documentation]    Given a broker with a wrong unified_sql db_host
    ...    When cbd starts
    ...    Then it should log an error about the connection
    ...    And it should not crash
    [Tags]    broker    sql
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_host    1.2.3.4
    FOR    ${i}    IN RANGE    2
        ${start}    Get Current Date
        Ctn Start Broker
        ${content}    Create List    error while starting connection
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    50
        Should Be True    ${result}    No message about the disconnection between cbd and the database
        Ctn Kindly Stop Broker
    END

BDB2
    [Documentation]    Given a broker with a wrong unified_sql db_password
    ...    When cbd starts
    ...    Then it should log an error about access denied
    ...    And it should not crash
    [Tags]    broker    sql
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_password    centreon1
    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List    mysql_connection: error while starting connection
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
    Should Be True    ${result}
    Ctn Kindly Stop Broker

BDB3
    [Documentation]    Given a broker with a correct unified_sql user password
    ...    When cbd starts
    ...    Then the connection to the database should be established
    [Tags]    broker    sql
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    central    sql    debug
    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List    connected to 'MariaDB' Server
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Should Be True    ${result}
    Ctn Kindly Stop Broker

BEDB1
    [Documentation]    Given the broker and engine are started,
    ...    When MariaDB is started after them,
    ...    Then the connection to the database should be established
    [Tags]    broker    sql    start-stop
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Engine    ${1}
    ${start}    Ctn Get Round Current Date
    Ctn Stop Mysql
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    error while starting connection
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Should Be True    ${result}    Message about the disconnection between cbd and the database is missing
    Ctn Start Mysql
    ${result}    Ctn Check Broker Stats Exist    central    mysql manager    waiting tasks in connection 0    60
    Should Be True    ${result}    Message about the connection to the database is missing.
    Ctn Kindly Stop Broker
    Ctn Stop Engine

BEDB2
    [Documentation]    Feature: SQL Connections via gRPC API
    ...    Scenario: Start broker and engine, stop MariaDB, then start it again
    ...    Given the broker and engine are running
    ...    When MariaDB is stopped and then started again
    ...    Then the gRPC API should provide information about SQL connections
    [Tags]    broker    sql    start-stop    grpc
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Engine    ${1}
    ${start}    Get Current Date
    Ctn Start Mysql
    Ctn Start Broker
    Ctn Start Engine
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

BEDB3
    [Documentation]    Feature: SQL Connections via gRPC API
    ...    Scenario: Start broker and engine, then stop MariaDB and then start it again
    ...    Given broker and engine are running
    ...    When MariaDB is stopped and then started again
    ...    Then the gRPC API should provide information about SQL connections
    [Tags]    broker    sql    start-stop    grpc
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Engine    ${1}
    ${start}    Get Current Date
    Ctn Stop Mysql
    Ctn Start Broker
    Ctn Start Engine
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

BDBM1
    [Documentation]    Feature: Broker and Engine Start/Stop with MariaDB
    ...     Scenario: Start broker and engine, then start MariaDB with different connection counts
    ...     Given the broker and engine are started
    ...     When MariaDB is started after them
    ...     And the broker is configured with connections_count set to 1 and 3
    ...     Then the connection to the database should be established for each configured connection
    [Tags]    broker    sql    start-stop
    @{lst}    Create List    1    3
    FOR    ${c}    IN    @{lst}
        Ctn Config Broker    central
        Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    ${c}
        Ctn Config Broker    rrd
        Ctn Config Broker    module
        Ctn Config Engine    ${1}
	Ctn Broker Config Log    central    sql    debug
        ${start}    Ctn Get Round Current Date
        Ctn Stop Mysql
        Ctn Start Broker
        Ctn Start Engine
        ${content}    Create List    error while starting connection
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
        Should Be True    ${result}    Message about the disconnection between cbd and the database is missing
        Ctn Start Mysql
        ${result}    Ctn Get Broker Stats Size    central    mysql manager
        Should Be True
        ...    ${result} >= ${c} + 1
        ...    The stats file should contain at least ${c} + 1 connections to the database and not ${result}.
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

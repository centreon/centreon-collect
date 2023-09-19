*** Settings ***
Documentation       Centreon Broker Mariadb access

Resource            ../resources/resources.robot
Library             Process
Library             OperatingSystem
Library             DateTime
Library             ../resources/Broker.py
Library             ../resources/Engine.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes


*** Test Cases ***
BDB1
    [Documentation]    Access denied when database name exists but is not the good one for sql output
    [Tags]    broker    sql
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Broker Config Output set    central    central-broker-master-sql    db_name    centreon
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Start Broker
        ${content}    Create List    storage and sql streams do not have the same database configuration
        ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
        Should Be True
        ...    ${result}
        ...    A message should tell that sql and storage outputs do not have the same configuration.
        Kindly Stop Broker
    END

BDB2
    [Documentation]    Access denied when database name exists but is not the good one for storage output
    [Tags]    broker    sql
    Config Broker    central
    Broker Config Log    central    sql    info
    Config Broker    rrd
    Config Broker    module
    Broker Config Output set    central    central-broker-master-perfdata    db_name    centreon
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Start Broker
        ${content}    Create List    storage and sql streams do not have the same database configuration
        ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
        Should Be True
        ...    ${result}
        ...    A log telling the impossibility to establish a connection between the storage stream and the database should appear.
        Kindly Stop Broker
    END

BDB3
    [Documentation]    Access denied when database name does not exist for sql output
    [Tags]    broker    sql
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Broker Config Output set    central    central-broker-master-sql    db_name    centreon1
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Start Broker
        ${content}    Create List    global error: mysql_connection: error while starting connection
        ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
        Should Be True    ${result}    No message about the database not connected.
        Kindly Stop Broker
    END

BDB4
    [Documentation]    Access denied when database name does not exist for storage and sql outputs
    [Tags]    broker    sql
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Broker Config Output set    central    central-broker-master-perfdata    db_name    centreon1
    Broker Config Output set    central    central-broker-master-sql    db_name    centreon1
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Start Broker
        ${content}    Create List    error while starting connection
        ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
        Should Be True
        ...    ${result}
        ...    No message about the fact that cbd is not correctly connected to the database.
        Kindly Stop Broker
    END

BDB5
    [Documentation]    cbd does not crash if the storage/sql db_host is wrong
    [Tags]    broker    sql
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Broker Config Output set    central    central-broker-master-perfdata    db_host    1.2.3.4
    Broker Config Output set    central    central-broker-master-sql    db_host    1.2.3.4
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Start Broker
        ${content}    Create List    error while starting connection
        ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    50
        Should Be True    ${result}    No message about the disconnection between cbd and the database
        Kindly Stop Broker
    END

BDB6
    [Documentation]    cbd does not crash if the sql db_host is wrong
    [Tags]    broker    sql
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Broker Config Output set    central    central-broker-master-sql    db_host    1.2.3.4
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Start Broker
        ${content}    Create List    error while starting connection
        ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
        Should Be True    ${result}    No message about the disconnection between cbd and the database
        Kindly Stop Broker
    END

BDB7
    [Documentation]    access denied when database user password is wrong for perfdata/sql
    [Tags]    broker    sql
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Broker Config Output set    central    central-broker-master-sql    db_password    centreon1
    Broker Config Output set    central    central-broker-master-perfdata    db_password    centreon1
    ${start}    Get Current Date
    Start Broker
    ${content}    Create List    mysql_connection: error while starting connection
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
    Should Be True    ${result}
    Kindly Stop Broker

BDB8
    [Documentation]    access denied when database user password is wrong for perfdata/sql
    [Tags]    broker    sql
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Broker Config Output set    central    central-broker-master-perfdata    db_password    centreon1
    Broker Config Output set    central    central-broker-master-sql    db_password    centreon1
    ${start}    Get Current Date
    Start Broker
    ${content}    Create List    mysql_connection: error while starting connection
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
    Should Be True    ${result}
    Kindly Stop Broker

BDB9
    [Documentation]    access denied when database user password is wrong for sql
    [Tags]    broker    sql
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Broker Config Output set    central    central-broker-master-sql    db_password    centreon1
    ${start}    Get Current Date
    Start Broker
    ${content}    Create List    mysql_connection: error while starting connection
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
    Should Be True    ${result}
    Kindly Stop Broker

BDB10
    [Documentation]    connection should be established when user password is good for sql/perfdata
    [Tags]    broker    sql
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Broker Config Log    central    sql    debug
    ${start}    Get Current Date
    Start Broker
    ${content}    Create List    sql stream initialization    storage stream initialization
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Should Be True    ${result}
    Kindly Stop Broker

BEDB2
    [Documentation]    start broker/engine and then start MariaDB => connection is established
    [Tags]    broker    sql    start-stop
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Config Engine    ${1}
    ${start}    Get Current Date
    Stop Mysql
    Start Broker
    Start Engine
    ${content}    Create List    error while starting connection
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Should Be True    ${result}    Message about the disconnection between cbd and the database is missing
    Start Mysql
    ${result}    Check Broker Stats Exist    central    mysql manager    waiting tasks in connection 0    60
    Should Be True    ${result}    Message about the connection to the database is missing.
    Kindly Stop Broker
    Stop Engine

BEDB3
    [Documentation]    start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
    [Tags]    broker    sql    start-stop    grpc
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Config Engine    ${1}
    ${start}    Get Current Date
    Start Mysql
    Start Broker
    Start Engine
    FOR    ${t}    IN RANGE    60
        ${result}    Check Sql Connections Count With Grpc    51001    ${3}
        IF    ${result}    BREAK
    END
    Should Be True    ${result}    gRPC does not return 3 connections as expected
    Stop Mysql
    FOR    ${t}    IN RANGE    60
        ${result}    Check All Sql connections Down With Grpc    51001
        IF    ${result}    BREAK
    END
    Should Be True    ${result}    Connections are not all down.

    Start Mysql
    FOR    ${t}    IN RANGE    60
        ${result}    Check Sql Connections Count With Grpc    51001    ${3}
        IF    ${result}    BREAK
    END
    Should Be True    ${result}    gRPC does not return 3 connections as expected
    Kindly Stop Broker
    Stop Engine

BEDB4
    [Documentation]    start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
    [Tags]    broker    sql    start-stop    grpc
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Config Engine    ${1}
    ${start}    Get Current Date
    Stop Mysql
    Start Broker
    Start Engine
    FOR    ${t}    IN RANGE    60
        ${result}    Check All Sql connections Down With Grpc    51001
        IF    ${result}    BREAK
    END
    Should Be True    ${result}    Connections are not all down.

    Start Mysql
    FOR    ${t}    IN RANGE    60
        ${result}    Check Sql Connections Count With Grpc    51001    ${3}
        IF    ${result}    BREAK
    END
    Should Be True    ${result}    gRPC does not return 3 connections as expected
    Kindly Stop Broker
    Stop Engine

BDBM1
    [Documentation]    start broker/engine and then start MariaDB => connection is established
    [Tags]    broker    sql    start-stop
    @{lst}    Create List    1    6
    FOR    ${c}    IN    @{lst}
        Config Broker    central
        Broker Config Output set    central    central-broker-master-sql    connections_count    ${c}
        Broker Config Output set    central    central-broker-master-perfdata    connections_count    ${c}
        Config Broker    rrd
        Config Broker    module
        Config Engine    ${1}
        ${start}    Get Round Current Date
        Stop Mysql
        Start Broker
        Start Engine
        ${content}    Create List    error while starting connection
        ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
        Should Be True    ${result}    Message about the disconnection between cbd and the database is missing
        Start Mysql
        ${result}    Get Broker Stats Size    central    mysql manager
        Should Be True
        ...    ${result} >= ${c} + 1
        ...    The stats file should contain at less ${c} + 1 connections to the database.
        Kindly Stop Broker
        Stop Engine
    END

BDBU1
    [Documentation]    Access denied when database name exists but is not the good one for unified sql output
    [Tags]    broker    sql    unified_sql
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Config Broker    rrd
    Config Broker    module
    # We replace the usual centreon_storage database by centreon to make the wanted error
    Broker Config Output set    central    central-broker-unified-sql    db_name    centreon
    Broker Config Log    central    sql    trace
    Broker Config Flush Log    central    0
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Start Broker
        ${content}    Create List    Table 'centreon\..*' doesn't exist
        ${result}    Find Regex In Log with timeout    ${centralLog}    ${start}    ${content}    60
        Should Be True    ${result}    A message about some missing tables in 'centreon' database should appear
        Kindly Stop Broker
    END

BDBU3
    [Documentation]    Access denied when database name does not exist for unified sql output
    [Tags]    broker    sql    unified_sql
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Config Broker    rrd
    Config Broker    module
    Broker Config Output set    central    central-broker-unified-sql    db_name    centreon1
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Start Broker
        ${content}    Create List    global error: mysql_connection: error while starting connection
        ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
        Should Be True    ${result}
        Kindly Stop Broker
    END

BDBU5
    [Documentation]    cbd does not crash if the unified sql db_host is wrong
    [Tags]    broker    sql    unified_sql
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Config Broker    rrd
    Config Broker    module
    Broker Config Output set    central    central-broker-unified-sql    db_host    1.2.3.4
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Start Broker
        ${content}    Create List    error while starting connection
        ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    50
        Should Be True    ${result}    Cannot find the message telling cbd is not connected to the database.
        Kindly Stop Broker
    END

BDBU7
    [Documentation]    Access denied when database user password is wrong for unified sql
    [Tags]    broker    sql    unified_sql
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Config Broker    rrd
    Config Broker    module
    Broker Config Output set    central    central-broker-unified-sql    db_password    centreon1
    ${start}    Get Current Date
    Start Broker
    ${content}    Create List    mysql_connection: error while starting connection
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
    Should Be True    ${result}    Error concerning cbd not connected to the database is missing.
    Kindly Stop Broker

BDBU10
    [Documentation]    Connection should be established when user password is good for unified sql
    [Tags]    broker    sql    unified_sql
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Config Broker    rrd
    Config Broker    module
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    sql    debug
    ${start}    Get Current Date
    Start Broker
    ${content}    Create List    mysql_connection 0x[0-9a-f]* : commit
    ${result}    Find Regex In Log with timeout    ${centralLog}    ${start}    ${content}    40
    Should Be True    ${result[0]}    Log concerning a commit (connection ok) is missing.
    Kindly Stop Broker

BDBMU1
    [Documentation]    start broker/engine with unified sql and then start MariaDB => connection is established
    [Tags]    broker    sql    start-stop    unified_sql
    @{lst}    Create List    1    6
    FOR    ${c}    IN    @{lst}
        Config Broker    central
        Config Broker Sql Output    central    unified_sql
        Broker Config Output set    central    central-broker-unified-sql    connections_count    ${c}
        Broker Config Output set    central    central-broker-unified-sql    retry_interval    5
        Config Broker    rrd
        Config Broker    module
        Config Engine    ${1}
        ${start}    Get Current Date
        Stop Mysql
        Start Broker
        Start Engine
        ${content}    Create List    mysql_connection: error while starting connection
        ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
        Should Be True    ${result}    Broker does not see any issue with the db while it is switched off
        Start Mysql
        ${result}    Check Broker Stats Exist    central    mysql manager    waiting tasks in connection 0    80
        Should Be True    ${result}    No stats on mysql manager found
        ${result}    Get Broker Stats Size    central    mysql manager    ${60}
        Should Be True    ${result} >= min(3, ${c} + 1)    Broker mysql manager stats do not show the ${c} connections
        Kindly Stop Broker
        Stop Engine
    END

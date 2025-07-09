*** Settings ***
Documentation       Centreon Broker Mariadb access

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BDB1
    [Documentation]    Access denied when database name exists but is not the good one for sql output
    [Tags]    broker    sql
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Output Set    central    central-broker-master-sql    db_name    centreon
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Ctn Start Broker
        ${content}    Create List    storage and sql streams do not have the same database configuration
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
        Should Be True
        ...    ${result}
        ...    A message should tell that sql and storage outputs do not have the same configuration.
        Ctn Kindly Stop Broker
    END

BDB2
    [Documentation]    Access denied when database name exists but is not the good one for storage output
    [Tags]    broker    sql
    Ctn Config Broker    central
    Ctn Broker Config Log    central    sql    info
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Output Set    central    central-broker-master-perfdata    db_name    centreon
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Ctn Start Broker
        ${content}    Create List    storage and sql streams do not have the same database configuration
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
        Should Be True
        ...    ${result}
        ...    A log telling the impossibility to establish a connection between the storage stream and the database should appear.
        Ctn Kindly Stop Broker
    END

BDB3
    [Documentation]    Access denied when database name does not exist for sql output
    [Tags]    broker    sql
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Output Set    central    central-broker-master-sql    db_name    centreon1
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Ctn Start Broker
        ${content}    Create List    global error: mysql_connection: error while starting connection
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
        Should Be True    ${result}    No message about the database not connected.
        Ctn Kindly Stop Broker
    END

BDB4
    [Documentation]    Access denied when database name does not exist for storage and sql outputs
    [Tags]    broker    sql
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Output Set    central    central-broker-master-perfdata    db_name    centreon1
    Ctn Broker Config Output Set    central    central-broker-master-sql    db_name    centreon1
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Ctn Start Broker
        ${content}    Create List    error while starting connection
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
        Should Be True
        ...    ${result}
        ...    No message about the fact that cbd is not correctly connected to the database.
        Ctn Kindly Stop Broker
    END

BDB5
    [Documentation]    cbd does not crash if the storage/sql db_host is wrong
    [Tags]    broker    sql
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Output Set    central    central-broker-master-perfdata    db_host    1.2.3.4
    Ctn Broker Config Output Set    central    central-broker-master-sql    db_host    1.2.3.4
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Ctn Start Broker
        ${content}    Create List    error while starting connection
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    50
        Should Be True    ${result}    No message about the disconnection between cbd and the database
        Ctn Kindly Stop Broker
    END

BDB6
    [Documentation]    cbd does not crash if the sql db_host is wrong
    [Tags]    broker    sql
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Output Set    central    central-broker-master-sql    db_host    1.2.3.4
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Ctn Start Broker
        ${content}    Create List    error while starting connection
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
        Should Be True    ${result}    No message about the disconnection between cbd and the database
        Ctn Kindly Stop Broker
    END

BDB7
    [Documentation]    access denied when database user password is wrong for perfdata/sql
    [Tags]    broker    sql
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Output Set    central    central-broker-master-sql    db_password    centreon1
    Ctn Broker Config Output Set    central    central-broker-master-perfdata    db_password    centreon1
    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List    mysql_connection: error while starting connection
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
    Should Be True    ${result}
    Ctn Kindly Stop Broker

BDB8
    [Documentation]    access denied when database user password is wrong for perfdata/sql
    [Tags]    broker    sql
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Output Set    central    central-broker-master-perfdata    db_password    centreon1
    Ctn Broker Config Output Set    central    central-broker-master-sql    db_password    centreon1
    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List    mysql_connection: error while starting connection
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
    Should Be True    ${result}
    Ctn Kindly Stop Broker

BDB9
    [Documentation]    access denied when database user password is wrong for sql
    [Tags]    broker    sql
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Output Set    central    central-broker-master-sql    db_password    centreon1
    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List    mysql_connection: error while starting connection
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
    Should Be True    ${result}
    Ctn Kindly Stop Broker

BDB10
    [Documentation]    connection should be established when user password is good for sql/perfdata
    [Tags]    broker    sql
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    core    debug
    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List    sql stream initialization    storage stream initialization
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Should Be True    ${result}
    Ctn Kindly Stop Broker

BEDB2
    [Documentation]    start broker/engine and then start MariaDB => connection is established
    [Tags]    broker    sql    start-stop
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Engine    ${1}
    ${start}    Get Current Date
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

BEDB3
    [Documentation]    start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
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

BEDB4
    [Documentation]    start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
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
    [Documentation]    start broker/engine and then start MariaDB => connection is established
    [Tags]    broker    sql    start-stop
    @{lst}    Create List    1    6
    FOR    ${c}    IN    @{lst}
        Ctn Config Broker    central
        Ctn Broker Config Output Set    central    central-broker-master-sql    connections_count    ${c}
        Ctn Broker Config Output Set    central    central-broker-master-perfdata    connections_count    ${c}
        Ctn Config Broker    rrd
        Ctn Config Broker    module
        Ctn Config Engine    ${1}
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
        ...    The stats file should contain at least ${c} + 1 connections to the database.
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

BDBU1
    [Documentation]    Access denied when database name exists but is not the good one for unified sql output
    [Tags]    broker    sql    unified_sql
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker    rrd
    # We replace the usual centreon_storage database by centreon to make the wanted error
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_name    centreon
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Flush Log    central    0
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Ctn Start Broker
        ${content}    Create List    Table 'centreon\..*' doesn't exist
        ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    60
        Should Be True    ${result[0]}    A message about some missing tables in 'centreon' database should appear
        Ctn Kindly Stop Broker
    END

BDBU3
    [Documentation]    Access denied when database name does not exist for unified sql output
    [Tags]    broker    sql    unified_sql
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_name    centreon1
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Ctn Start Broker
        ${content}    Create List    global error: mysql_connection: error while starting connection
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
        Should Be True    ${result}
        Ctn Kindly Stop Broker
    END

BDBU5
    [Documentation]    cbd does not crash if the unified sql db_host is wrong
    [Tags]    broker    sql    unified_sql
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_host    1.2.3.4
    FOR    ${i}    IN RANGE    0    5
        ${start}    Get Current Date
        Ctn Start Broker
        ${content}    Create List    error while starting connection
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    50
        Should Be True    ${result}    Cannot find the message telling cbd is not connected to the database.
        Ctn Kindly Stop Broker
    END

BDBU7
    [Documentation]    Access denied when database user password is wrong for unified sql
    [Tags]    broker    sql    unified_sql
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_password    centreon1
    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List    mysql_connection: error while starting connection
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
    Should Be True    ${result}    Error concerning cbd not connected to the database is missing.
    Ctn Kindly Stop Broker

BDBU10
    [Documentation]    Connection should be established when user password is good for unified sql
    [Tags]    broker    sql    unified_sql
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    sql    debug
    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List    mysql_connection 0x[0-9a-f]* : commit
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Should Be True    ${result[0]}    Log concerning a commit (connection ok) is missing.
    Ctn Kindly Stop Broker

BDBMU1
    [Documentation]    start broker/engine with unified sql and then start MariaDB => connection is established
    [Tags]    broker    sql    start-stop    unified_sql
    @{lst}    Create List    1    3
    FOR    ${c}    IN    @{lst}
        Ctn Config Broker    central
        Ctn Config Broker Sql Output    central    unified_sql
        Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    ${c}
        Ctn Broker Config Output Set    central    central-broker-unified-sql    retry_interval    5
        Ctn Config Broker    rrd
        Ctn Config Broker    module
        Ctn Config Engine    ${1}
        ${start}    Get Current Date
        Ctn Stop Mysql
        Ctn Start Broker
        Ctn Start Engine
        ${content}    Create List    mysql_connection: error while starting connection
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    20
        Should Be True    ${result}    Broker does not see any issue with the db while it is switched off
        Ctn Start Mysql
        ${result}    Ctn Check Broker Stats Exist    central    mysql manager    waiting tasks in connection 0    80
        Should Be True    ${result}    No stats on mysql manager found
        ${result}    Ctn Get Broker Stats Size    central    mysql manager    ${60}
        Should Be True    ${result} >= ${c} + 1    Broker mysql manager stats do not show the ${c} connections
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

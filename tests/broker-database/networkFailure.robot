*** Settings ***
Documentation       Centreon Broker database connection failure

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
NetworkDbFail1
    [Documentation]    network failure test between broker and database (shutting down connection for 100ms)
    [Tags]    broker    database    network
    Ctn Network Failure    interval=100ms

NetworkDbFail2
    [Documentation]    network failure test between broker and database (shutting down connection for 1s)
    [Tags]    broker    database    network
    Ctn Network Failure    interval=1s

NetworkDbFail3
    [Documentation]    network failure test between broker and database (shutting down connection for 10s)
    [Tags]    broker    database    network
    Ctn Network Failure    interval=10s

NetworkDbFail4
    [Documentation]    network failure test between broker and database (shutting down connection for 30s)
    [Tags]    broker    database    network
    Ctn Network Failure    interval=30s

NetworkDbFail5
    [Documentation]    network failure test between broker and database (shutting down connection for 60s)
    [Tags]    broker    database    network
    Ctn Network Failure    interval=1m

NetworkDBFail6
    [Documentation]    network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s
    [Tags]    broker    database    network    unstable
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Broker Config Output Set    central    central-broker-master-sql    db_host    127.0.0.1
    Ctn Broker Config Output Set    central    central-broker-master-sql    connections_count    5
    Ctn Broker Config Output Set    central    central-broker-master-perfdata    db_host    127.0.0.1
    Ctn Broker Config Output Set    central    central-broker-master-perfdata    connections_count    5
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Broker and Engine are not connected
    ${content}    Create List    run query: SELECT
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Should Be True    ${result}    No SELECT done by broker in the DB
    Ctn Disable Eth Connection On Port    port=3306
    Sleep    1m
    Ctn Reset Eth Connection
    ${content}    Create List    0 events acknowledged
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Ctn Stop Engine
    Ctn Kindly Stop Broker

NetworkDBFailU6
    [Documentation]    network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s (with unified_sql)
    [Tags]    broker    database    network    unified_sql    unstable
    Ctn Reset Eth Connection
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_host    127.0.0.1
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Broker and Engine are not connected
    ${content}    Create List    run query: SELECT
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Should Be True    ${result}    No SELECT done by broker in the DB
    Ctn Disable Eth Connection On Port    port=3306
    Log To Console    Waiting for 1m while the connection to the DB is cut.
    Sleep    1m
    Log To Console    Reestablishing the connection and test last steps.
    Ctn Reset Eth Connection
    ${content}    Create List    0 events acknowledged
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Ctn Stop Engine
    Ctn Kindly Stop Broker

NetworkDBFail7
    [Documentation]    network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s
    [Tags]    broker    database    network
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Reset Eth Connection
    Ctn Broker Config Output Set    central    central-broker-master-sql    db_host    127.0.0.1
    Ctn Broker Config Output Set    central    central-broker-master-sql    connections_count    5
    Ctn Broker Config Output Set    central    central-broker-master-perfdata    db_host    127.0.0.1
    Ctn Broker Config Output Set    central    central-broker-master-perfdata    connections_count    5
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Broker and Engine are not connected
    ${content}    Create List    run query: SELECT
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Should Be True    ${result}    No SELECT done by broker in the DB
    FOR    ${i}    IN    0    5
        Ctn Disable Eth Connection On Port    port=3306
        Sleep    10s
        Ctn Reset Eth Connection
        Sleep    10s
    END
    ${content}    Create List    0 events acknowledged
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    There are still events in the queue.
    Ctn Stop Engine
    Ctn Kindly Stop Broker

NetworkDBFailU7
    [Documentation]    network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s (with unified_sql)
    [Tags]    broker    database    network    unified_sql
    Ctn Reset Eth Connection
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_host    127.0.0.1
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    5
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Broker and Engine are not connected
    ${content}    Create List    run query: SELECT
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Should Be True    ${result}    No SELECT done by broker in the DB
    FOR    ${i}    IN    0    5
        Ctn Disable Eth Connection On Port    port=3306
        Sleep    10s
        Ctn Reset Eth Connection
        Sleep    10s
    END
    ${content}    Create List    0 events acknowledged
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    There are still events in the queue.
    Ctn Stop Engine
    Ctn Kindly Stop Broker

NetworkDBFailU8
    [Documentation]    network failure test between broker and database: we wait for the connection to be established and then we shutdown the connection until _check_queues failure
    [Tags]    MON-71277 broker    database    network    unified_sql    unstable
    Ctn Reset Eth Connection
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_host    127.0.0.1
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    3
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Broker and Engine are not connected
    ${content}    Create List    run query: SELECT
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Should Be True    ${result}    No SELECT done by broker in the DB

    Log To Console    Connection failure.
    ${start}    Get Current Date
    Ctn Disable Eth Connection On Port    port=3306
    ${content}    Create List    fail to store queued data in database
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Should Be True    ${result}    No failure found in log

    ${start}    Get Current Date
    Log To Console    Reestablishing the connection and test last steps.
    Ctn Reset Eth Connection
    ${content}    Create List    unified_sql:_check_queues
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    40
    Ctn Stop Engine
    Ctn Kindly Stop Broker



*** Keywords ***
Ctn Disable Sleep Enable
    [Arguments]    ${interval}
    Ctn Disable Eth Connection On Port    port=3306
    Sleep    ${interval}
    Ctn Reset Eth Connection

Ctn Network Failure
    [Arguments]    ${interval}
    Ctn Reset Eth Connection
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Broker Config Output Set    central    central-broker-master-sql    db_host    127.0.0.1
    Ctn Broker Config Output Set    central    central-broker-master-sql    connections_count    10
    Ctn Broker Config Output Set    central    central-broker-master-perfdata    db_host    127.0.0.1
    Ctn Broker Config Output Set    central    central-broker-master-perfdata    connections_count    10
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Source Log    central    true
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    SQL: performing mysql_ping
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    120
    Should Be True    ${result}    We should have a call to mysql_ping every 30s on inactive connections.
    Ctn Disable Sleep Enable    ${interval}
    ${end}    Get Current Date
    ${content}    Create List    mysql_connection 0x[0-9,a-f]+ : commit
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${end}    ${content}    80
    Should Be True
    ...    ${result[0]}
    ...    timeout after network to be restablished (network failure duration : ${interval})
    Ctn Kindly Stop Broker
    Ctn Stop Engine

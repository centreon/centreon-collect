*** Settings ***
Documentation       Centreon Broker database TLS/SSL connection tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
BDBTSSLC1
    [Documentation]    Verify broker establishes TLS connection to database when DB_SSL_ENABLED is true
    [Tags]    broker    database    tls    MON-191981
    Skip If    '${DBSslEnabled}' != 'true'    Test requires DB_SSL_ENABLED=true

    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_host    127.0.0.1
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_ssl_enabled    ${DBSslEnabled}
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_ssl_verify_cert    true


    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Check for TLS configuration message in broker logs
    ${content}    Create List    mysql_connection: configuring SSL/TLS connection
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=TLS configuration message not found in broker logs

    # Verify database connection was established successfully (not just configured)
    ${content}    Create List    unified sql: stream class instanciation
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=Database connection not established

    # Verify the connection is working by checking for SQL queries in logs
    # This confirms the SSL connection was successful and broker can communicate with DB
    ${content}    Create List    run query:
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No SQL queries executed - TLS connection may have failed
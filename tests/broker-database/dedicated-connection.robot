*** Settings ***
Documentation       Centreon Broker data_bin and logs dedicated connections

Resource            ../resources/resources.robot
Library             DateTime
Library             Examples
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Stop Engine Broker And Save Logs    only_central=True


*** Test Cases ***
DEDICATED_DB_CONNECTION_${nb_conn}_${store_in_data_bin}
    [Documentation]    count database connection
    [Tags]    broker    database
    Config Broker    central
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql

    Broker Config Output Set    central    central-broker-unified-sql    db_host    127.0.0.1
    Broker Config Output Set    central    central-broker-unified-sql    connections_count    ${nb_conn}
    Broker Config Output Set    central    central-broker-unified-sql    store_in_data_bin    ${store_in_data_bin}

    ${start}    Get Current Date
    Start Broker    only_central=${True}
    ${content}    Create List    unified sql: stream class instanciation
    ${result}    Find In Log with Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No unified sql instanciation

    IF    ${nb_conn} > 1
        ${nb_dedicated}    Evaluate    ${nb_conn_expected} - 1
        ${content}    Create List    use of ${nb_dedicated} dedicated connection for logs and data_bin tables
        ${result}    Find In Log with Timeout    ${centralLog}    ${start}    ${content}    5
        Should Be True    ${result}    No dedicated message
    END

    ${connected}    Wait For Connections    3306    ${nb_conn_expected}
    Should Be True    ${connected}    no ${nb_conn_expected} connections found

    Examples:    nb_conn    nb_conn_expected    store_in_data_bin    --
    ...    1    1    yes
    ...    2    2    yes
    ...    3    3    yes
    ...    3    2    no

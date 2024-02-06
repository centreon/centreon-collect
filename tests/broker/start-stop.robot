*** Settings ***
Documentation       Centreon Broker only start/stop tests

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes


*** Test Cases ***
BSS1
    [Documentation]    Start-Stop two instances of broker and no coredump
    [Tags]    broker    start-stop
    Config Broker    central
    Config Broker    rrd
    Repeat Keyword    5 times    Start Stop Service    0

BSS2
    [Documentation]    Start/Stop 10 times broker with 300ms interval and no coredump
    [Tags]    broker    start-stop
    Config Broker    central
    Repeat Keyword    10 times    Start Stop Instance    300ms

BSS3
    [Documentation]    Start-Stop one instance of broker and no coredump
    [Tags]    broker    start-stop
    Config Broker    central
    Repeat Keyword    5 times    Start Stop Instance    0

BSS4
    [Documentation]    Start/Stop 10 times broker with 1sec interval and no coredump
    [Tags]    broker    start-stop
    Config Broker    central
    Repeat Keyword    10 times    Start Stop Instance    1s

BSS5
    [Documentation]    Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
    [Tags]    broker    start-stop
    Config Broker    central
    Broker Config Output Set    central    centreon-broker-master-rrd    one_peer_retention_mode    yes
    Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Repeat Keyword    5 times    Start Stop Instance    1s

BSSU1
    [Documentation]    Start-Stop with unified_sql two instances of broker and no coredump
    [Tags]    broker    start-stop    unified_sql
    Config Broker    central
    Config Broker    rrd
    Config Broker Sql Output    central    unified_sql
    Repeat Keyword    5 times    Start Stop Service    0

BSSU2
    [Documentation]    Start/Stop with unified_sql 10 times broker with 300ms interval and no coredump
    [Tags]    broker    start-stop    unified_sql
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Repeat Keyword    10 times    Start Stop Instance    300ms

BSSU3
    [Documentation]    Start-Stop with unified_sql one instance of broker and no coredump
    [Tags]    broker    start-stop    unified_sql
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Repeat Keyword    5 times    Start Stop Instance    0

BSSU4
    [Documentation]    Start/Stop with unified_sql 10 times broker with 1sec interval and no coredump
    [Tags]    broker    start-stop    unified_sql
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Repeat Keyword    10 times    Start Stop Instance    1s

BSSU5
    [Documentation]    Start-Stop with unified_sql with reversed connection on TCP acceptor with only one instance and no deadlock
    [Tags]    broker    start-stop    unified_sql
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Broker Config Output Set    central    centreon-broker-master-rrd    one_peer_retention_mode    yes
    Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Repeat Keyword    5 times    Start Stop Instance    1s

START_STOP_CBD
    [Documentation]    restart cbd with unified_sql services state must not be null after restart
    [Tags]    broker    start-stop    unified_sql
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Config Broker    module    ${1}
    Config BBDO3    nbEngine=1
    Config Engine    ${1}    ${50}    ${20}
    Config Broker Sql Output    central    unified_sql

    Clear Db    services
    Clear Db    hosts
    ${start}    Get Current Date

    Start Engine
    Start Broker

    # wait engine start
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial service state on (host_50,service_1000) should be raised before we can start our external commands.

    # restart central broker
    Stop Broker
    Start Broker

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    30
        Sleep    1
        ${output}    Query    SELECT state FROM services WHERE enabled=1 AND state IS NULL
        Should Be Equal    "${output}"    "()"    at least one service state is null

        ${output}    Query    SELECT state FROM hosts WHERE enabled=1 AND state IS NULL
        Should Be Equal    "${output}"    "()"    at least one host state is null
    END

    [Teardown]    Run Keywords    Stop Engine    AND    Stop Broker


*** Keywords ***
Start Stop Service
    [Arguments]    ${interval}
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-rrd.json    alias=b2
    Sleep    ${interval}
    Send Signal To Process    SIGTERM    b1
    ${result}    Wait For Process    b1    timeout=60s    on_timeout=kill
    Should Be True
    ...    ${result.rc} == -15 or ${result.rc} == 0
    ...    Broker service badly stopped with code ${result.rc}
    Send Signal To Process    SIGTERM    b2
    ${result}    Wait For Process    b2    timeout=60s    on_timeout=kill
    Should Be True
    ...    ${result.rc} == -15 or ${result.rc} == 0
    ...    Broker service badly stopped with code ${result.rc}

Start Stop Instance
    [Arguments]    ${interval}
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Sleep    ${interval}
    Send Signal To Process    SIGTERM    b1
    ${result}    Wait For Process    b1    timeout=60s    on_timeout=kill
    Should Be True
    ...    ${result.rc} == -15 or ${result.rc} == 0
    ...    Broker instance badly stopped with code ${result.rc}

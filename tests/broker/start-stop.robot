*** Settings ***
Documentation       Centreon Broker only start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
BSS1
    [Documentation]    Start-Stop two instances of broker and no coredump
    [Tags]    broker    start-stop
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Repeat Keyword    5 times    Ctn Start Stop Service    0
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BSSU1
    [Documentation]    Start-Stop two instances of broker with BBDO3 and no coredump
    [Tags]    broker    start-stop
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config BBDO3    0
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Repeat Keyword    5 times    Ctn Start Stop Service    0
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BSS2
    [Documentation]    Start/Stop 10 times broker with 300ms interval and no coredump
    [Tags]    broker    start-stop
    Ctn Config Broker    central
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Repeat Keyword    10 times    Ctn Start Stop Instance    300ms
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BSSU2
    [Documentation]    Start/Stop 10 times broker (BBDO3) with 300ms interval and no coredump
    [Tags]    broker    start-stop
    Ctn Config Broker    central
    Ctn Config BBDO3    0
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Repeat Keyword    10 times    Ctn Start Stop Instance    300ms
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BSS3
    [Documentation]    Start-Stop one instance of broker 5 times and no coredump
    [Tags]    broker    start-stop
    Ctn Config Broker    central
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Repeat Keyword    5 times    Ctn Start Stop Instance    0
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BSSU3
    [Documentation]    Start-Stop one instance of broker (BBDO3) and no coredump
    [Tags]    broker    start-stop
    Ctn Config Broker    central
    Ctn Config BBDO3    0
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Repeat Keyword    5 times    Ctn Start Stop Instance    0
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BSS4
    [Documentation]    Start/Stop 10 times broker with 1sec interval and no coredump
    [Tags]    broker    start-stop
    Ctn Config Broker    central
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Repeat Keyword    10 times    Ctn Start Stop Instance    1s
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BSSU4
    [Documentation]    Start/Stop 10 times broker with 1sec interval and no coredump
    [Tags]    broker    start-stop
    Ctn Config Broker    central
    Ctn Config BBDO3    0
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Repeat Keyword    10 times    Ctn Start Stop Instance    1s
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BSS5
    [Documentation]    Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
    [Tags]    broker    start-stop
    Ctn Config Broker    central
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    one_peer_retention_mode    yes
    Ctn Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Repeat Keyword    5 times    Ctn Start Stop Instance    1s
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

BSSU5
    [Documentation]    Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
    [Tags]    broker    start-stop
    Ctn Config Broker    central
    Ctn Config BBDO3    0
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    one_peer_retention_mode    yes
    Ctn Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Repeat Keyword    5 times    Ctn Start Stop Instance    1s
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration

START_STOP_CBD
    [Documentation]    restart cbd with unified_sql services state must not be null after restart
    [Tags]    broker    start-stop    unified_sql
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker Sql Output    central    unified_sql

    Ctn Clear Db    services
    Ctn Clear Db    hosts
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${start}    Get Current Date

    Ctn Start Engine
    Ctn Start Broker

    Ctn Wait For Engine To Be Ready    ${start}    1

    # restart central broker
    Ctn Stop Broker
    Ctn Start Broker

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    30
        Sleep    1
        ${output}    Query    SELECT state FROM services WHERE enabled=1 AND state IS NULL
        Should Be Equal    "${output}"    "()"    at least one service state is null

        ${output}    Query    SELECT state FROM hosts WHERE enabled=1 AND state IS NULL
        Should Be Equal    "${output}"    "()"    at least one host state is null
    END

    Disconnect From Database
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration
    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Stop Broker

*** Keywords ***
Ctn Start Stop Service
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

Ctn Start Stop Instance
    [Arguments]    ${interval}
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Sleep    ${interval}
    Send Signal To Process    SIGTERM    b1
    ${result}    Wait For Process    b1    timeout=60s    on_timeout=kill
    Should Be True
    ...    ${result.rc} == -15 or ${result.rc} == 0
    ...    Broker instance badly stopped with code ${result.rc}

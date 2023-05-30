*** Settings ***
Documentation       Centreon Broker only start/stop tests

Resource            ../resources/resources.robot
Library             Process
Library             OperatingSystem
Library             ../resources/Broker.py

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


*** Keywords ***
Start Stop Service
    [Arguments]    ${interval}
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-rrd.json    alias=b2
    Sleep    ${interval}
    Send Signal To Process    SIGTERM    b1
    ${result}=    Wait For Process    b1    timeout=60s    on_timeout=kill
    Should Be True
    ...    ${result.rc} == -15 or ${result.rc} == 0
    ...    msg=Broker service badly stopped with code ${result.rc}
    Send Signal To Process    SIGTERM    b2
    ${result}=    Wait For Process    b2    timeout=60s    on_timeout=kill
    Should Be True
    ...    ${result.rc} == -15 or ${result.rc} == 0
    ...    msg=Broker service badly stopped with code ${result.rc}

Start Stop Instance
    [Arguments]    ${interval}
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Sleep    ${interval}
    Send Signal To Process    SIGTERM    b1
    ${result}=    Wait For Process    b1    timeout=60s    on_timeout=kill
    Should Be True
    ...    ${result.rc} == -15 or ${result.rc} == 0
    ...    msg=Broker instance badly stopped with code ${result.rc}

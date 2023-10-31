*** Settings ***
Documentation       Centreon Broker only start/stop tests

Resource            ../resources/resources.robot
Library             Process
Library             OperatingSystem
Library             ../resources/Broker.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Test Cases ***
BGRPCSS1
    [Documentation]    Start-Stop two instances of broker configured with grpc stream and no coredump
    [Tags]    broker    start-stop    grpc
    Config Broker    central
    Config Broker    rrd
    Change Broker Tcp Output To Grpc    central
    Change Broker Tcp Input To Grpc    rrd
    Repeat Keyword    5 times    Start Stop Service    100ms

BGRPCSS2
    [Documentation]    Start/Stop 10 times broker configured with grpc stream with 300ms interval and no coredump
    [Tags]    broker    start-stop    grpc
    Config Broker    central
    Change Broker Tcp Output To Grpc    central
    Repeat Keyword    10 times    Start Stop Instance    300ms

BGRPCSS3
    [Documentation]    Start-Stop one instance of broker configured with grpc stream and no coredump
    [Tags]    broker    start-stop    grpc
    Config Broker    central
    Change Broker Tcp Output To Grpc    central
    Repeat Keyword    5 times    Start Stop Instance    100ms

BGRPCSS4
    [Documentation]    Start/Stop 10 times broker configured with grpc stream with 1sec interval and no coredump
    [Tags]    broker    start-stop    grpc
    Config Broker    central
    Change Broker Tcp Output To Grpc    central
    Repeat Keyword    10 times    Start Stop Instance    1s

BGRPCSS5
    [Documentation]    Start-Stop with reversed connection on grpc acceptor with only one instance and no deadlock
    [Tags]    broker    start-stop    grpc
    Config Broker    central
    Change Broker Tcp Output To Grpc    central
    Broker Config Output Set    central    centreon-broker-master-rrd    one_peer_retention_mode    yes
    Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Repeat Keyword    5 times    Start Stop Instance    1s

BGRPCSSU1
    [Documentation]    Start-Stop with unified_sql two instances of broker with grpc stream and no coredump
    [Tags]    broker    start-stop    unified_sql    grpc
    Config Broker    central
    Config Broker    rrd
    Config Broker Sql Output    central    unified_sql
    Change Broker Tcp Output To Grpc    central
    Change Broker Tcp Input To Grpc    rrd
    Repeat Keyword    5 times    Start Stop Service    100ms

BGRPCSSU2
    [Documentation]    Start/Stop with unified_sql 10 times broker configured with grpc stream with 300ms interval and no coredump
    [Tags]    broker    start-stop    unified_sql    grpc
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Change Broker Tcp Output To Grpc    central
    Repeat Keyword    10 times    Start Stop Instance    300ms

BGRPCSSU3
    [Documentation]    Start-Stop with unified_sql one instance of broker configured with grpc and no coredump
    [Tags]    broker    start-stop    unified_sql    grpc
    Config Broker    central
    Change Broker Tcp Output To Grpc    central
    Config Broker Sql Output    central    unified_sql
    Repeat Keyword    5 times    Start Stop Instance    100ms

BGRPCSSU4
    [Documentation]    Start/Stop with unified_sql 10 times broker configured with grpc stream with 1sec interval and no coredump
    [Tags]    broker    start-stop    unified_sql    grpc
    Config Broker    central
    Change Broker Tcp Output To Grpc    central
    Config Broker Sql Output    central    unified_sql
    Repeat Keyword    10 times    Start Stop Instance    1s

BGRPCSSU5
    [Documentation]    Start-Stop with unified_sql with reversed connection on grpc acceptor with only one instance and no deadlock
    [Tags]    broker    start-stop    unified_sql    grpc
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Broker Config Output Set    central    centreon-broker-master-rrd    one_peer_retention_mode    yes
    Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Change Broker Tcp Output To Grpc    central
    Repeat Keyword    5 times    Start Stop Instance    1s


*** Keywords ***
Start Stop Service
    [Arguments]    ${interval}
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-rrd.json    alias=b2
    Sleep    ${interval}
    Kindly Stop Broker

Start Stop Instance
    [Arguments]    ${interval}
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Sleep    ${interval}
    Kindly Stop Broker    True
    Send Signal To Process    SIGTERM    b1
    ${result}    Wait For Process    b1    timeout=60s    on_timeout=kill
    Should Be True    ${result.rc} == -15 or ${result.rc} == 0    Broker instance badly stopped

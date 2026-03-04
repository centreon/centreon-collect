*** Settings ***
Documentation       Centreon Broker only start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BGRPCSS1
    [Documentation]    Scenario: Two broker instances with grpc stream start and stop cleanly
    ...    Given central broker with grpc output and rrd broker with grpc input
    ...    When both brokers are started and stopped 5 times with 100ms interval in new generation mode
    ...    Then no coredump occurs and the connection is established each time
    [Tags]    broker    start-stop    grpc
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Repeat Keyword    5 times    Ctn Start Stop Service    100ms

BGRPCSS2
    [Documentation]    Scenario: Single broker instance with grpc starts and stops 10 times with 300ms interval
    ...    Given central broker with grpc output
    ...    When the broker is started and stopped 10 times with 300ms interval in new generation mode
    ...    Then no coredump occurs
    [Tags]    broker    start-stop    grpc
    Ctn Config Broker    central
    Ctn Change Broker Tcp Output To Grpc    central
    Repeat Keyword    10 times    Ctn Start Stop Instance    300ms

BGRPCSS3
    [Documentation]    Scenario: Single broker instance with grpc starts and stops 5 times with 100ms interval
    ...    Given central broker with grpc output
    ...    When the broker is started and stopped 5 times with 100ms interval in new generation mode
    ...    Then no coredump occurs
    [Tags]    broker    start-stop    grpc
    Ctn Config Broker    central
    Ctn Change Broker Tcp Output To Grpc    central
    Repeat Keyword    5 times    Ctn Start Stop Instance    100ms

BGRPCSS4
    [Documentation]    Scenario: Single broker instance with grpc starts and stops 10 times with 1s interval
    ...    Given central broker with grpc output
    ...    When the broker is started and stopped 10 times with 1s interval in new generation mode
    ...    Then no coredump occurs
    [Tags]    broker    start-stop    grpc
    Ctn Config Broker    central
    Ctn Change Broker Tcp Output To Grpc    central
    Repeat Keyword    10 times    Ctn Start Stop Instance    1s

BGRPCSS5
    [Documentation]    Scenario: Reversed grpc acceptor with one_peer_retention_mode starts and stops without deadlock
    ...    Given central broker with grpc output in one_peer_retention_mode with no host configured
    ...    When the broker is started and stopped 5 times with 1s interval in new generation mode
    ...    Then no deadlock occurs
    [Tags]    broker    start-stop    grpc
    Ctn Config Broker    central
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    one_peer_retention_mode    yes
    Ctn Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Repeat Keyword    5 times    Ctn Start Stop Instance    1s

BGRPCSSU1
    [Documentation]    Scenario: Two broker instances with unified_sql and grpc stream start and stop cleanly
    ...    Given central broker with unified_sql and grpc output and rrd broker with grpc input
    ...    When both brokers are started and stopped 5 times with 100ms interval in new generation mode
    ...    Then no coredump occurs and the connection is established each time
    [Tags]    broker    start-stop    unified_sql    grpc
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Repeat Keyword    5 times    Ctn Start Stop Service    100ms

BGRPCSSU2
    [Documentation]    Scenario: Single broker instance with unified_sql and grpc starts and stops 10 times with 300ms interval
    ...    Given central broker with unified_sql and grpc output
    ...    When the broker is started and stopped 10 times with 300ms interval in new generation mode
    ...    Then no coredump occurs
    [Tags]    broker    start-stop    unified_sql    grpc
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Change Broker Tcp Output To Grpc    central
    Repeat Keyword    10 times    Ctn Start Stop Instance    300ms

BGRPCSSU3
    [Documentation]    Scenario: Single broker instance with unified_sql and grpc starts and stops 5 times with 100ms interval
    ...    Given central broker with unified_sql and grpc output
    ...    When the broker is started and stopped 5 times with 100ms interval in new generation mode
    ...    Then no coredump occurs
    [Tags]    broker    start-stop    unified_sql    grpc
    Ctn Config Broker    central
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Config Broker Sql Output    central    unified_sql
    Repeat Keyword    5 times    Ctn Start Stop Instance    100ms

BGRPCSSU4
    [Documentation]    Scenario: Single broker instance with unified_sql and grpc starts and stops 10 times with 1s interval
    ...    Given central broker with unified_sql and grpc output
    ...    When the broker is started and stopped 10 times with 1s interval in new generation mode
    ...    Then no coredump occurs
    [Tags]    broker    start-stop    unified_sql    grpc
    Ctn Config Broker    central
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Config Broker Sql Output    central    unified_sql
    Repeat Keyword    10 times    Ctn Start Stop Instance    1s

BGRPCSSU5
    [Documentation]    Scenario: Reversed grpc acceptor with unified_sql and one_peer_retention_mode starts and stops without deadlock
    ...    Given central broker with unified_sql and grpc output in one_peer_retention_mode
    ...    When the broker is started and stopped 5 times with 1s interval in new generation mode
    ...    Then no deadlock occurs
    [Tags]    broker    start-stop    unified_sql    grpc
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    one_peer_retention_mode    yes
    Ctn Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Ctn Change Broker Tcp Output To Grpc    central
    Repeat Keyword    5 times    Ctn Start Stop Instance    1s


*** Keywords ***
Ctn Start Stop Service
    [Arguments]    ${interval}
    Ctn Broker Config Flush
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-rrd.json    alias=b2
    Sleep    ${interval}
    Ctn Kindly Stop Broker

Ctn Start Stop Instance
    [Arguments]    ${interval}
    Ctn Broker Config Flush
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Sleep    ${interval}
    Ctn Kindly Stop Broker    True
    Send Signal To Process    SIGTERM    b1
    ${result}    Wait For Process    b1    timeout=60s    on_timeout=kill
    Should Be True    ${result.rc} == -15 or ${result.rc} == 0    Broker instance badly stopped

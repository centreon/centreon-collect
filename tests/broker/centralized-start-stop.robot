*** Settings ***
Documentation       Centreon Broker only start/stop tests in centralized configuration mode

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CBSS1
    [Documentation]    Scenario: Two broker instances start and stop cleanly 5 times in new generation mode
    ...    Given central and rrd brokers configured in new generation mode
    ...    When both brokers are started and stopped 5 times immediately
    ...    Then no coredump occurs each time
    [Tags]    broker    start-stop
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Repeat Keyword    5 times    Ctn Start Stop Service    0

CBSS2
    [Documentation]    Scenario: Single broker instance starts and stops 10 times with 300ms interval in new generation mode
    ...    Given central broker configured in new generation mode
    ...    When the broker is started and stopped 10 times with 300ms interval
    ...    Then no coredump occurs
    [Tags]    broker    start-stop
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Repeat Keyword    10 times    Ctn Start Stop Instance    300ms

CBSS3
    [Documentation]    Scenario: Single broker instance starts and stops 5 times immediately in new generation mode
    ...    Given central broker configured in new generation mode
    ...    When the broker is started and stopped 5 times immediately
    ...    Then no coredump occurs
    [Tags]    broker    start-stop
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Repeat Keyword    5 times    Ctn Start Stop Instance    0

CBSS4
    [Documentation]    Scenario: Single broker instance starts and stops 10 times with 1s interval in new generation mode
    ...    Given central broker configured in new generation mode
    ...    When the broker is started and stopped 10 times with 1s interval
    ...    Then no coredump occurs
    [Tags]    broker    start-stop
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Repeat Keyword    10 times    Ctn Start Stop Instance    1s

CBSS5
    [Documentation]    Scenario: Reversed TCP connection with one_peer_retention_mode starts and stops without deadlock in new generation mode
    ...    Given central broker configured with one_peer_retention_mode and no host on the rrd output
    ...    When the broker is started and stopped 5 times with 1s interval in new generation mode
    ...    Then no deadlock occurs
    [Tags]    broker    start-stop
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    one_peer_retention_mode    yes
    Ctn Broker Config Output Remove    central    centreon-broker-master-rrd    host
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    Repeat Keyword    5 times    Ctn Start Stop Instance    1s

CBSS_CBD
    [Documentation]    Scenario: Broker restart with unified_sql preserves non-null service and host states
    ...    Given broker and engine are started in new generation mode
    ...    And broker is then restarted
    ...    When services and hosts are queried from the database for 30 seconds
    ...    Then no service or host state is null
    [Tags]    broker    start-stop    unified_sql
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}

    Ctn Clear Db    services
    Ctn Clear Db    hosts
    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${start}    Ctn Get Round Current Date

    Ctn Start Engine    newGeneration=True
    Ctn Start Broker    newGeneration=True

    Ctn Wait For Engine To Be Ready    ${start}    1

    # restart central broker
    Ctn Kindly Stop Broker
    Ctn Start Broker    newGeneration=True

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    30
        Sleep    1
        ${output}    Query    SELECT state FROM services WHERE enabled=1 AND state IS NULL
        Should Be Equal    "${output}"    "()"    at least one service state is null

        ${output}    Query    SELECT state FROM hosts WHERE enabled=1 AND state IS NULL
        Should Be Equal    "${output}"    "()"    at least one host state is null
    END

    Disconnect From Database
    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Stop Broker


*** Keywords ***
Ctn Start Stop Service
    [Arguments]    ${interval}
    Ctn Broker Config Flush
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
    Ctn Broker Config Flush
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Sleep    ${interval}
    Send Signal To Process    SIGTERM    b1
    ${result}    Wait For Process    b1    timeout=60s    on_timeout=kill
    Should Be True
    ...    ${result.rc} == -15 or ${result.rc} == 0
    ...    Broker instance badly stopped with code ${result.rc}

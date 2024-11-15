*** Settings ***
Documentation       Centreon Broker only start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
BC1
    [Documentation]    Central and RRD brokers are started.
    ...    Then we check they are correctly connected.
    ...    RRD broker is stopped. The connection is lost.
    ...    Then RRD broker is started again. The connection is re-established.
    ...    Central broker is stopped. The connection is lost.
    ...    Then Central broker is started again. The connection is re-established.
    [Tags]    broker    start-stop    unified_sql    MON-15671
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config BBDO3    ${0}

    Remove Directory    ${varRoot}/lib/centreon-broker/pollers-configuration    recursive=True
    ${start}    Get Current Date

    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-rrd.json    alias=b2

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between central and rrd broker is KO

    ${result}    Ctn Get Peers    51001
    Log To Console    ${result}

    # RRD cbd is stopped
    Send Signal To Process    SIGTERM    b2
    ${result}    Wait For Process    b2    timeout=60s    on_timeout=kill
    Should Be True
    ...    ${result.rc} == -15 or ${result.rc} == 0
    ...    RRD Broker badly stopped with code ${result.rc}

    # RRD cbd is started
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-rrd.json    alias=b2

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between central and rrd broker is KO

    ${result}    Ctn Get Peers    51001
    Log To Console    ${result}

    # Central cbd is stopped
    Send Signal To Process    SIGTERM    b1
    ${result}    Wait For Process    b1    timeout=60s    on_timeout=kill
    Should Be True
    ...    ${result.rc} == -15 or ${result.rc} == 0
    ...    Central Broker badly stopped with code ${result.rc}

    # Central cbd is started
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between central and rrd broker is KO

    ${result}    Ctn Get Peers    51001
    Log To Console    ${result}
    Should Not Exist    ${varRoot}/lib/centreon-broker/pollers-configuration
    Ctn Kindly Stop Broker

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

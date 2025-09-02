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


BBDO_300_3100
    [Documentation]    Given a central configured in bbdo 3.0.0 and a rrd cbd configured in 3.1.0, rrd cbd should operate in bbdo 3.0.0
    ...    Then we wait event of type 131074 (bbdo2 ack).
    [Tags]    broker    MON-173480
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module

    Ctn Config BBDO3    nbEngine=1    version=3.0.0

    Ctn Broker Config Add Item    rrd    bbdo_version    3.1.0
    Ctn Broker Config Add Item    central    event_queue_max_size    ${10}
    Ctn Broker Config Add Item    rrd    event_queue_max_size    ${10}
    Ctn Broker Config Log    central    bbdo    trace
    Ctn Broker Config Log    rrd    bbdo    info

    Ctn Clear Retention
    
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    ${pid1}    Get Process Id    b1
    ${pid2}    Get Process Id    b2
    ${result}    Ctn Check Connection    5670    ${pid1}    ${pid2}
    Should Be True    ${result}    The connection between cbd central and rrd is not established.

    ${content}    Create List    bbdo version downgraded to 3.0.0
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    30
    Should Be True    ${result}    No bbdo downgraded message in rrd logs.

    ${content}    Create List    unserialized 20 bytes for event of type 131074
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    No bbdo2 ack received.

    [Teardown]    Ctn Stop Engine Broker And Save Logs

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

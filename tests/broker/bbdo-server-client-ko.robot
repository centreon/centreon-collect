*** Settings ***
Documentation       Centreon Broker start/stop tests with bbdo_server and bbdo_client input/output streams. Only these streams are used instead of grpc and tcp.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
BSCSSK1
    [Documentation]    Start-Stop two instances of broker, server configured with grpc and client with tcp. No connectrion established and error raised on client side.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    tcp
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    tcp    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    grpc
    Ctn Broker Config Log    central    grpc    debug
    Ctn Broker Config Log    central    tcp    debug
    Ctn Broker Config Log    rrd    grpc    debug
    Ctn Broker Config Log    rrd    tcp    debug
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    rrd    core    error
    ${start}    Get Current Date    exclude_millis=True
    Sleep    1s
    Ctn Start Broker

    # Client cannot connect. It returns an error
    ${content}    Create List    peer tcp://localhost:5670 is sending corrupted data
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No message about the bad connection.

    Ctn Kindly Stop Broker

BSCSSK2
    [Documentation]    Start-Stop two instances of broker, server configured with tcp and client with grpc. No connection established and error raised on client side.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    grpc
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    grpc    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    tcp
    Ctn Broker Config Log    central    grpc    debug
    Ctn Broker Config Log    central    tcp    debug
    Ctn Broker Config Log    rrd    grpc    debug
    Ctn Broker Config Log    rrd    tcp    debug
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    rrd    core    error
    ${start}    Get Current Date    exclude_millis=True
    Sleep    1s
    Ctn Start Broker

    # Client cannot connect. It returns an error
    ${content}    Create List
    ...    BBDO: invalid protocol header, aborting connection: waiting for message of type 'version_response' but nothing received
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No message about the bad connection.

    Ctn Kindly Stop Broker


*** Keywords ***
Ctn Start Stop Service
    [Arguments]    ${interval}
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-rrd.json    alias=b2
    Sleep    ${interval}
    ${pid1}    Get Process Id    b1
    ${pid2}    Get Process Id    b2
    ${result}    Ctn Check Connection    5670    ${pid1}    ${pid2}
    Should Be True    ${result}    The connection between cbd central and rrd is not established.

    Send Signal To Process    SIGTERM    b1
    ${result}    Ctn Wait Or Dump And Kill Process    b1    /usr/sbin/cbd    60s
    Should Be True    ${result.rc} == -15 or ${result.rc} == 0    Broker service badly stopped
    Send Signal To Process    SIGTERM    b2
    ${result}    Ctn Wait Or Dump And Kill Process    b2    /usr/sbin/cbd    60s
    Should Be True    ${result.rc} == -15 or ${result.rc} == 0    Broker service badly stopped

Ctn Start Stop Instance
    [Arguments]    ${interval}
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Sleep    ${interval}
    Send Signal To Process    SIGTERM    b1
    ${result}    Ctn Wait Or Dump And Kill Process    b1    /usr/sbin/cbd    60s
    Should Be True    ${result.rc} == -15 or ${result.rc} == 0    Broker instance badly stopped

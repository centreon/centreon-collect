*** Settings ***
Documentation       Centreon Broker start/stop tests with bbdo_server and bbdo_client input/output streams. Only these streams are used instead of grpc and tcp.

Resource            ../resources/resources.resource
Library             Process
Library             OperatingSystem
Library             ../resources/Broker.py
Library             DateTime

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes


*** Test Cases ***
BSCSSK1
    [Documentation]    Start-Stop two instances of broker, server configured with grpc and client with tcp. No connectrion established and error raised on client side.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    tcp
    Config Broker BBDO Output    central    bbdo_client    5670    tcp    localhost
    Config Broker BBDO Input    rrd    bbdo_server    5670    grpc
    Broker Config Log    central    grpc    debug
    Broker Config Log    central    tcp    debug
    Broker Config Log    rrd    grpc    debug
    Broker Config Log    rrd    tcp    debug
    Broker Config Log    central    core    error
    Broker Config Log    rrd    core    error
    ${start}=    Get Current Date    exclude_millis=True
    Sleep    1s
    Start Broker

    # Client cannot connect. It returns an error
    ${content}=    Create List    peer tcp://localhost:5670 is sending corrupted data
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No message about the bad connection.

    Kindly Stop Broker

BSCSSK2
    [Documentation]    Start-Stop two instances of broker, server configured with tcp and client with grpc. No connection established and error raised on client side.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    grpc
    Config Broker BBDO Output    central    bbdo_client    5670    grpc    localhost
    Config Broker BBDO Input    rrd    bbdo_server    5670    tcp
    Broker Config Log    central    grpc    debug
    Broker Config Log    central    tcp    debug
    Broker Config Log    rrd    grpc    debug
    Broker Config Log    rrd    tcp    debug
    Broker Config Log    central    core    error
    Broker Config Log    rrd    core    error
    ${start}=    Get Current Date    exclude_millis=True
    Sleep    1s
    Start Broker

    # Client cannot connect. It returns an error
    ${content}=    Create List
    ...    BBDO: invalid protocol header, aborting connection: waiting for message of type 'version_response' but nothing received
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No message about the bad connection.

    Kindly Stop Broker


*** Keywords ***
Start Stop Service
    [Arguments]    ${interval}
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-rrd.json    alias=b2
    Sleep    ${interval}
    ${pid1}=    Get Process Id    b1
    ${pid2}=    Get Process Id    b2
    ${result}=    check connection    5670    ${pid1}    ${pid2}
    Should Be True    ${result}    msg=The connection between cbd central and rrd is not established.

    Send Signal To Process    SIGTERM    b1
    ${result}=    Wait Or Dump And Kill Process    b1    60s
    Should Be True    ${result.rc} == -15 or ${result.rc} == 0    msg=Broker service badly stopped
    Send Signal To Process    SIGTERM    b2
    ${result}=    Wait Or Dump And Kill Process    b2    60s
    Should Be True    ${result.rc} == -15 or ${result.rc} == 0    msg=Broker service badly stopped

Start Stop Instance
    [Arguments]    ${interval}
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Sleep    ${interval}
    Send Signal To Process    SIGTERM    b1
    ${result}=    Wait Or Dump And Kill Process    b1    60s
    Should Be True    ${result.rc} == -15 or ${result.rc} == 0    msg=Broker instance badly stopped

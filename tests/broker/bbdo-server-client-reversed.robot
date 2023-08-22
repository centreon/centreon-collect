*** Settings ***
Documentation       Centreon Broker start/stop tests with bbdo_server and bbdo_client input/output streams. Only these streams are used instead of grpc and tcp.

Resource            ../resources/resources.resource
Library             DateTime
Library             Process
Library             OperatingSystem
Library             ../resources/Broker.py

Suite Setup         Prepare Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save logs If Failed


*** Test Cases ***
BSCSSR1
    [Documentation]    Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client and reversed.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    tcp
    Config Broker BBDO Output    central    bbdo_server    5670    tcp
    Config Broker BBDO Input    rrd    bbdo_client    5670    tcp    localhost
    Broker Config Log    central    config    debug
    Repeat Keyword    5 times    Start Stop Service    0

BSCSSRR1
    [Documentation]    Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client, reversed and retention. central-broker-master-output is then a failover.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    tcp
    Config Broker BBDO Output    central    bbdo_server    5670    tcp
    Broker Config Output set    central    central-broker-master-output    retention    yes
    Config Broker BBDO Input    rrd    bbdo_client    5670    tcp    localhost
    Broker Config Log    central    config    debug
    ${start}=    Get Round Current Date
    Repeat Keyword    5 times    Start Stop Service    0
    ${content}=    Create List    failover 'central-broker-master-output' construction.
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No information about TLS activation.

BSCSSPRR1
    [Documentation]    Start-Stop two instances of broker and no coredump. The server contains a listen address, reversed and retention. central-broker-master-output is then a failover.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    tcp
    Config Broker BBDO Output    central    bbdo_server    5670    tcp    localhost
    Config Broker BBDO Input    rrd    bbdo_client    5670    tcp    localhost
    Broker Config Output set    central    central-broker-master-output    retention    yes
    Broker Config Log    central    config    info
    Repeat Keyword    5 times    Start Stop Service    0

BSCSSRR2
    [Documentation]    Start/Stop 10 times broker with 300ms interval and no coredump, reversed and retention. central-broker-master-output is then a failover.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Config Broker    central
    Config Broker BBDO Input    central    bbdo_client    5669    tcp    localhost
    Config Broker BBDO Output    central    bbdo_server    5670    tcp
    Broker Config Output set    central    central-broker-master-output    retention    yes
    Repeat Keyword    10 times    Start Stop Instance    300ms

BSCSSGRR1
    [Documentation]    Start-Stop two instances of broker and no coredump, reversed and retention, with transport protocol grpc, start-stop 5 times.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    grpc    localhost
    Config Broker BBDO Output    central    bbdo_server    5670    grpc
    Broker Config Output set    central    central-broker-master-output    retention    yes
    Config Broker BBDO Input    rrd    bbdo_client    5670    grpc    localhost
    Broker Config Log    central    config    info
    ${start}=    Get Round Current Date
    Repeat Keyword    5 times    Start Stop Service    0
    ${content}=    Create List
    ...    endpoint applier: creating new failover 'central-broker-master-output'
    ...    failover 'central-broker-master-output' construction.
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No information about TLS activation.

BSCSSTRR1
    [Documentation]    Start-Stop two instances of broker and no coredump. Encryption is enabled. transport protocol is tcp, reversed and retention.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp    tls
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Output    central    bbdo_server    5670    tcp
    Config Broker BBDO Input    rrd    bbdo_client    5670    tcp    localhost
    Broker Config Output set    central    central-broker-master-output    encryption    yes
    Broker Config Output set    central    central-broker-master-output    retention    yes
    Broker Config Input set    rrd    rrd-broker-master-input    encryption    yes
    Broker Config Log    central    config    off
    Broker Config Log    central    core    off
    Broker Config Log    central    tls    debug
    ${start}=    Get Round Current Date
    Repeat Keyword    5 times    Start Stop Service    0
    ${content}=    Create List    TLS: successful handshake
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No information about TLS activation.

BSCSSTRR2
    [Documentation]    Start-Stop two instances of broker and no coredump. Encryption is enabled.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp    tls
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Output    central    bbdo_server    5670    tcp
    Config Broker BBDO Input    rrd    bbdo_client    5670    tcp    localhost
    Broker Config Output set    central    central-broker-master-output    encryption    yes
    Broker Config Output set    central    central-broker-master-output    retention    yes
    Broker Config Input set    rrd    rrd-broker-master-input    encryption    yes
    Broker Config Log    central    config    off
    Broker Config Log    central    core    off
    Broker Config Log    central    tls    debug
    Broker Config Output set
    ...    central
    ...    central-broker-master-output
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/server.key
    Broker Config Output set
    ...    central
    ...    central-broker-master-output
    ...    certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Broker Config Output set
    ...    central
    ...    central-broker-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Broker Config Input set    rrd    rrd-broker-master-input    private_key    ${EtcRoot}/centreon-broker/client.key
    Broker Config Input set    rrd    rrd-broker-master-input    certificate    ${EtcRoot}/centreon-broker/client.crt
    ${start}=    Get Round Current Date
    Repeat Keyword    5 times    Start Stop Service    0
    ${content}=    Create List    TLS: successful handshake
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No information about TLS activation.

BSCSSTGRR2
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys. Reversed grpc connection with retention.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc    tls
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Output    central    bbdo_server    5670    grpc
    Config Broker BBDO Input    rrd    bbdo_client    5670    grpc    localhost
    Broker Config Output set    central    central-broker-master-output    encryption    yes
    Broker Config Output set    central    central-broker-master-output    retention    yes
    Broker Config Input set    rrd    rrd-broker-master-input    encryption    yes
    Broker Config Flush Log    central    0
    Broker Config Flush Log    rrd    0
    Broker Config Log    central    config    off
    Broker Config Log    central    core    off
    Broker Config Log    rrd    core    off
    Broker Config Log    central    tls    debug
    Broker Config Log    central    grpc    debug
    Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/server.key
    ...    ${EtcRoot}/centreon-broker/server.crt
    Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/client.key
    ...    ${EtcRoot}/centreon-broker/client.crt

    Broker Config Output set
    ...    central
    ...    central-broker-master-output
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/server.key
    Broker Config Output set
    ...    central
    ...    central-broker-master-output
    ...    certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Broker Config Input set    rrd    rrd-broker-master-input    private_key    ${EtcRoot}/centreon-broker/client.key
    Broker Config Input set    rrd    rrd-broker-master-input    certificate    ${EtcRoot}/centreon-broker/client.crt
    Broker Config Input set
    ...    rrd
    ...    rrd-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    ${start}=    Get Round Current Date
    Start Broker
    ${content}=    Create List    write: buff:    write done: buff:
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No information about TLS activation.
    Kindly Stop Broker

BSCSSCRR1
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side. Connection reversed with retention.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Output    central    bbdo_server    5670    tcp
    Config Broker BBDO Input    rrd    bbdo_client    5670    tcp    localhost
    Broker Config Output set    central    central-broker-master-output    compression    yes
    Broker Config Output set    central    central-broker-master-output    retention    yes
    Broker Config Log    central    config    off
    Broker Config Log    central    core    trace
    Broker Config Log    rrd    core    trace
    Broker Config Flush Log    central    0
    ${start}=    Get Round Current Date
    Start Broker
    ${content}=    Create List    compression: writing
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No compression enabled
    Kindly Stop Broker

BSCSSCRR2
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side. Connection reversed with retention.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Output    central    bbdo_server    5670    tcp
    Config Broker BBDO Input    rrd    bbdo_client    5670    tcp    localhost
    Broker Config Output set    central    central-broker-master-output    compression    no
    Broker Config Output set    central    central-broker-master-output    retention    yes
    Broker Config Log    central    config    off
    Broker Config Log    central    core    off
    Broker Config Log    rrd    core    trace
    Broker Config Log    central    bbdo    trace
    Broker Config Flush Log    central    0
    ${start}=    Get Round Current Date
    Start Broker
    ${content}=    Create List    BBDO: we have extensions '' and peer has 'COMPRESSION'
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=Compression enabled but should not.
    Kindly Stop Broker

BSCSSCGRR1
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression    tls
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Output    central    bbdo_server    5670    grpc
    Config Broker BBDO Input    rrd    bbdo_client    5670    grpc    localhost
    Broker Config Output set    central    central-broker-master-output    compression    yes
    Broker Config Output set    central    central-broker-master-output    retention    yes
    Broker Config Log    central    config    off
    Broker Config Log    central    core    off
    Broker Config Log    rrd    core    off
    Broker Config Log    central    tls    debug
    Broker Config Log    central    grpc    debug
    Broker Config Flush Log    central    0
    ${start}=    Get Round Current Date
    Start Broker
    ${content}=    Create List    server default compression deflate
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No compression enabled
    Kindly Stop Broker

BSCSSCGRR2
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression    tls
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Output    central    bbdo_server    5670    grpc
    Config Broker BBDO Input    rrd    bbdo_client    5670    grpc    localhost
    Broker Config Output set    central    central-broker-master-output    compression    no
    Broker Config Output set    central    central-broker-master-output    retention    yes
    Broker Config Log    central    config    off
    Broker Config Log    central    core    off
    Broker Config Log    rrd    core    off
    Broker Config Log    central    tls    debug
    Broker Config Log    central    grpc    debug
    Broker Config Flush Log    central    0
    ${start}=    Get Round Current Date
    Start Broker
    ${content}=    Create List    server default compression deflate
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    15
    Should Be True    not ${result}    msg=No compression enabled
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

Prepare Suite
    Clean Before Suite
    Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/server.key
    ...    ${EtcRoot}/centreon-broker/server.crt
    Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/client.key
    ...    ${EtcRoot}/centreon-broker/client.crt

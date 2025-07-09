*** Settings ***
Documentation       Centreon Broker start/stop tests with bbdo_server and bbdo_client input/output streams. Only these streams are used instead of grpc and tcp.

Resource            ../resources/import.resource

Suite Setup         Ctn Prepare Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BSCSSR1
    [Documentation]    Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client and reversed.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    tcp
    Ctn Config Broker Bbdo Output    central    bbdo_server    5670    tcp
    Ctn Config Broker Bbdo Input    rrd    bbdo_client    5670    tcp    localhost
    Ctn Broker Config Log    central    config    debug
    Repeat Keyword    5 times    Ctn Start Stop Service    0

BSCSSRR1
    [Documentation]    Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client, reversed and retention. centreon-broker-master-rrd is then a failover.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    tcp
    Ctn Config Broker Bbdo Output    central    bbdo_server    5670    tcp
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    retention    yes
    Ctn Config Broker Bbdo Input    rrd    bbdo_client    5670    tcp    localhost
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    processing    trace
    ${start}    Ctn Get Round Current Date
    Repeat Keyword    5 times    Ctn Start Stop Service    0
    ${content}    Create List    failover 'centreon-broker-master-rrd' construction.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No information about TLS activation.

BSCSSPRR1
    [Documentation]    Start-Stop two instances of broker and no coredump. The server contains a listen address, reversed and retention. centreon-broker-master-rrd is then a failover.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    tcp
    Ctn Config Broker Bbdo Output    central    bbdo_server    5670    tcp    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_client    5670    tcp    localhost
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    retention    yes
    Ctn Broker Config Log    central    config    info
    Repeat Keyword    5 times    Ctn Start Stop Service    0

BSCSSRR2
    [Documentation]    Start/Stop 10 times broker with 300ms interval and no coredump, reversed and retention. centreon-broker-master-rrd is then a failover.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Ctn Config Broker    central
    Ctn Config Broker Bbdo Input    central    bbdo_client    5669    tcp    localhost
    Ctn Config Broker Bbdo Output    central    bbdo_server    5670    tcp
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    retention    yes
    Repeat Keyword    10 times    Ctn Start Stop Instance    300ms

BSCSSGRR1
    [Documentation]    Start-Stop two instances of broker and no coredump, reversed and retention, with transport protocol grpc, start-stop 5 times.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    grpc    localhost
    Ctn Config Broker Bbdo Output    central    bbdo_server    5670    grpc
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    retention    yes
    Ctn Config Broker Bbdo Input    rrd    bbdo_client    5670    grpc    localhost
    Ctn Broker Config Log    central    config    info
    Ctn Broker Config Log    central    processing    trace
    ${start}    Ctn Get Round Current Date
    Repeat Keyword    5 times    Ctn Start Stop Service    0
    ${content}    Create List
    ...    endpoint applier: creating new failover 'centreon-broker-master-rrd'
    ...    failover 'centreon-broker-master-rrd' construction.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No information about TLS activation.

BSCSSTRR1
    [Documentation]    Start-Stop two instances of broker and no coredump. Encryption is enabled. transport protocol is tcp, reversed and retention.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp    tls
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Output    central    bbdo_server    5670    tcp
    Ctn Config Broker Bbdo Input    rrd    bbdo_client    5670    tcp    localhost
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    encryption    yes
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    retention    yes
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    encryption    yes
    Ctn Broker Config Log    central    config    off
    Ctn Broker Config Log    central    core    off
    Ctn Broker Config Log    central    tls    debug
    ${start}    Ctn Get Round Current Date
    Repeat Keyword    5 times    Ctn Start Stop Service    0
    ${content}    Create List    TLS: successful handshake
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No information about TLS activation.

BSCSSTRR2
    [Documentation]    Start-Stop two instances of broker and no coredump. Encryption is enabled.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp    tls
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Output    central    bbdo_server    5670    tcp
    Ctn Config Broker Bbdo Input    rrd    bbdo_client    5670    tcp    localhost
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    encryption    yes
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    retention    yes
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    encryption    yes
    Ctn Broker Config Log    central    config    off
    Ctn Broker Config Log    central    core    off
    Ctn Broker Config Log    central    tls    debug
    Ctn Broker Config Output Set
    ...    central
    ...    centreon-broker-master-rrd
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/server.key
    Ctn Broker Config Output Set
    ...    central
    ...    centreon-broker-master-rrd
    ...    certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Broker Config Output Set
    ...    central
    ...    centreon-broker-master-rrd
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    private_key    ${EtcRoot}/centreon-broker/client.key
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    certificate    ${EtcRoot}/centreon-broker/client.crt
    ${start}    Ctn Get Round Current Date
    Repeat Keyword    5 times    Ctn Start Stop Service    0
    ${content}    Create List    TLS: successful handshake
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No information about TLS activation.

BSCSSTGRR2
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys. Reversed grpc connection with retention.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc    tls
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Output    central    bbdo_server    5670    grpc
    Ctn Config Broker Bbdo Input    rrd    bbdo_client    5670    grpc    localhost
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    encryption    yes
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    retention    yes
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    encryption    yes
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Broker Config Log    central    config    off
    Ctn Broker Config Log    central    core    off
    Ctn Broker Config Log    rrd    core    off
    Ctn Broker Config Log    central    tls    debug
    Ctn Broker Config Log    central    grpc    trace
    Ctn Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/server.key
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/client.key
    ...    ${EtcRoot}/centreon-broker/client.crt

    Ctn Broker Config Output Set
    ...    central
    ...    centreon-broker-master-rrd
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/server.key
    Ctn Broker Config Output Set
    ...    central
    ...    centreon-broker-master-rrd
    ...    certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    private_key    ${EtcRoot}/centreon-broker/client.key
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    certificate    ${EtcRoot}/centreon-broker/client.crt
    Ctn Broker Config Input Set
    ...    rrd
    ...    central-rrd-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    ${content}    Create List    write: buff:    write done: buff:
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No information about TLS activation.
    Ctn Kindly Stop Broker

BSCSSCRR1
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side. Connection reversed with retention.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Broker Bbdo Output    central    bbdo_server    5670    tcp
    Ctn Config Broker Bbdo Input    rrd    bbdo_client    5670    tcp    localhost
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    compression    yes
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    retention    yes
    Ctn Broker Config Log    central    config    off
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    rrd    core    trace
    Ctn Broker Config Flush Log    central    0
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    compression: writing
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No compression enabled
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BSCSSCRR2
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side. Connection reversed with retention.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Output    central    bbdo_server    5670    tcp
    Ctn Config Broker Bbdo Input    rrd    bbdo_client    5670    tcp    localhost
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    compression    no
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    retention    yes
    Ctn Broker Config Log    central    config    off
    Ctn Broker Config Log    central    core    off
    Ctn Broker Config Log    rrd    core    trace
    Ctn Broker Config Log    central    bbdo    trace
    Ctn Broker Config Flush Log    central    0
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    ${content}    Create List    BBDO: we have extensions '' and peer has 'COMPRESSION'
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Compression enabled but should not.
    Ctn Kindly Stop Broker

BSCSSCGRR1
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression    tls
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Output    central    bbdo_server    5670    grpc
    Ctn Config Broker Bbdo Input    rrd    bbdo_client    5670    grpc    localhost
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    compression    yes
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    retention    yes
    Ctn Broker Config Log    central    config    off
    Ctn Broker Config Log    central    core    off
    Ctn Broker Config Log    rrd    core    off
    Ctn Broker Config Log    central    tls    debug
    Ctn Broker Config Log    central    grpc    debug
    Ctn Broker Config Flush Log    central    0
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    ${content}    Create List    server default compression deflate
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No compression enabled
    Ctn Kindly Stop Broker

BSCSSCGRR2
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression    tls
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Output    central    bbdo_server    5670    grpc
    Ctn Config Broker Bbdo Input    rrd    bbdo_client    5670    grpc    localhost
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    compression    no
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    retention    yes
    Ctn Broker Config Log    central    config    off
    Ctn Broker Config Log    central    core    off
    Ctn Broker Config Log    rrd    core    off
    Ctn Broker Config Log    central    tls    debug
    Ctn Broker Config Log    central    grpc    debug
    Ctn Broker Config Flush Log    central    0
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    ${content}    Create List    server default compression deflate
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    15
    Should Be True    not ${result}    No compression enabled
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

Ctn Prepare Suite
    Ctn Clean Before Suite
    Ctn Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/server.key
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/client.key
    ...    ${EtcRoot}/centreon-broker/client.crt

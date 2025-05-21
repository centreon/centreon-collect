*** Settings ***
Documentation       Centreon Broker start/stop tests with bbdo_server and bbdo_client input/output streams. Only these streams are used instead of grpc and tcp.

Resource            ../resources/import.resource

Suite Setup         Ctn Prepare Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
BSCSS1
    [Documentation]    Start-Stop two instances of broker and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    tcp
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    tcp    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    tcp
    Ctn Broker Config Log    central    config    info
    Repeat Keyword    5 times    Ctn Start Stop Service    0

BSCSSP1
    [Documentation]    Start-Stop two instances of broker and no coredump. The server contains a listen address
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    tcp
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    tcp    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    tcp    localhost
    Ctn Broker Config Log    central    config    info
    Repeat Keyword    5 times    Ctn Start Stop Service    0

BSCSS2
    [Documentation]    Start/Stop 10 times broker with 300ms interval and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Ctn Config Broker    central
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    tcp
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    tcp    localhost
    Repeat Keyword    10 times    Ctn Start Stop Instance    300ms

BSCSS3
    [Documentation]    Start-Stop one instance of broker with tcp connection and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Ctn Config Broker    central
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    tcp
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    tcp    localhost
    Repeat Keyword    5 times    Ctn Start Stop Instance    0

BSCSS4
    [Documentation]    Start/Stop 10 times broker with 1sec interval and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Ctn Config Broker    central
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    tcp
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    tcp    localhost
    Repeat Keyword    10 times    Ctn Start Stop Instance    1s

BSCSSG1
    [Documentation]    Start-Stop two instances of broker and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    gRPC    localhost
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    gRPC    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    gRPC
    Ctn Broker Config Log    central    config    info
    Repeat Keyword    5 times    Ctn Start Stop Service    0

BSCSSG2
    [Documentation]    Start/Stop 10 times broker with 300ms interval and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc
    Ctn Config Broker    central
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    gRPC    localhost
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    gRPC    localhost
    Repeat Keyword    10 times    Ctn Start Stop Instance    300ms

BSCSSG3
    [Documentation]    Start-Stop one instance of broker with grpc connection and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc
    Ctn Config Broker    central
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    gRPC
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    gRPC    localhost
    Repeat Keyword    5 times    Ctn Start Stop Instance    0

BSCSSG4
    [Documentation]    Start/Stop 10 times broker with 1sec interval and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc
    Ctn Config Broker    central
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    gRPC
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    gRPC    localhost
    Repeat Keyword    10 times    Ctn Start Stop Instance    1s

BSCSST1
    [Documentation]    Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp    tls
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    tcp
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    tcp    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    tcp
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    encryption    yes
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    encryption    yes
    Ctn Broker Config Log    central    config    off
    Ctn Broker Config Log    central    core    off
    Ctn Broker Config Log    central    tls    debug
    ${start}    Get Current Date
    Repeat Keyword    5 times    Ctn Start Stop Service    0
    ${content}    Create List    TLS: successful handshake
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No information about TLS activation.

BSCSST2
    [Documentation]    Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp    tls
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    tcp
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    tcp    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    tcp
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    encryption    yes
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
    Ctn Broker Config Input Set
    ...    rrd
    ...    central-rrd-master-input
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/client.key
    Ctn Broker Config Input Set
    ...    rrd
    ...    central-rrd-master-input
    ...    certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    ${start}    Get Current Date
    Repeat Keyword    5 times    Ctn Start Stop Service    0
    ${content}    Create List    TLS: successful handshake
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No information about TLS activation.

BSCSSTG1
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. This is not sufficient, then an error is raised.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc    tls
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    gRPC
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    gRPC    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    gRPC
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    encryption    yes
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    encryption    yes
    Ctn Broker Config Log    central    config    off
    Ctn Broker Config Log    central    core    off
    Ctn Broker Config Log    rrd    core    off
    Ctn Broker Config Log    central    tls    debug
    Ctn Broker Config Log    central    grpc    debug
    Ctn Broker Config Log    rrd    grpc    debug
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Source Log    central    1
    Ctn Broker Config Flush Log    rrd    0
    Ctn Broker Config Source Log    rrd    1
    
    Ctn Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/server.key
    ...    ${EtcRoot}/centreon-broker/server.crt

    Ctn Broker Config Input Set
    ...    rrd
    ...    central-rrd-master-input
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/server.key
    Ctn Broker Config Input Set
    ...    rrd
    ...    central-rrd-master-input
    ...    certificate
    ...    ${EtcRoot}/centreon-broker/server.crt

    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List    Handshake failed
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No information about TLS activation.

BSCSSTG2
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc    tls
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    grpc
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    grpc    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    grpc
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    encryption    yes
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    encryption    yes
    Ctn Broker Config Log    central    config    off
    Ctn Broker Config Log    central    core    off
    Ctn Broker Config Log    rrd    core    off
    Ctn Broker Config Log    central    tls    debug
    Ctn Broker Config Log    rrd    grpc    trace
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
    Ctn Broker Config Output Set
    ...    central
    ...    centreon-broker-master-rrd
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Ctn Broker Config Input Set
    ...    rrd
    ...    central-rrd-master-input
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/client.key
    Ctn Broker Config Input Set
    ...    rrd
    ...    central-rrd-master-input
    ...    certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List    encrypted connection    write: buff:    write done: buff:
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No information about TLS activation.
    Ctn Kindly Stop Broker

BSCSSTG3
    [Documentation]    Start-Stop two instances of broker. The connection cannot be established if the server private key is missing and an error message explains this issue.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc    tls
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    grpc
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    grpc    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    grpc
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    encryption    yes
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    encryption    yes
    Ctn Broker Config Log    central    config    off
    Ctn Broker Config Log    central    core    off
    Ctn Broker Config Log    rrd    core    off
    Ctn Broker Config Log    central    tls    debug
    Ctn Broker Config Log    central    grpc    debug
    Ctn Broker Config Log    rrd    grpc    debug
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
    Ctn Broker Config Output Set
    ...    central
    ...    centreon-broker-master-rrd
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Ctn Broker Config Input Set
    ...    rrd
    ...    central-rrd-master-input
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/missing-client.key
    Ctn Broker Config Input Set
    ...    rrd
    ...    central-rrd-master-input
    ...    certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List
    ...    Cannot open file '/tmp/etc/centreon-broker/missing-client.key': No such file or directory
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    30
    Should Be True    ${result}    No information about the missing private key on server.

BSCSSC1
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    tcp
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    tcp    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    tcp
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    compression    yes
    Ctn Broker Config Log    central    config    off
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    rrd    core    trace
    Ctn Broker Config Flush Log    central    0
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    compression: writing
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No compression enabled
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BSCSSC2
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    tcp
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    tcp    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    tcp
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    compression    no
    Ctn Broker Config Log    central    config    off
    Ctn Broker Config Log    central    core    off
    Ctn Broker Config Log    rrd    core    trace
    Ctn Broker Config Log    central    bbdo    trace
    Ctn Broker Config Flush Log    central    0
    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List    BBDO: we have extensions '' and peer has 'COMPRESSION'
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Compression enabled but should not.
    Ctn Kindly Stop Broker

BSCSSCG1
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on client side.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression    tls
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    grpc
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    grpc    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    grpc
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    compression    yes
    Ctn Broker Config Log    central    config    off
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    rrd    core    off
    Ctn Broker Config Log    central    tls    debug
    Ctn Broker Config Log    rrd    grpc    debug
    Ctn Broker Config Log    central    grpc    debug
    Ctn Broker Config Flush Log    central    0
    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List    activate compression deflate
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No compression enabled
    Ctn Kindly Stop Broker

BSCSSGA1
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server. Error messages are raised.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression    tls
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    grpc
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    grpc    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    grpc
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    authorization    titus
    Ctn Broker Config Log    central    config    off
    Ctn Broker Config Log    central    core    off
    Ctn Broker Config Log    rrd    core    off
    Ctn Broker Config Log    rrd    tls    debug
    Ctn Broker Config Log    rrd    grpc    trace
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List    Wrong client authorization token
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    30
    Should Be True    ${result}    An error message about the authorization token should be raised.
    Ctn Kindly Stop Broker

BSCSSGA2
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server and also on the client. All looks ok.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression    tls
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    grpc
    Ctn Config Broker Bbdo Output    central    bbdo_client    5670    grpc    localhost
    Ctn Config Broker Bbdo Input    rrd    bbdo_server    5670    grpc
    Ctn Broker Config Input Set    rrd    central-rrd-master-input    authorization    titus
    Ctn Broker Config Output Set    central    centreon-broker-master-rrd    authorization    titus
    Ctn Broker Config Log    central    config    trace
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    rrd    core    off
    Ctn Broker Config Log    rrd    tls    debug
    Ctn Broker Config Log    rrd    grpc    trace
    Ctn Broker Config Log    central    grpc    trace
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Broker Config Source Log    rrd    1
    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List    receive: buff
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    30
    Should Be True    ${result}    If the authorization token is the same on both side, no issue
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
    ${result}    Wait For Process    b1    timeout=60s    on_timeout=kill
    Should Be True    ${result.rc} == -15 or ${result.rc} == 0    Broker service badly stopped
    Send Signal To Process    SIGTERM    b2
    ${result}    Wait For Process    b2    timeout=60s    on_timeout=kill
    Should Be True    ${result.rc} == -15 or ${result.rc} == 0    Broker service badly stopped

Ctn Start Stop Instance
    [Arguments]    ${interval}
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Sleep    ${interval}
    Send Signal To Process    SIGTERM    b1
    ${result}    Wait For Process    b1    timeout=60s    on_timeout=kill
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

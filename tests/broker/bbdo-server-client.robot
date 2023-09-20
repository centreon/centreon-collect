*** Settings ***
Documentation       Centreon Broker start/stop tests with bbdo_server and bbdo_client input/output streams. Only these streams are used instead of grpc and tcp.

Resource            ../resources/resources.robot
Library             DateTime
Library             Process
Library             OperatingSystem
Library             ../resources/Broker.py

Suite Setup         Prepare Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes


*** Test Cases ***
BSCSS1
    [Documentation]    Start-Stop two instances of broker and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    tcp
    Config Broker BBDO Output    central    bbdo_client    5670    tcp    localhost
    Config Broker BBDO Input    rrd    bbdo_server    5670    tcp
    Broker Config Log    central    config    info
    Repeat Keyword    5 times    Start Stop Service    0

BSCSSP1
    [Documentation]    Start-Stop two instances of broker and no coredump. The server contains a listen address
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    tcp
    Config Broker BBDO Output    central    bbdo_client    5670    tcp    localhost
    Config Broker BBDO Input    rrd    bbdo_server    5670    tcp    localhost
    Broker Config Log    central    config    info
    Repeat Keyword    5 times    Start Stop Service    0

BSCSS2
    [Documentation]    Start/Stop 10 times broker with 300ms interval and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Config Broker    central
    Config Broker BBDO Input    central    bbdo_server    5669    tcp
    Config Broker BBDO Output    central    bbdo_client    5670    tcp    localhost
    Repeat Keyword    10 times    Start Stop Instance    300ms

BSCSS3
    [Documentation]    Start-Stop one instance of broker and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Config Broker    central
    Config Broker BBDO Input    central    bbdo_server    5669    tcp
    Config Broker BBDO Output    central    bbdo_client    5670    tcp    localhost
    Repeat Keyword    5 times    Start Stop Instance    0

BSCSS4
    [Documentation]    Start/Stop 10 times broker with 1sec interval and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp
    Config Broker    central
    Config Broker BBDO Input    central    bbdo_server    5669    tcp
    Config Broker BBDO Output    central    bbdo_client    5670    tcp    localhost
    Repeat Keyword    10 times    Start Stop Instance    1s

BSCSSG1
    [Documentation]    Start-Stop two instances of broker and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    gRPC    localhost
    Config Broker BBDO Output    central    bbdo_client    5670    gRPC    localhost
    Config Broker BBDO Input    rrd    bbdo_server    5670    gRPC
    Broker Config Log    central    config    info
    Repeat Keyword    5 times    Start Stop Service    0

BSCSSG2
    [Documentation]    Start/Stop 10 times broker with 300ms interval and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc
    Config Broker    central
    Config Broker BBDO Input    central    bbdo_server    5669    gRPC    localhost
    Config Broker BBDO Output    central    bbdo_client    5670    gRPC    localhost
    Repeat Keyword    10 times    Start Stop Instance    300ms

BSCSSG3
    [Documentation]    Start-Stop one instance of broker and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc
    Config Broker    central
    Config Broker BBDO Input    central    bbdo_server    5669    gRPC
    Config Broker BBDO Output    central    bbdo_client    5670    gRPC    localhost
    Repeat Keyword    5 times    Start Stop Instance    0

BSCSSG4
    [Documentation]    Start/Stop 10 times broker with 1sec interval and no coredump
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc
    Config Broker    central
    Config Broker BBDO Input    central    bbdo_server    5669    gRPC
    Config Broker BBDO Output    central    bbdo_client    5670    gRPC    localhost
    Repeat Keyword    10 times    Start Stop Instance    1s

BSCSST1
    [Documentation]    Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp    tls
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    tcp
    Config Broker BBDO Output    central    bbdo_client    5670    tcp    localhost
    Config Broker BBDO Input    rrd    bbdo_server    5670    tcp
    Broker Config Output set    central    central-broker-master-output    encryption    yes
    Broker Config Input set    rrd    rrd-broker-master-input    encryption    yes
    Broker Config Log    central    config    off
    Broker Config Log    central    core    off
    Broker Config Log    central    tls    debug
    ${start}=    Get Current Date
    Repeat Keyword    5 times    Start Stop Service    0
    ${content}=    Create List    TLS: successful handshake
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No information about TLS activation.

BSCSST2
    [Documentation]    Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    tcp    tls
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    tcp
    Config Broker BBDO Output    central    bbdo_client    5670    tcp    localhost
    Config Broker BBDO Input    rrd    bbdo_server    5670    tcp
    Broker Config Output set    central    central-broker-master-output    encryption    yes
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
    ${start}=    Get Current Date
    Repeat Keyword    5 times    Start Stop Service    0
    ${content}=    Create List    TLS: successful handshake
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No information about TLS activation.

BSCSSTG1
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. This is not sufficient, then an error is raised.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc    tls
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    gRPC
    Config Broker BBDO Output    central    bbdo_client    5670    gRPC    localhost
    Config Broker BBDO Input    rrd    bbdo_server    5670    gRPC
    Broker Config Output set    central    central-broker-master-output    encryption    yes
    Broker Config Input set    rrd    rrd-broker-master-input    encryption    yes
    Broker Config Log    central    config    off
    Broker Config Log    central    core    off
    Broker Config Log    rrd    core    off
    Broker Config Log    central    tls    debug
    Broker Config Log    central    grpc    debug
    Broker Config Log    rrd    grpc    debug
    Broker Config Flush Log    central    0
    Broker Config Source Log    central    1
    Broker Config Flush Log    rrd    0
    Broker Config Source Log    rrd    1
    ${start}=    Get Current Date
    Start Broker
    ${content}=    Create List    Handshake failed
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No information about TLS activation.

BSCSSTG2
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc    tls
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    grpc
    Config Broker BBDO Output    central    bbdo_client    5670    grpc    localhost
    Config Broker BBDO Input    rrd    bbdo_server    5670    grpc
    Broker Config Output set    central    central-broker-master-output    encryption    yes
    Broker Config Input set    rrd    rrd-broker-master-input    encryption    yes
    Broker Config Log    central    config    off
    Broker Config Log    central    core    off
    Broker Config Log    rrd    core    off
    Broker Config Log    central    tls    debug
    Broker Config Log    rrd    grpc    trace
    Broker Config Log    central    grpc    trace
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
    Broker Config Output set
    ...    central
    ...    central-broker-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Broker Config Input set    rrd    rrd-broker-master-input    private_key    ${EtcRoot}/centreon-broker/client.key
    Broker Config Input set    rrd    rrd-broker-master-input    certificate    ${EtcRoot}/centreon-broker/client.crt
    ${start}=    Get Current Date
    Start Broker
    ${content}=    Create List    encrypted connection    write: buff:    write done: buff:
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No information about TLS activation.
    Kindly Stop Broker

BSCSSTG3
    [Documentation]    Start-Stop two instances of broker. The connection cannot be established if the server private key is missing and an error message explains this issue.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    grpc    tls
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    grpc
    Config Broker BBDO Output    central    bbdo_client    5670    grpc    localhost
    Config Broker BBDO Input    rrd    bbdo_server    5670    grpc
    Broker Config Output set    central    central-broker-master-output    encryption    yes
    Broker Config Input set    rrd    rrd-broker-master-input    encryption    yes
    Broker Config Log    central    config    off
    Broker Config Log    central    core    off
    Broker Config Log    rrd    core    off
    Broker Config Log    central    tls    debug
    Broker Config Log    central    grpc    debug
    Broker Config Log    rrd    grpc    debug
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
    Broker Config Output set
    ...    central
    ...    central-broker-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Broker Config Input set
    ...    rrd
    ...    rrd-broker-master-input
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/missing-client.key
    Broker Config Input set    rrd    rrd-broker-master-input    certificate    ${EtcRoot}/centreon-broker/client.crt
    ${start}=    Get Current Date
    Start Broker
    ${content}=    Create List
    ...    Cannot open file '/tmp/etc/centreon-broker/missing-client.key': No such file or directory
    ${result}=    Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No information about the missing private key on server.

BSCSSC1
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    tcp
    Config Broker BBDO Output    central    bbdo_client    5670    tcp    localhost
    Config Broker BBDO Input    rrd    bbdo_server    5670    tcp
    Broker Config Output set    central    central-broker-master-output    compression    yes
    Broker Config Log    central    config    off
    Broker Config Log    central    core    trace
    Broker Config Log    rrd    core    trace
    Broker Config Flush Log    central    0
    ${start}=    Get Current Date
    Start Broker
    ${content}=    Create List    compression: writing
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No compression enabled
    Kindly Stop Broker

BSCSSC2
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    tcp
    Config Broker BBDO Output    central    bbdo_client    5670    tcp    localhost
    Config Broker BBDO Input    rrd    bbdo_server    5670    tcp
    Broker Config Output set    central    central-broker-master-output    compression    no
    Broker Config Log    central    config    off
    Broker Config Log    central    core    off
    Broker Config Log    rrd    core    trace
    Broker Config Log    central    bbdo    trace
    Broker Config Flush Log    central    0
    ${start}=    Get Current Date
    Start Broker
    ${content}=    Create List    BBDO: we have extensions '' and peer has 'COMPRESSION'
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=Compression enabled but should not.
    Kindly Stop Broker

BSCSSCG1
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on client side.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression    tls
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    grpc
    Config Broker BBDO Output    central    bbdo_client    5670    grpc    localhost
    Config Broker BBDO Input    rrd    bbdo_server    5670    grpc
    Broker Config Output set    central    central-broker-master-output    compression    yes
    Broker Config Log    central    config    off
    Broker Config Log    central    core    trace
    Broker Config Log    rrd    core    off
    Broker Config Log    central    tls    debug
    Broker Config Log    rrd    grpc    debug
    Broker Config Log    central    grpc    debug
    Broker Config Flush Log    central    0
    ${start}=    Get Current Date
    Start Broker
    ${content}=    Create List    activate compression deflate
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No compression enabled
    Kindly Stop Broker

BSCSSGA1
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server. Error messages are raised.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression    tls
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    grpc
    Config Broker BBDO Output    central    bbdo_client    5670    grpc    localhost
    Config Broker BBDO Input    rrd    bbdo_server    5670    grpc
    Broker Config input set    rrd    rrd-broker-master-input    authorization    titus
    Broker Config Log    central    config    off
    Broker Config Log    central    core    off
    Broker Config Log    rrd    core    off
    Broker Config Log    rrd    tls    debug
    Broker Config Log    rrd    grpc    trace
    Broker Config Flush Log    central    0
    Broker Config Flush Log    rrd    0
    ${start}=    Get Current Date
    Start Broker
    ${content}=    Create List    Wrong client authorization token
    ${result}=    Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=An error message about the authorization token should be raised.
    Kindly Stop Broker

BSCSSGA2
    [Documentation]    Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server and also on the client. All looks ok.
    [Tags]    broker    start-stop    bbdo_server    bbdo_client    compression    tls
    Config Broker    central
    Config Broker    rrd
    Config Broker BBDO Input    central    bbdo_server    5669    grpc
    Config Broker BBDO Output    central    bbdo_client    5670    grpc    localhost
    Config Broker BBDO Input    rrd    bbdo_server    5670    grpc
    Broker Config input set    rrd    rrd-broker-master-input    authorization    titus
    Broker Config output set    central    central-broker-master-output    authorization    titus
    Broker Config Log    central    config    trace
    Broker Config Log    central    core    trace
    Broker Config Log    rrd    core    off
    Broker Config Log    rrd    tls    debug
    Broker Config Log    rrd    grpc    debug
    Broker Config Log    central    grpc    debug
    Broker Config Flush Log    central    0
    Broker Config Flush Log    rrd    0
    Broker Config Source Log    rrd    1
    ${start}=    Get Current Date
    Start Broker
    ${content}=    Create List    receive: buff
    ${result}=    Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=If the authorization token is the same on both side, no issue
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
    ${result}=    Wait For Process    b1    timeout=60s    on_timeout=kill
    Should Be True    ${result.rc} == -15 or ${result.rc} == 0    msg=Broker service badly stopped
    Send Signal To Process    SIGTERM    b2
    ${result}=    Wait For Process    b2    timeout=60s    on_timeout=kill
    Should Be True    ${result.rc} == -15 or ${result.rc} == 0    msg=Broker service badly stopped

Start Stop Instance
    [Arguments]    ${interval}
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    Sleep    ${interval}
    Send Signal To Process    SIGTERM    b1
    ${result}=    Wait For Process    b1    timeout=60s    on_timeout=kill
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

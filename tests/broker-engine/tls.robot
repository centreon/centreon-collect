*** Settings ***
Documentation       Centreon Broker and Engine communication with or without TLS

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Variables ***
&{ext}                  yes=TLS    no=    auto=TLS
@{choices}              yes    no    auto
@{LIST_HANDSHAKE}       performing handshake    successful handshake
@{TLS_REFUSED}
...                     BBDO: connection refused: extension 'TLS' is set to 'yes' in the configuration but peer


*** Test Cases ***
BECT1
    [Documentation]    Broker/Engine communication with anonymous TLS between central and poller
    [Tags]    broker    engine    tls    tcp
    ${test_direct_grpc}    Ctn Is Using Direct Grpc
    IF    ${test_direct_grpc}
        Pass Execution    Test passes, skipping on direct grpc tests: TLS is not a BBDO extension there
    END
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    FOR    ${comp1}    IN    @{choices}
        FOR    ${comp2}    IN    @{choices}
            Log To Console    TLS set to ${comp1} on central and to ${comp2} on module
            Ctn Config Broker    central
            Ctn Config Broker    module
            Ctn Broker Config Input Set    central    central-broker-master-input    tls    ${comp1}
            Ctn Broker Config Output Set    module0    central-module-master-output    tls    ${comp2}
            Ctn Broker Config Log    central    bbdo    info
            Ctn Broker Config Log    module0    bbdo    info
            ${start}    Get Current Date
            Ctn Start Broker
            Ctn Start Engine
            Ctn Check Tls Negotiation    ${comp1}    ${comp2}    ${start}
            Ctn Kindly Stop Broker
            Ctn Stop Engine
        END
    END

BECT2
    [Documentation]    Broker/Engine communication with TLS between central and poller with key/cert
    [Tags]    broker    engine    tls    tcp
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    ${hostname}    Ctn Get Hostname
    Ctn Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/server.key
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/client.key
    ...    ${EtcRoot}/centreon-broker/client.crt

    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/client.key
    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/client.crt
    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/server.key
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Ctn Broker Config Log    central    tls    debug
    Ctn Broker Config Log    module0    tls    debug
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    module0    bbdo    info
    Ctn Broker Config Input Set    central    central-broker-master-input    tls    yes
    Ctn Broker Config Output Set    module0    central-module-master-output    tls    yes
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    Ctn Kindly Stop Broker
    Ctn Stop Engine
    ${content1}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content2}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content1}    Combine Lists    ${content1}    ${LIST_HANDSHAKE}
    ${content2}    Combine Lists    ${content2}    ${LIST_HANDSHAKE}
    ${result}    Ctn Find In Log    ${centralLog}    ${start}    ${content1}
    Should Be True    ${result}
    ${result}    Ctn Find In Log    ${moduleLog0}    ${start}    ${content2}
    Should Be True    ${result}

BECT3
    [Documentation]    Broker/Engine communication with anonymous TLS and ca certificate
    [Tags]    broker    engine    tls    tcp
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    ${hostname}    Ctn Get Hostname
    Ctn Create Certificate    ${hostname}    ${EtcRoot}/centreon-broker/server.crt
    Ctn Create Certificate    ${hostname}    ${EtcRoot}/centreon-broker/client.crt

    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Ctn Broker Config Log    central    tls    debug
    Ctn Broker Config Log    module0    tls    debug
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    module0    bbdo    info
    Ctn Broker Config Input Set    central    central-broker-master-input    tls    yes
    Ctn Broker Config Output Set    module0    central-module-master-output    tls    yes
    # We get the current date just before starting broker
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    Ctn Kindly Stop Broker
    Ctn Stop Engine
    ${content1}    Create List    we have extensions 'TLS' and peer has 'TLS'    using anonymous server credentials
    ${content2}    Create List    we have extensions 'TLS' and peer has 'TLS'    using anonymous client credentials
    ${content1}    Combine Lists    ${content1}    ${LIST_HANDSHAKE}
    ${content2}    Combine Lists    ${content2}    ${LIST_HANDSHAKE}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}    Ctn Find In Log    ${log}    ${start}    ${content1}
    Should Be True    ${result}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-module-master0.log
    ${result}    Ctn Find In Log    ${log}    ${start}    ${content2}
    Should Be True    ${result}

BECT4
    [Documentation]    Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
    [Tags]    broker    engine    tls    tcp
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    Set Local Variable    ${hostname}    centreon
    Ctn Create Key And Certificate
    ...    ${hostname}
    ...    ${EtcRoot}/centreon-broker/server.key
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Create Key And Certificate
    ...    ${hostname}
    ...    ${EtcRoot}/centreon-broker/client.key
    ...    ${EtcRoot}/centreon-broker/client.crt

    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Ctn Broker Config Log    central    tls    debug
    Ctn Broker Config Log    module0    tls    debug
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    module0    bbdo    info
    Ctn Broker Config Input Set    central    central-broker-master-input    tls    yes
    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/client.key
    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/client.crt
    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Broker Config Input Set    central    central-broker-master-input    tls_hostname    centreon
    Ctn Broker Config Output Set    module0    central-module-master-output    tls    yes
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/server.key
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    # We get the current date just before starting broker
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    Ctn Kindly Stop Broker
    Ctn Stop Engine
    ${content1}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content2}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content1}    Combine Lists    ${content1}    ${LIST_HANDSHAKE}
    ${content2}    Combine Lists    ${content2}    ${LIST_HANDSHAKE}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}    Ctn Find In Log    ${log}    ${start}    ${content1}
    Should Be True    ${result}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-module-master0.log
    ${result}    Ctn Find In Log    ${log}    ${start}    ${content2}
    Should Be True    ${result}

BECT_GRPC1
    [Documentation]    Broker/Engine communication with GRPC and with anonymous TLS between central and poller
    [Tags]    broker    engine    tls    tcp
    ${test_direct_grpc}    Ctn Is Using Direct Grpc
    IF    ${test_direct_grpc}
        Pass Execution    Test passes, skipping on direct grpc tests: TLS is not a BBDO extension there
    END
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    FOR    ${comp1}    IN    @{choices}
        FOR    ${comp2}    IN    @{choices}
            Log To Console    TLS set to ${comp1} on central and to ${comp2} on module
            Ctn Config Broker    central
            Ctn Config Broker    module
            Ctn Broker Config Input Set    central    central-broker-master-input    tls    ${comp1}
            Ctn Broker Config Output Set    module0    central-module-master-output    tls    ${comp2}
            Ctn Broker Config Log    central    bbdo    info
            Ctn Broker Config Log    module0    bbdo    info
            Ctn Broker Config Log    central    grpc    debug
            Ctn Broker Config Log    module0    grpc    debug
            Ctn Change Broker Tcp Output To Grpc    module0
            Ctn Change Broker Tcp Input To Grpc    central
            ${start}    Get Current Date
            Ctn Start Broker
            Ctn Start Engine
            Ctn Check Tls Negotiation    ${comp1}    ${comp2}    ${start}
            Ctn Kindly Stop Broker
            Ctn Stop Engine
        END
    END

BECT_GRPC2
    [Documentation]    Broker/Engine communication with TLS between central and poller with key/cert
    [Tags]    broker    engine    tls    tcp
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    Ctn Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/server.key
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/client.key
    ...    ${EtcRoot}/centreon-broker/client.crt

    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/client.key
    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/client.crt
    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/server.key
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Ctn Broker Config Log    central    tls    debug
    Ctn Broker Config Log    module0    tls    debug
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    module0    bbdo    info
    Ctn Broker Config Input Set    central    central-broker-master-input    tls    yes
    Ctn Broker Config Output Set    module0    central-module-master-output    tls    yes
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    Ctn Kindly Stop Broker
    Ctn Stop Engine
    ${content1}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content2}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content1}    Combine Lists    ${content1}    ${LIST_HANDSHAKE}
    ${content2}    Combine Lists    ${content2}    ${LIST_HANDSHAKE}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}    Ctn Find In Log    ${log}    ${start}    ${content1}
    Should Be True    ${result}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-module-master0.log
    ${result}    Ctn Find In Log    ${log}    ${start}    ${content2}
    Should Be True    ${result}

BECT_GRPC3
    [Documentation]    Broker/Engine communication with anonymous TLS and ca certificate
    [Tags]    broker    engine    tls    tcp
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    ${hostname}    Ctn Get Hostname
    Ctn Create Certificate    ${hostname}    ${EtcRoot}/centreon-broker/server.crt
    Ctn Create Certificate    ${hostname}    ${EtcRoot}/centreon-broker/client.crt

    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Ctn Broker Config Log    central    tls    debug
    Ctn Broker Config Log    module0    tls    debug
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    module0    bbdo    info
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Broker Config Input Set    central    central-broker-master-input    tls    yes
    Ctn Broker Config Output Set    module0    central-module-master-output    tls    yes
    # We get the current date just before starting broker
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    Ctn Kindly Stop Broker
    Ctn Stop Engine
    ${content1}    Create List    we have extensions 'TLS' and peer has 'TLS'    using anonymous server credentials
    ${content2}    Create List    we have extensions 'TLS' and peer has 'TLS'    using anonymous client credentials
    ${content1}    Combine Lists    ${content1}    ${LIST_HANDSHAKE}
    ${content2}    Combine Lists    ${content2}    ${LIST_HANDSHAKE}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}    Ctn Find In Log    ${log}    ${start}    ${content1}
    Should Be True    ${result}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-module-master0.log
    ${result}    Ctn Find In Log    ${log}    ${start}    ${content2}
    Should Be True    ${result}

BECT_GRPC4
    [Documentation]    Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
    [Tags]    broker    engine    tls    tcp
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    Set Local Variable    ${hostname}    centreon
    Ctn Create Key And Certificate
    ...    ${hostname}
    ...    ${EtcRoot}/centreon-broker/server.key
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Create Key And Certificate
    ...    ${hostname}
    ...    ${EtcRoot}/centreon-broker/client.key
    ...    ${EtcRoot}/centreon-broker/client.crt

    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Ctn Broker Config Log    central    tls    debug
    Ctn Broker Config Log    module0    tls    debug
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    module0    bbdo    info
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Broker Config Input Set    central    central-broker-master-input    tls    yes
    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/client.key
    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/client.crt
    Ctn Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Broker Config Input Set    central    central-broker-master-input    tls_hostname    centreon
    Ctn Broker Config Output Set    module0    central-module-master-output    tls    yes
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/server.key
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/server.crt
    Ctn Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    # We get the current date just before starting broker
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    Ctn Kindly Stop Broker
    Ctn Stop Engine
    ${content1}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content2}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content1}    Combine Lists    ${content1}    ${LIST_HANDSHAKE}
    ${content2}    Combine Lists    ${content2}    ${LIST_HANDSHAKE}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}    Ctn Find In Log    ${log}    ${start}    ${content1}
    Should Be True    ${result}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-module-master0.log
    ${result}    Ctn Find In Log    ${log}    ${start}    ${content2}
    Should Be True    ${result}


*** Keywords ***
Ctn Check Tls Negotiation
    [Documentation]    Check the BBDO negotiation between the central input with TLS set to
    ...    ${central_tls} and the poller output with TLS set to ${module_tls}. When TLS is
    ...    set to 'yes' on one side only, this side refuses the connection.
    [Arguments]    ${central_tls}    ${module_tls}    ${start}
    IF    "${central_tls}" == "yes" and "${module_tls}" == "no"
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${TLS_REFUSED}    30
        Should Be True    ${result}    The central should refuse a poller that does not activate TLS
    ELSE IF    "${central_tls}" == "no" and "${module_tls}" == "yes"
        ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${TLS_REFUSED}    30
        Should Be True    ${result}    The poller should refuse a central that does not activate TLS
    ELSE
        ${result}    Ctn Check Connections
        Should Be True    ${result}    Engine and Broker not connected
    END
    ${content1}    Create List    we have extensions '${ext["${central_tls}"]}' and peer has '${ext["${module_tls}"]}'
    ${content2}    Create List    we have extensions '${ext["${module_tls}"]}' and peer has '${ext["${central_tls}"]}'
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content1}    30
    Should Be True    ${result}    Unexpected extensions in the central log
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content2}    30
    Should Be True    ${result}    Unexpected extensions in the poller log

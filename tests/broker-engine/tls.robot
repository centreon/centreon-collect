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


*** Test Cases ***
BECT1
    [Documentation]    Broker/Engine communication with anonymous TLS between central and poller
    [Tags]    broker    engine    tls    tcp
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
            ${result}    Ctn Check Connections
            Should Be True    ${result}    Engine and Broker not connected
            Ctn Kindly Stop Broker
            Ctn Stop Engine
            ${content1}    Create List    we have extensions '${ext["${comp1}"]}' and peer has '${ext["${comp2}"]}'
            ${content2}    Create List    we have extensions '${ext["${comp2}"]}' and peer has '${ext["${comp1}"]}'
            IF    "${comp1}" == "yes" and "${comp2}" == "no"
                Insert Into List
                ...    ${content1}
                ...    ${-1}
                ...    extension 'TLS' is set to 'yes' in the configuration but cannot be activated because of peer configuration
            ELSE IF    "${comp1}" == "no" and "${comp2}" == "yes"
                Insert Into List
                ...    ${content2}
                ...    ${-1}
                ...    extension 'TLS' is set to 'yes' in the configuration but cannot be activated because of peer configuration
            END
            ${result}    Ctn Find In Log    ${centralLog}    ${start}    ${content1}
            Should Be True    ${result}
            ${result}    Ctn Find In Log    ${moduleLog0}    ${start}    ${content2}
            Should Be True    ${result}
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
            ${result}    Ctn Check Connections
            Should Be True    ${result}    Engine and Broker not connected
            Ctn Kindly Stop Broker
            Ctn Stop Engine
            ${content1}    Create List    we have extensions '${ext["${comp1}"]}' and peer has '${ext["${comp2}"]}'
            ${content2}    Create List    we have extensions '${ext["${comp2}"]}' and peer has '${ext["${comp1}"]}'
            IF    "${comp1}" == "yes" and "${comp2}" == "no"
                Insert Into List
                ...    ${content1}
                ...    ${-1}
                ...    extension 'TLS' is set to 'yes' in the configuration but cannot be activated because of peer configuration
            ELSE IF    "${comp1}" == "no" and "${comp2}" == "yes"
                Insert Into List
                ...    ${content2}
                ...    ${-1}
                ...    extension 'TLS' is set to 'yes' in the configuration but cannot be activated because of peer configuration
            END
            ${result}    Ctn Find In Log    ${centralLog}    ${start}    ${content1}
            Should Be True    ${result}
            ${result}    Ctn Find In Log    ${moduleLog0}    ${start}    ${content2}
            Should Be True    ${result}
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

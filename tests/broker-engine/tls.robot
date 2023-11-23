*** Settings ***
Documentation       Centreon Broker and Engine communication with or without TLS

Resource            ../resources/resources.robot
Library             Process
Library             OperatingSystem
Library             DateTime
Library             Collections
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Variables ***
&{ext}                  yes=TLS    no=    auto=TLS
@{choices}              yes    no    auto
@{LIST_HANDSHAKE}       performing handshake    successful handshake


*** Test Cases ***
BECT1
    [Documentation]    Broker/Engine communication with anonymous TLS between central and poller
    [Tags]    broker    engine    tls    tcp
    Config Engine    ${1}
    Config Broker    rrd
    FOR    ${comp1}    IN    @{choices}
        FOR    ${comp2}    IN    @{choices}
            Log To Console    TLS set to ${comp1} on central and to ${comp2} on module
            Config Broker    central
            Config Broker    module
            Broker Config Input Set    central    central-broker-master-input    tls    ${comp1}
            Broker Config Output Set    module0    central-module-master-output    tls    ${comp2}
            Broker Config Log    central    bbdo    info
            Broker Config Log    module0    bbdo    info
            ${start}    Get Current Date
            Start Broker
            Start Engine
            ${result}    Check Connections
            Should Be True    ${result}    Engine and Broker not connected
            Kindly Stop Broker
            Stop Engine
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
            ${result}    Find In Log    ${centralLog}    ${start}    ${content1}
            Should Be True    ${result}
            ${result}    Find In Log    ${engineLog0}    ${start}    ${content2}
            Should Be True    ${result}
        END
    END

BECT2
    [Documentation]    Broker/Engine communication with TLS between central and poller with key/cert
    [Tags]    broker    engine    tls    tcp
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module

    ${hostname}    Get Hostname
    Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/server.key
    ...    ${EtcRoot}/centreon-broker/server.crt
    Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/client.key
    ...    ${EtcRoot}/centreon-broker/client.crt

    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/client.key
    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/client.crt
    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/server.key
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/server.crt
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Broker Config Log    central    tls    debug
    Broker Config Log    module0    tls    debug
    Broker Config Log    central    bbdo    info
    Broker Config Log    module0    bbdo    info
    Broker Config Input Set    central    central-broker-master-input    tls    yes
    Broker Config Output Set    module0    central-module-master-output    tls    yes
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    Kindly Stop Broker
    Stop Engine
    ${content1}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content2}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content1}    Combine Lists    ${content1}    ${LIST_HANDSHAKE}
    ${content2}    Combine Lists    ${content2}    ${LIST_HANDSHAKE}
    ${result}    Find In Log    ${centralLog}    ${start}    ${content1}
    Should Be True    ${result}
    ${result}    Find In Log    ${engineLog0}    ${start}    ${content2}
    Should Be True    ${result}

BECT3
    [Documentation]    Broker/Engine communication with anonymous TLS and ca certificate
    [Tags]    broker    engine    tls    tcp
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module

    ${hostname}    Get Hostname
    Create Certificate    ${hostname}    ${EtcRoot}/centreon-broker/server.crt
    Create Certificate    ${hostname}    ${EtcRoot}/centreon-broker/client.crt

    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Broker Config Log    central    tls    debug
    Broker Config Log    module0    tls    debug
    Broker Config Log    central    bbdo    info
    Broker Config Log    module0    bbdo    info
    Broker Config Input Set    central    central-broker-master-input    tls    yes
    Broker Config Output Set    module0    central-module-master-output    tls    yes
    # We get the current date just before starting broker
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    Kindly Stop Broker
    Stop Engine
    ${content1}    Create List    we have extensions 'TLS' and peer has 'TLS'    using anonymous server credentials
    ${content2}    Create List    we have extensions 'TLS' and peer has 'TLS'    using anonymous client credentials
    ${content1}    Combine Lists    ${content1}    ${LIST_HANDSHAKE}
    ${content2}    Combine Lists    ${content2}    ${LIST_HANDSHAKE}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}    Find In Log    ${log}    ${start}    ${content1}
    Should Be True    ${result}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-module-master0.log
    ${result}    Find In Log    ${log}    ${start}    ${content2}
    Should Be True    ${result}

BECT4
    [Documentation]    Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
    [Tags]    broker    engine    tls    tcp
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module

    Set Local Variable    ${hostname}    centreon
    Create Key And Certificate
    ...    ${hostname}
    ...    ${EtcRoot}/centreon-broker/server.key
    ...    ${EtcRoot}/centreon-broker/server.crt
    Create Key And Certificate
    ...    ${hostname}
    ...    ${EtcRoot}/centreon-broker/client.key
    ...    ${EtcRoot}/centreon-broker/client.crt

    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Broker Config Log    central    tls    debug
    Broker Config Log    module0    tls    debug
    Broker Config Log    central    bbdo    info
    Broker Config Log    module0    bbdo    info
    Broker Config Input Set    central    central-broker-master-input    tls    yes
    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/client.key
    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/client.crt
    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Broker Config Input Set    central    central-broker-master-input    tls_hostname    centreon
    Broker Config Output Set    module0    central-module-master-output    tls    yes
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/server.key
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/server.crt
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    # We get the current date just before starting broker
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    Kindly Stop Broker
    Stop Engine
    ${content1}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content2}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content1}    Combine Lists    ${content1}    ${LIST_HANDSHAKE}
    ${content2}    Combine Lists    ${content2}    ${LIST_HANDSHAKE}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}    Find In Log    ${log}    ${start}    ${content1}
    Should Be True    ${result}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-module-master0.log
    ${result}    Find In Log    ${log}    ${start}    ${content2}
    Should Be True    ${result}

BECT_GRPC1
    [Documentation]    Broker/Engine communication with GRPC and with anonymous TLS between central and poller
    [Tags]    broker    engine    tls    tcp
    Config Engine    ${1}
    Config Broker    rrd
    FOR    ${comp1}    IN    @{choices}
        FOR    ${comp2}    IN    @{choices}
            Log To Console    TLS set to ${comp1} on central and to ${comp2} on module
            Config Broker    central
            Config Broker    module
            Broker Config Input Set    central    central-broker-master-input    tls    ${comp1}
            Broker Config Output Set    module0    central-module-master-output    tls    ${comp2}
            Broker Config Log    central    bbdo    info
            Broker Config Log    module0    bbdo    info
            Broker Config Log    central    grpc    debug
            Broker Config Log    module0    grpc    debug
            Change Broker Tcp Output To Grpc    module0
            Change Broker Tcp Input To Grpc    central
            ${start}    Get Current Date
            Start Broker
            Start Engine
            ${result}    Check Connections
            Should Be True    ${result}    Engine and Broker not connected
            Kindly Stop Broker
            Stop Engine
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
            ${result}    Find In Log    ${centralLog}    ${start}    ${content1}
            Should Be True    ${result}
            ${result}    Find In Log    ${engineLog0}    ${start}    ${content2}
            Should Be True    ${result}
        END
    END

BECT_GRPC2
    [Documentation]    Broker/Engine communication with TLS between central and poller with key/cert
    [Tags]    broker    engine    tls    tcp
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module

    Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/server.key
    ...    ${EtcRoot}/centreon-broker/server.crt
    Create Key And Certificate
    ...    localhost
    ...    ${EtcRoot}/centreon-broker/client.key
    ...    ${EtcRoot}/centreon-broker/client.crt

    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/client.key
    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/client.crt
    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/server.key
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/server.crt
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Broker Config Log    central    tls    debug
    Broker Config Log    module0    tls    debug
    Broker Config Log    central    bbdo    info
    Broker Config Log    module0    bbdo    info
    Broker Config Input Set    central    central-broker-master-input    tls    yes
    Broker Config Output Set    module0    central-module-master-output    tls    yes
    Change Broker Tcp Output To Grpc    module0
    Change Broker Tcp Input To Grpc    central
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    Kindly Stop Broker
    Stop Engine
    ${content1}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content2}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content1}    Combine Lists    ${content1}    ${LIST_HANDSHAKE}
    ${content2}    Combine Lists    ${content2}    ${LIST_HANDSHAKE}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}    Find In Log    ${log}    ${start}    ${content1}
    Should Be True    ${result}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-module-master0.log
    ${result}    Find In Log    ${log}    ${start}    ${content2}
    Should Be True    ${result}

BECT_GRPC3
    [Documentation]    Broker/Engine communication with anonymous TLS and ca certificate
    [Tags]    broker    engine    tls    tcp
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module

    ${hostname}    Get Hostname
    Create Certificate    ${hostname}    ${EtcRoot}/centreon-broker/server.crt
    Create Certificate    ${hostname}    ${EtcRoot}/centreon-broker/client.crt

    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Broker Config Log    central    tls    debug
    Broker Config Log    module0    tls    debug
    Broker Config Log    central    bbdo    info
    Broker Config Log    module0    bbdo    info
    Change Broker Tcp Output To Grpc    module0
    Change Broker Tcp Input To Grpc    central
    Broker Config Input Set    central    central-broker-master-input    tls    yes
    Broker Config Output Set    module0    central-module-master-output    tls    yes
    # We get the current date just before starting broker
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    Kindly Stop Broker
    Stop Engine
    ${content1}    Create List    we have extensions 'TLS' and peer has 'TLS'    using anonymous server credentials
    ${content2}    Create List    we have extensions 'TLS' and peer has 'TLS'    using anonymous client credentials
    ${content1}    Combine Lists    ${content1}    ${LIST_HANDSHAKE}
    ${content2}    Combine Lists    ${content2}    ${LIST_HANDSHAKE}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}    Find In Log    ${log}    ${start}    ${content1}
    Should Be True    ${result}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-module-master0.log
    ${result}    Find In Log    ${log}    ${start}    ${content2}
    Should Be True    ${result}

BECT_GRPC4
    [Documentation]    Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
    [Tags]    broker    engine    tls    tcp
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module

    Set Local Variable    ${hostname}    centreon
    Create Key And Certificate
    ...    ${hostname}
    ...    ${EtcRoot}/centreon-broker/server.key
    ...    ${EtcRoot}/centreon-broker/server.crt
    Create Key And Certificate
    ...    ${hostname}
    ...    ${EtcRoot}/centreon-broker/client.key
    ...    ${EtcRoot}/centreon-broker/client.crt

    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    Broker Config Log    central    tls    debug
    Broker Config Log    module0    tls    debug
    Broker Config Log    central    bbdo    info
    Broker Config Log    module0    bbdo    info
    Change Broker Tcp Output To Grpc    module0
    Change Broker Tcp Input To Grpc    central
    Broker Config Input Set    central    central-broker-master-input    tls    yes
    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/client.key
    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/client.crt
    Broker Config Input Set
    ...    central
    ...    central-broker-master-input
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/server.crt
    Broker Config Input Set    central    central-broker-master-input    tls_hostname    centreon
    Broker Config Output Set    module0    central-module-master-output    tls    yes
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    private_key
    ...    ${EtcRoot}/centreon-broker/server.key
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    public_cert
    ...    ${EtcRoot}/centreon-broker/server.crt
    Broker Config Output Set
    ...    module0
    ...    central-module-master-output
    ...    ca_certificate
    ...    ${EtcRoot}/centreon-broker/client.crt
    # We get the current date just before starting broker
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    Kindly Stop Broker
    Stop Engine
    ${content1}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content2}    Create List    we have extensions 'TLS' and peer has 'TLS'    using certificates as credentials
    ${content1}    Combine Lists    ${content1}    ${LIST_HANDSHAKE}
    ${content2}    Combine Lists    ${content2}    ${LIST_HANDSHAKE}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}    Find In Log    ${log}    ${start}    ${content1}
    Should Be True    ${result}
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-module-master0.log
    ${result}    Find In Log    ${log}    ${start}    ${content2}
    Should Be True    ${result}

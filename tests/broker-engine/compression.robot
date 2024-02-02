*** Settings ***
Documentation       Centreon Broker and Engine communication with or without compression

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Variables ***
&{ext}          yes=COMPRESSION    no=    auto=COMPRESSION
@{choices}      yes    no    auto


*** Test Cases ***
BECC1
    [Documentation]    Broker/Engine communication with compression between central and poller
    [Tags]    broker    engine    compression    tcp
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    FOR    ${comp1}    IN    @{choices}
        FOR    ${comp2}    IN    @{choices}
            Log To Console    Compression set to ${comp1} on central and to ${comp2} on module
            Ctn Config Broker    central
            Ctn Config Broker    module
            Ctn Broker Config Input Set    central    central-broker-master-input    compression    ${comp1}
            Ctn Broker Config Output Set    module0    central-module-master-output    compression    ${comp2}
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
                ...    extension 'COMPRESSION' is set to 'yes' in the configuration but cannot be activated because of peer configuration
            ELSE IF    "${comp1}" == "no" and "${comp2}" == "yes"
                Insert Into List
                ...    ${content2}
                ...    ${-1}
                ...    extension 'COMPRESSION' is set to 'yes' in the configuration but cannot be activated because of peer configuration
            END
            ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
            ${result}    Ctn Find In Log    ${log}    ${start}    ${content1}
            Should Be True    ${result}
            ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-module-master0.log
            ${result}    Ctn Find In Log    ${log}    ${start}    ${content2}
            Should Be True    ${result}
        END
    END

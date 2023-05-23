*** Settings ***
Documentation       Centreon Broker and Engine communication with or without compression

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
Test Teardown       Save logs If Failed


*** Variables ***
&{ext}          yes=COMPRESSION    no=    auto=COMPRESSION
@{choices}      yes    no    auto


*** Test Cases ***
BECC1
    [Documentation]    Broker/Engine communication with compression between central and poller
    [Tags]    broker    engine    compression    tcp
    Config Engine    ${1}
    Config Broker    rrd
    FOR    ${comp1}    IN    @{choices}
        FOR    ${comp2}    IN    @{choices}
            Log To Console    Compression set to ${comp1} on central and to ${comp2} on module
            Config Broker    central
            Config Broker    module
            Broker Config Input set    central    central-broker-master-input    compression    ${comp1}
            Broker Config Output set    module0    central-module-master-output    compression    ${comp2}
            Broker Config Log    central    bbdo    info
            Broker Config Log    module0    bbdo    info
            ${start}=    Get Current Date
            Start Broker
            Start Engine
            ${result}=    Check Connections
            Should Be True    ${result}    msg=Engine and Broker not connected
            Kindly Stop Broker
            Stop Engine
            ${content1}=    Create List    we have extensions '${ext["${comp1}"]}' and peer has '${ext["${comp2}"]}'
            ${content2}=    Create List    we have extensions '${ext["${comp2}"]}' and peer has '${ext["${comp1}"]}'
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
            ${log}=    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
            ${result}=    Find In Log    ${log}    ${start}    ${content1}
            Should Be True    ${result}
            ${log}=    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-module-master0.log
            ${result}=    Find In Log    ${log}    ${start}    ${content2}
            Should Be True    ${result}
        END
    END

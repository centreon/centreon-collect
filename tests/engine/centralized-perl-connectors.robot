*** Settings ***
Documentation       Centreon Engine test perl connectors with centralized configuration

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CEPC1
    [Documentation]    Given Engine is configured with a Perl connector
    ...    When Engine starts
    ...    Then the Perl connector is launched and data becomes available
    [Tags]    engine    start-stop
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    bbdo    info
    Ctn Engine Config Set Value    ${0}    log_level_commands    trace
    ${start}    Ctn Get Round Current Date

    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    # Sending the new configuration to Engine.
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${content}    Create List    connector::run: connector='Perl Connector'
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Missing a message talking about 'Perl Connector'

    ${content}    Create List    connector::data_is_available
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
    Should Be True    ${result}    Missing a message telling data is available from the Perl connector

    Ctn Stop Engine

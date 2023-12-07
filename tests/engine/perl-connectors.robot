*** Settings ***
Documentation       Centreon Engine test perl connectors

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes


*** Test Cases ***
EPC1
    [Documentation]    Check with perl connector
    [Tags]    engine    start-stop
    Config Engine    ${1}
    Config Broker    module
    Engine Config Set Value    ${0}    log_level_commands    trace
    ${start}    Get Current Date

    Start Engine
    ${content}    Create List    connector::run: connector='Perl Connector'
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Missing a message talking about 'Perl Connector'

    ${content}    Create List    connector::data_is_available
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
    Should Be True    ${result}    Missing a message telling data is available from the Perl connector

    Stop Engine

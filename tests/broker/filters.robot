*** Settings ***
Documentation       Centreon Broker only start/stop tests

Resource            ../resources/resources.robot
Library             Process
Library             OperatingSystem
Library             ../resources/Broker.py
Library             DateTime

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes


*** Test Cases ***
BFC1
    [Documentation]    Start broker with invalid filters but one filter ok
    [Tags]    broker    start-stop    log-v2
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    config    info
    Broker Config Log    central    sql    error
    Broker Config Log    central    core    error
    Broker Config Output Set Json
    ...    central
    ...    central-broker-master-sql
    ...    filters
    ...    {"category": ["neb", "foo", "bar"]}
    ${start}=    Get Round Current Date
    Start Broker
    ${content}=    Create List
    ...    'foo' is not a known category: cannot find event category 'foo'
    ...    'bar' is not a known category: cannot find event category 'bar'
    ...    Filters applied on endpoint:neb
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg="Only neb filter should be applied on sql output"

    Kindly Stop Broker

BFC2
    [Documentation]    Start broker with only invalid filters on an output
    [Tags]    broker    start-stop    log-v2
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    config    info
    Broker Config Log    central    sql    error
    Broker Config Log    central    core    error
    Broker Config Output Set Json
    ...    central
    ...    central-broker-master-sql
    ...    filters
    ...    {"category": ["doe", "foo", "bar"]}
    ${start}=    Get Round Current Date
    Start Broker
    ${content}=    Create List
    ...    'doe' is not a known category: cannot find event category 'doe'
    ...    'bar' is not a known category: cannot find event category 'bar'
    ...    Filters applied on endpoint:all
    ${result}=    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg="Only neb filter should be applied on sql output"

    Kindly Stop Broker

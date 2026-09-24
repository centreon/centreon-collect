*** Settings ***
Documentation       Centreon Broker only start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BFC1
    [Documentation]    Start broker with invalid filters but one filter ok
    [Tags]    broker    start-stop    log-v2

    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    info
    Ctn Broker Config Log    central    sql    error
    Ctn Broker Config Log    central    core    error
    ${test_direct_grpc}    Ctn Is Using Direct Grpc
    IF    ${test_direct_grpc}
        Ctn Broker Config Output Set Json
        ...    central
        ...    central-broker-unified-sql
        ...    filters
        ...    {"category": ["neb", "foo", "bar"]}
    ELSE
        Ctn Broker Config Output Set Json
        ...    central
        ...    central-broker-master-sql
        ...    filters
        ...    {"category": ["neb", "foo", "bar"]}
    END
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    ${content}    Create List
    ...    'foo' is not a known category: cannot find event category 'foo'
    ...    'bar' is not a known category: cannot find event category 'bar'
    ...    Filters applied on endpoint:neb
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    "Only neb filter should be applied on sql output"

    Ctn Kindly Stop Broker

BFC2
    [Documentation]    Start broker with only invalid filters on an output
    [Tags]    broker    start-stop    log-v2

    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    info
    Ctn Broker Config Log    central    sql    error
    Ctn Broker Config Log    central    core    error
    ${test_direct_grpc}    Ctn Is Using Direct Grpc
    IF    ${test_direct_grpc}
        Ctn Broker Config Output Set Json
        ...    central
        ...    central-broker-unified-sql
        ...    filters
        ...    {"category": ["doe", "foo", "bar"]}
    ELSE
        Ctn Broker Config Output Set Json
        ...    central
        ...    central-broker-master-sql
        ...    filters
        ...    {"category": ["doe", "foo", "bar"]}
    END
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    ${content}    Create List
    ...    'doe' is not a known category: cannot find event category 'doe'
    ...    'bar' is not a known category: cannot find event category 'bar'
    ...    Filters applied on endpoint:all
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    "Only neb filter should be applied on sql output"

    Ctn Kindly Stop Broker

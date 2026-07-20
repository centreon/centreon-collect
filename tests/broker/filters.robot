*** Settings ***
Documentation       Centreon Broker only start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
BFC1
    [Documentation]    Scenario: Start broker with valid and invalid filters on an output
    ...   Given Broker is configured with filters "neb", "foo", and "bar" on the unified SQL output
    ...   When Broker is started
    ...   Then error messages should appear for invalid categories "foo" and "bar"
    ...   And only the valid "neb" filter should be applied
    [Tags]    broker    start-stop    log-v2
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    info
    Ctn Broker Config Log    central    sql    error
    Ctn Broker Config Log    central    core    error
    Ctn Config BBDO3    ${0}    3.1.0    ${True}
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Ctn Broker Config Output Set Json
    ...    central
    ...    central-broker-unified-sql
    ...    filters
    ...    {"category": ["neb", "foo", "bar"]}
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
    [Documentation]    Scenario: Start broker with only invalid filters on an output
    ...   Given Broker is configured with filters "doe", "foo", and "bar" on the unified SQL output
    ...   When Broker is started
    ...   Then error messages should appear for invalid categories
    [Tags]    broker    start-stop    log-v2
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    info
    Ctn Broker Config Log    central    sql    error
    Ctn Broker Config Log    central    core    error
    Ctn Config BBDO3    ${0}    3.1.0    ${True}
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Ctn Broker Config Output Set Json
    ...    central
    ...    central-broker-unified-sql
    ...    filters
    ...    {"category": ["doe", "foo", "bar"]}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    ${content}    Create List
    ...    'doe' is not a known category: cannot find event category 'doe'
    ...    'bar' is not a known category: cannot find event category 'bar'
    ...    Filters applied on endpoint:all
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    "Only neb filter should be applied on sql output"

    Ctn Kindly Stop Broker

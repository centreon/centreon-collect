*** Settings ***
Documentation       Engine/Broker tests on severities.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BECSEV1
    [Documentation]    Feature: Severity Management between Engine and Broker
    ...    As a Centreon administrator
    ...    I want to configure severities in Engine
    ...    So that Broker stores them correctly in centreon_storage.severities table
    ...
    ...    Background:
    ...        Given Engine is configured with centralized setup
    ...        And Broker components (central, rrd, module) are configured
    ...        And Database logging is enabled with debug/trace level
    ...        And Retention data is cleared
    ...
    ...    Scenario: Initial severity configuration
    ...        Given Engine is configured with 20 severities
    ...        When Broker and Engine are started
    ...        Then 20 severities should be added/modified in logs
    ...        And INSERT statements should be executed in severities table
    ...        And Configuration file should match database content
    ...        And Severity IDs should be consistent
    ...
    ...    Scenario: Severity configuration modification
    ...        Given Initial configuration with 20 severities is loaded
    ...        When Configuration is modified to 30 severities
    ...        And Engine configuration change is notified
    ...        Then 10 additional severities should be added/modified
    ...        And Configuration file should still match database content
    ...        And Severity IDs should remain consistent
    ...
    ...    Scenario: Severity configuration reduction
    ...        Given Configuration with 30 severities is loaded
    ...        When Configuration is reduced to 11 severities starting at ID 50
    ...        And Engine configuration change is notified
    ...        Then 11 severities should be present in final configuration
    ...        And Unused severities should be implicitly removed
    ...        And Configuration file should match database content
    ...        And Severity IDs should be consistent with new range
    [Tags]    broker    engine    protobuf    bbdo    severities    MON-153802
    # Clear Db severities
    Ctn Config Centralized Engine    ${1}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    module0    processing    error
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Clear Retention
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    ${result}    Ctn Check Severity With Timeout    severity20    5    1    30

    Log To Console    At first, we should have 20 severities added/modified.
    ${content}    Create List    20 severities added/modified    mysql_connection .*: INSERT INTO severities
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result[0]}    20 severities should be added/modified
    ${result}    Ctn Severities Are Identical    ${1}    ${VarRoot}/lib/centreon/config/1/severities.cfg
    Should Be True    ${result}    Severities are not identical between database and configuration file for poller 1.
    ${result}    Ctn Check Severity Ids    ${centralLog}    ${start}
    Should Be True    ${result}    Severity ids are not correct.

    # Modification of the severities configuration
    ${start}    Ctn Get Round Current Date
    Ctn Create Severities File    ${0}    ${30}
    Ctn Notify Broker Of Engine Config Change    0

    # Check to verify the new severities have been added.
    Log To Console    Now, we should have 30 severities.
    ${content}    Create List    10 severities added/modified    mysql_connection .*: INSERT INTO severities
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result[0]}    20 severities should be added/modified
    ${result}    Ctn Severities Are Identical    ${1}    ${VarRoot}/lib/centreon/config/1/severities.cfg
    Should Be True    ${result}    Severities are not identical between database and configuration file for poller 1.
    ${result}    Ctn Check Severity Ids    ${centralLog}    ${start}
    Should Be True    ${result}    Severity ids are not correct.

    # Last Modification
    ${start}    Ctn Get Round Current Date
    Ctn Create Severities File    ${0}    ${11}    ${50}
    Ctn Notify Broker Of Engine Config Change    0

    Log To Console    And now, we should just have 11 severities.
    ${content}    Create List    11 severities added/modified    mysql_connection .*: INSERT INTO severities
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result[0]}    11 severities should be added/modified
    ${result}    Ctn Severities Are Identical    ${1}    ${VarRoot}/lib/centreon/config/1/severities.cfg    timeout=30
    Should Be True    ${result}    Severities are not identical between database and configuration file for poller 1.
    ${result}    Ctn Check Severity Ids    ${centralLog}    ${start}
    Should Be True    ${result}    Severity ids are not correct.

    Log To Console    Stopping Engine end Broker
    Ctn Stop Engine
    Ctn Kindly Stop Broker

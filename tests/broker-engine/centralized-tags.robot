*** Settings ***
Documentation       Engine/Broker tests on tags.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BECTAG1
    [Documentation]    Feature: Tag Management between Engine and Broker
    ...    As a Centreon administrator
    ...    I want to configure tags in Engine
    ...    So that Broker stores them correctly in centreon_storage.tags table
    ...
    ...    Background:
    ...        Given Engine is configured with centralized setup
    ...        And Broker components (central, rrd, module) are configured
    ...        And Database logging is enabled with debug/trace level
    ...        And Retention data is cleared
    ...
    ...    Scenario: Initial tag configuration
    ...        Given Engine is configured with 20 tags
    ...        When Broker and Engine are started
    ...        Then 20 tags should be added/modified in logs
    ...        And INSERT statements should be executed in tags table
    ...        And Configuration file should match database content
    ...        And Tag IDs should be consistent
    ...
    ...    Scenario: Tag configuration modification
    ...        Given Initial configuration with 20 tags is loaded
    ...        When Configuration is modified to 30 tags
    ...        And Engine configuration change is notified
    ...        Then 10 additional tags should be added/modified
    ...        And Configuration file should still match database content
    ...        And Tag IDs should remain consistent
    ...
    ...    Scenario: Tag configuration reduction
    ...        Given Configuration with 30 tags is loaded
    ...        When Configuration is reduced to 11 tags starting at ID 50
    ...        And Engine configuration change is notified
    ...        Then 11 tags should be present in final configuration
    ...        And Unused tags should be implicitly removed
    ...        And Configuration file should match database content
    ...        And Tag IDs should be consistent with new range
    [Tags]    broker    engine    protobuf    bbdo    tags    MON-153802
    # Clear Db tags
    Ctn Config Centralized Engine    ${1}
    Ctn Create Tags File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
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
    ${result}    Ctn Check Tag With Timeout    tag20    3    30

    Log To Console    At first, we should have 20 tags added/modified.
    ${content}    Create List    20 tags added/modified    execute statement 59b6865e: INSERT INTO tags
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    20 tags should be added/modified
    ${result}    Ctn Tags Are Identical    ${1}    ${VarRoot}/lib/centreon/config/1/tags.cfg
    Should Be True    ${result}    Tags are not identical between database and configuration file for poller 1.
    ${result}    Ctn Check Tag Ids    ${centralLog}    ${start}
    Should Be True    ${result}    Tag ids are not correct.

    # Modification of the tags configuration
    ${start}    Ctn Get Round Current Date
    Ctn Create Tags File    ${0}    ${30}
    Ctn Notify Broker Of Engine Config Change    0

    # Check to verify the new tags have been added.
    Log To Console    Now, we should have 30 tags.
    ${content}    Create List    10 tags added/modified    execute statement 59b6865e: INSERT INTO tags
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    20 tags should be added/modified
    ${result}    Ctn Tags Are Identical    ${1}    ${VarRoot}/lib/centreon/config/1/tags.cfg
    Should Be True    ${result}    Tags are not identical between database and configuration file for poller 1.
    ${result}    Ctn Check Tag Ids    ${centralLog}    ${start}
    Should Be True    ${result}    Tag ids are not correct.

    # Last Modification
    ${start}    Ctn Get Round Current Date
    Ctn Create Tags File    ${0}    ${11}    ${50}
    Ctn Notify Broker Of Engine Config Change    0

    Log To Console    And now, we should just have 11 tags.
    ${content}    Create List    11 tags added/modified    execute statement 59b6865e: INSERT INTO tags
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    11 tags should be added/modified
    ${result}    Ctn Tags Are Identical    ${1}    ${VarRoot}/lib/centreon/config/1/tags.cfg    timeout=30
    Should Be True    ${result}    Tags are not identical between database and configuration file for poller 1.
    ${result}    Ctn Check Tag Ids    ${centralLog}    ${start}
    Should Be True    ${result}    Tag ids are not correct.

    Log To Console    Stopping Engine end Broker
    Ctn Stop Engine
    Ctn Kindly Stop Broker

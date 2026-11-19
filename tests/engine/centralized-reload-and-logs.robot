*** Settings ***
Documentation       Centreon Engine reload and log tests with centralized configuration

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CERL
    [Documentation]    Given Engine is started and writing logs to centengine.log
    ...    When the log file is removed
    ...    Then Engine continues running but the log file is gone
    ...    And when Engine is reloaded the centengine.log file is recreated
    [Tags]    engine    log-v2    MON-146656
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    bbdo    info
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_events    info
    Ctn Engine Config Set Value    ${0}    log_flush_period    0

    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker	newGeneration=True
    Ctn Start Engine    newGeneration=True

    # Sending the new configuration to Engine.
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.
    Log To Console    centengine.log should exist

    File Should Exist    ${VarRoot}/log/centreon-engine/config0/centengine.log

    Log To Console    centengine.log is removed
    Remove File    ${VarRoot}/log/centreon-engine/config0/centengine.log

    Wait Until Removed    ${VarRoot}/log/centreon-engine/config0/centengine.log    timeout=30s

    Log To Console    After , centengine.log should not exist
    File Should Not Exist    ${VarRoot}/log/centreon-engine/config0/centengine.log
    Ctn Reload Engine
    Log To Console    After centengine reload, centengine.log should exist again.

    Wait Until Created    ${VarRoot}/log/centreon-engine/config0/centengine.log    timeout=30s
    Ctn Stop Engine
    Ctn Kindly Stop Broker

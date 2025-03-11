*** Settings ***
Documentation       Centreon Engine forced checks tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
ERL
    [Documentation]    Engine is started and writes logs in centengine.log.
    ...    Then we remove the log file. The file disappears but Engine is still writing into it.
    ...    Engine is reloaded and the centengine.log should appear again.
    [Tags]    engine    log-v2    MON-146656
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_events    info
    Ctn Engine Config Set Value    ${0}    log_flush_period    0

    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

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

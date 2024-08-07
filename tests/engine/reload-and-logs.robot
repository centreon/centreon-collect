*** Settings ***
Documentation       Centreon Engine forced checks tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
ERL
    [Documentation]    Engine is started and writes logs in centengine.log.
    ...    Then we remove the log file. The file disappears but Engine is still writing into it.
    ...    Engine is reloaded and the centengine.log should appear again.
    [Tags]    engine    log-v2    MON-146656
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_events    info
    Ctn Engine Config Set Value    ${0}    log_flush_period    0

    Ctn Clear Retention
    Ctn Clear Db    hosts
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    File Should Exist    ${VarRoot}/log/centreon-engine/config0/centengine.log

    Remove File    ${VarRoot}/log/centreon-engine/config0/centengine.log

    Sleep    5s

    File Should Not Exist    ${VarRoot}/log/centreon-engine/config0/centengine.log
    Ctn Reload Engine

    Wait Until Created    ${VarRoot}/log/centreon-engine/config0/centengine.log    timeout=30s
    Ctn Stop Engine

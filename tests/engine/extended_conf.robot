*** Settings ***
Documentation       Centreon Engine forced checks tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Run Keywords    Ctn Stop engine    AND    Ctn Save Logs If Failed


*** Test Cases ***
EXT_CONF1
    [Documentation]    Engine configuration is overided by json conf
    [Tags]    engine    mon-34326
    Ctn Config Engine    ${1}
    Ctn Config Broker    module    ${1}
    Create File    /tmp/centengine_extend.json    {"log_level_checks": "trace", "log_level_comments": "debug"}
    ${start}    Get Current Date
    Ctn Start Engine With Extend Conf
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${level}    Ctn Get Engine Log Level    50001    checks
    Should Be Equal    ${level}    trace    log_level_checks must come from the extended conf, trace
    ${level}    Ctn Get Engine Log Level    50001    comments
    Should Be Equal    ${level}    debug    log_level_comments must come from the extended conf, debug

EXT_CONF2
    [Documentation]    Engine configuration is overided by json conf after reload
    [Tags]    engine    mon-34326
    Ctn Config Engine    ${1}
    Ctn Config Broker    module    ${1}
    Create File    /tmp/centengine_extend.json    {}
    ${start}    Get Current Date
    Ctn Start Engine With Extend Conf
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Create File    /tmp/centengine_extend.json    {"log_level_checks": "trace", "log_level_comments": "debug"}

    ${start}    Get Current Date
    Send Signal To Process    SIGHUP    e0
    ${content}    Create List    Need reload.
    ${result}    Ctn Find In Log With Timeout
    ...    ${ENGINE_LOG}/config0/centengine.log
    ...    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    A message telling Need reload. should be available in config0/centengine.log.

    ${level}    Ctn Get Engine Log Level    50001    checks
    Should Be Equal    ${level}    trace    log_level_checks must be the extended conf value
    ${level}    Ctn Get Engine Log Level    50001    comments
    Should Be Equal    ${level}    debug    log_level_comments must be the extended conf value

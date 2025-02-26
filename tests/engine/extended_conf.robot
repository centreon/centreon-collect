*** Settings ***
Documentation       Centreon Engine forced checks tests

Resource            ../resources/resources.robot
Library             DateTime
Library             ../resources/Broker.py
Library             ../resources/Engine.py

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Run Keywords    Ctn Stop engine    AND    Ctn Save Logs If Failed


*** Test Cases ***
EXT_CONF1
    [Documentation]    Engine configuration is overided by json conf
    [Tags]    engine    MON-71614
    Ctn Config Engine    ${1}
    Ctn Config Broker    module    ${1}
    Create File    /tmp/centengine_extend.json    {"log_level_checks": "trace", "log_level_comments": "debug"}
    ${start}    Get Current Date
    Ctn Start Engine With Extend Conf
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${level}    Ctn Get Engine Log Level    50001    checks
    Should Be Equal    ${level}    trace    log_level_checks must be the extended conf value
    ${level}    Ctn Get Engine Log Level    50001    comments
    Should Be Equal    ${level}    debug    log_level_comments must be the extended conf value

EXT_CONF2
    [Documentation]    Engine configuration is overided by json conf after reload
    [Tags]    engine    MON-71614
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

VERIFIY_CONF
    [Documentation]    Verify deprecated engine configuration options are logged as warnings
    ...    Given the engine and broker are configured with module 1
    ...    And the engine configuration is set with deprecated options
    ...    When the engine is started
    ...    Then a warning message for 'auto_reschedule_checks' should be logged
    ...    And a warning message for 'auto_rescheduling_interval' should be logged
    ...    And a warning message for 'auto_rescheduling_window' should be logged
    ...    And the engine should be stopped
    [Tags]    engine    MON-158938
    Ctn Config Engine    ${1}
    Ctn Config Broker    module    

    Ctn Engine Config Set Value    ${0}    auto_reschedule_checks    1    True
    Ctn Engine Config Set value    ${0}    auto_rescheduling_interval    30    True
    Ctn Engine Config Set value    ${0}    auto_rescheduling_window    60    True
    
    ${start}    Get Current Date
    Sleep    1s
    Ctn Start Engine
    
    # look in logfile a warning that tell the auto reshucling is deprecated
    ${content}    Create List    The option 'auto_reschedule_checks' is no longer available. This option is deprecated.
    ${result}    Ctn Find In Log With Timeout    ${ENGINE_LOG}/config0/centengine-stdout.log    ${start}    ${content}    60
        Should Be True
    ...    ${result}
    ...    A message telling auto_reschedule_checks is deprecated. should be available in config0/centengine-stdout.log.
 
    # look in logfile a warning that tell the auto reshucling is deprecated
    ${content}    Create List    The option 'auto_rescheduling_interval' is no longer available. This option is deprecated.
    ${result}    Ctn Find In Log With Timeout    ${ENGINE_LOG}/config0/centengine-stdout.log    ${start}    ${content}    60
        Should Be True
    ...    ${result}
    ...    A message telling auto_rescheduling_interval is deprecated. should be available in config0/centengine-stdout.log.

    # look in logfile a warning that tell the auto reshucling is deprecated
    ${content}    Create List    The option 'auto_rescheduling_window' is no longer available. This option is deprecated.
    ${result}    Ctn Find In Log With Timeout    ${ENGINE_LOG}/config0/centengine-stdout.log    ${start}    ${content}    60
        Should Be True
    ...    ${result}
    ...    A message telling auto_rescheduling_window is deprecated. should be available in config0/centengine-stdout.log.

    Ctn Stop engine
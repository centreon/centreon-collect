*** Settings ***
Documentation       Centreon Engine extended configuration tests with centralized configuration

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Run Keywords    Ctn Stop Engine    AND    Ctn Save Logs If Failed


*** Test Cases ***
CEXT_CONF1
    [Documentation]    Given Engine is configured with a module broker
    ...    When Engine starts with an extended JSON configuration overriding log levels
    ...    Then the log levels from the extended conf are applied at startup
    [Tags]    engine    mon-34326
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    rrd    core    error

    Create File    /tmp/centengine_extend.json    {"log_level_checks": "trace", "log_level_comments": "debug"}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True    with_extended_conf=True
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${level}    Ctn Get Engine Log Level    50001    checks
    Should Be Equal    ${level}    trace    log_level_checks must come from the extended conf: trace
    ${level}    Ctn Get Engine Log Level    50001    comments
    Should Be Equal    ${level}    debug    log_level_comments must come from the extended conf: debug

CEXT_CONF2
    [Documentation]    Given Engine is configured with a module broker and an empty extended JSON conf
    ...    When the extended conf is updated with new log levels and Engine is reloaded
    ...    Then the new log levels from the updated extended conf are applied after reload
    [Tags]    engine    mon-34326
    Ctn Config Engine    ${1}
    Ctn Config Broker    module    ${1}
    Create File    /tmp/centengine_extend.json    {}
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine With Extended Conf
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Create File    /tmp/centengine_extend.json    {"log_level_checks": "trace", "log_level_comments": "debug"}

    ${start}    Ctn Get Round Current Date
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

CVERIFY_CONF
    [Documentation]    Given Engine and broker are configured with module
    ...    And the engine configuration includes deprecated options
    ...    When Engine starts
    ...    Then a warning message for 'auto_reschedule_checks' is logged
    ...    And a warning message for 'auto_rescheduling_interval' is logged
    ...    And a warning message for 'auto_rescheduling_window' is logged
    [Tags]    engine    MON-158938
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    config    info

    Ctn Engine Config Set Value    ${0}    auto_reschedule_checks    1    True
    Ctn Engine Config Set value    ${0}    auto_rescheduling_interval    30    True
    Ctn Engine Config Set value    ${0}    auto_rescheduling_window    60    True

    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Start Broker    newGeneration=True

    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    # look in logfile a warning that tell the auto reshucling is deprecated
    ${content}    Create List    The option 'auto_reschedule_checks' is no longer available. This option is deprecated.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
        Should Be True
    ...    ${result}
    ...    A message telling auto_reschedule_checks is deprecated. should be available in config0/centengine-stdout.log.

    # look in logfile a warning that tell the auto reshucling is deprecated
    ${content}    Create List    The option 'auto_rescheduling_interval' is no longer available. This option is deprecated.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
        Should Be True
    ...    ${result}
    ...    A message telling auto_rescheduling_interval is deprecated. should be available in config0/centengine-stdout.log.

    # look in logfile a warning that tell the auto reshucling is deprecated
    ${content}    Create List    The option 'auto_rescheduling_window' is no longer available. This option is deprecated.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
        Should Be True
    ...    ${result}
    ...    A message telling auto_rescheduling_window is deprecated. should be available in config0/centengine-stdout.log.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

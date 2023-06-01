*** Settings ***
Documentation       Centreon Engine forced checks tests

Resource            ../resources/resources.robot
Library             DateTime
Library             Process
Library             OperatingSystem
Library             ../resources/Broker.py
Library             ../resources/Engine.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes


*** Test Cases ***
EFHC1
    [Documentation]    Engine is configured with hosts and we force checks on one 5 times on bbdo2
    [Tags]    engine    external_cmd    log-v2
    Config Engine    ${1}
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}

    Clear Retention
    Clear DB    hosts
    ${start}=    Get Current Date
    Start Engine
    Start Broker
    ${result}=    Check host status    host_1    4    1    False
    Should be true    ${result}    msg=host_1 should be pending

    ${content}=    Create List    INITIAL HOST STATE: host_1;
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
    Process host check result    host_1    0    host_1 UP
    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced HOST CHECK    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    ${content}=    Create List
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;1;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;2;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;HARD;3;

    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=Message about SCHEDULE FORCED CHECK and HOST ALERT should be available in log.

    ${result}=    Check host status    host_1    1    1    False
    Should be true    ${result}    msg=host_1 should be down/hard
    Stop Engine
    Kindly Stop Broker

EFHC2
    [Documentation]    Engine is configured with hosts and we force checks on one 5 times on bbdo2
    [Tags]    engine    external_cmd    log-v2
    Config Engine    ${1}
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}

    Clear Retention
    ${start}=    Get Current Date
    Start Engine
    Start Broker
    ${content}=    Create List    INITIAL HOST STATE: host_1;
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
    Process host check result    host_1    0    host_1 UP
    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced HOST CHECK    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    ${content}=    Create List
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;1;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;2;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;HARD;3;

    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=Message about SCHEDULE FORCED CHECK and HOST ALERT should be available in log.

    ${result}=    Check host status    host_1    1    1    False
    Should be true    ${result}    msg=host_1 should be down/hard
    Stop Engine
    Kindly Stop Broker

EFHCU1
    [Documentation]    Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior. resources table is cleared before starting broker.
    [Tags]    engine    external_cmd
    Config Engine    ${1}
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Log    module0    neb    debug
    Config Broker Sql Output    central    unified_sql
    Broker Config Log    central    sql    debug
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0

    Clear Retention
    Clear db    resources
    ${start}=    Get Current Date
    Start Engine
    Start Broker
    ${result}=    Check host status    host_1    4    1    True
    Should be true    ${result}    msg=host_1 should be pending
    ${content}=    Create List    INITIAL HOST STATE: host_1;
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
    Process host check result    host_1    0    host_1 UP
    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced HOST CHECK    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    ${content}=    Create List
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;1;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;2;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;HARD;3;

    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=Message about SCHEDULE FORCED CHECK and HOST ALERT should be available in log.

    ${result}=    Check host status    host_1    1    1    True
    Should be true    ${result}    msg=host_1 should be down/hard
    Stop Engine
    Kindly Stop Broker

EFHCU2
    [Documentation]    Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior.
    [Tags]    engine    external_cmd
    Config Engine    ${1}
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Log    module0    neb    debug
    Config Broker Sql Output    central    unified_sql
    Broker Config Log    central    sql    debug
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0

    Clear Retention
    ${start}=    Get Current Date
    Start Engine
    Start Broker
    ${result}=    Check host status    host_1    4    1    True
    Should be true    ${result}    msg=host_1 should be pending
    ${content}=    Create List    INITIAL HOST STATE: host_1;
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    msg=An Initial host state on host_1 should be raised before we can start our external commands.
    Process host check result    host_1    0    host_1 UP
    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced HOST CHECK    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    ${content}=    Create List
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;1;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;2;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;HARD;3;

    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=Message about SCHEDULE FORCED CHECK and HOST ALERT should be available in log.

    ${result}=    Check host status    host_1    1    1    True
    Should be true    ${result}    msg=host_1 should be down/hard
    Stop Engine
    Kindly Stop Broker

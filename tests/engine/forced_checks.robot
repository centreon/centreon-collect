*** Settings ***
Documentation       Centreon Engine forced checks tests

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes


*** Test Cases ***
EFHC1
    [Documentation]    Engine is configured with hosts and we force checks on one 5 times on bbdo2
    [Tags]    engine    external_cmd    log-v2
    Config Engine    ${1}
    # We force the check command of host_1 to return 2 as status.
    Config Host Command Status    ${0}    checkh1    2
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Engine Config Set Value    ${0}    log_level_events    info
    Engine Config Set Value    ${0}    log_flush_period    0

    Clear Retention
    Clear Db    hosts
    ${start}    Get Current Date
    Start Engine
    Start Broker
    ${result}    Check Host Status    host_1    4    1    False
    Should Be True    ${result}    host_1 should be pending

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    Process Host Check Result    host_1    0    host_1 UP
    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    ${content}    Create List
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;1;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;2;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;HARD;3;

    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Message about SCHEDULE FORCED CHECK and HOST ALERT should be available in log.

    ${result}    Check Host Status    host_1    1    1    False
    Should Be True    ${result}    host_1 should be down/hard
    Stop Engine
    Kindly Stop Broker

EFHC2
    [Documentation]    Engine is configured with hosts and we force check on one 5 times on bbdo2
    [Tags]    engine    external_cmd    log-v2
    Config Engine    ${1}

    # We force the check command of host_1 to return 2 as status.
    Config Host Command Status    ${0}    checkh1    2
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}

    Clear Retention
    ${start}    Get Current Date
    Start Engine
    Start Broker
    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    Process Host Check Result    host_1    0    host_1 UP
    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    ${content}    Create List
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;1;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;2;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;HARD;3;

    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Message about SCHEDULE FORCED CHECK and HOST ALERT should be available in log.

    ${result}    Check Host Status    host_1    1    1    False
    Should Be True    ${result}    host_1 should be down/hard
    Stop Engine
    Kindly Stop Broker

EFHCU1
    [Documentation]    Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior. resources table is cleared before starting broker.
    [Tags]    engine    external_cmd
    Config Engine    ${1}

    # We force the check command of host_1 to return 2 as status.
    Config Host Command Status    ${0}    checkh1    2
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Broker Config Log    module0    neb    debug
    Config Broker Sql Output    central    unified_sql
    Broker Config Log    central    sql    debug
    Config BBDO3    1

    Clear Retention
    Clear Db    resources
    ${start}    Get Current Date
    Start Engine
    Start Broker
    ${result}    Check Host Status    host_1    4    1    True
    Should Be True    ${result}    host_1 should be pending
    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    Process Host Check Result    host_1    0    host_1 UP
    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    ${content}    Create List
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;1;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;2;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;HARD;3;

    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Message about SCHEDULE FORCED CHECK and HOST ALERT should be available in log.

    ${result}    Check Host Status    host_1    1    1    True
    Should Be True    ${result}    host_1 should be down/hard
    Stop Engine
    Kindly Stop Broker

EFHCU2
    [Documentation]    Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior.
    [Tags]    engine    external_cmd
    Config Engine    ${1}

    # We force the check command of host_1 to return 2 as status.
    Config Host Command Status    ${0}    checkh1    2
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Broker Config Log    module0    neb    debug
    Config Broker Sql Output    central    unified_sql
    Broker Config Log    central    sql    debug
    Config BBDO3    1

    Clear Retention
    ${start}    Get Current Date
    Start Engine
    Start Broker
    ${result}    Check Host Status    host_1    4    1    True
    Should Be True    ${result}    host_1 should be pending
    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    Process Host Check Result    host_1    0    host_1 UP
    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    ${content}    Create List
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;1;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;2;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;HARD;3;

    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Message about SCHEDULE FORCED CHECK and HOST ALERT should be available in log.

    ${result}    Check Host Status    host_1    1    1    True
    Should Be True    ${result}    host_1 should be down/hard
    Stop Engine
    Kindly Stop Broker

EMACROS
    [Documentation]    macros ADMINEMAIL and ADMINPAGER are replaced in check outputs
    [Tags]    engine    external_cmd    macros
    Config Engine    ${1}
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Engine Config Set Value    0    log_level_checks    trace    True
    Engine Config Change Command
    ...    0
    ...    \\d+
    ...    /bin/echo "ResourceFile: $RESOURCEFILE$ - LogFile: $LOGFILE$ - AdminEmail: $ADMINEMAIL$ - AdminPager: $ADMINPAGER$"
    Clear Retention
    ${start}    Get Current Date
    Start Engine
    Start Broker

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    Schedule Forced Svc Check    host_1    service_1
    Sleep    5s

    ${content}    Create List
    ...    ResourceFile: /tmp/etc/centreon-engine/config0/resource.cfg - LogFile: /tmp/var/log/centreon-engine/config0/centengine.log - AdminEmail: titus@bidibule.com - AdminPager: admin
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    AdminEmail: titus@bidibule.com - AdminPager: admin not found in log.

    Stop Engine
    Kindly Stop Broker

EMACROS_NOTIF
    [Documentation]    macros ADMINEMAIL and ADMINPAGER are replaced in notification commands
    [Tags]    engine    external_cmd    macros
    Config Engine    ${1}
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Engine Config Set Value    0    log_level_checks    trace    True
    Engine Config Add Value    0    cfg_file    ${EtcRoot}/centreon-engine/config0/contacts.cfg
    Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /bin/sh -c '/bin/echo "ResourceFile: $RESOURCEFILE$ - LogFile: $LOGFILE$ - AdminEmail: $ADMINEMAIL$ - AdminPager: $ADMINPAGER$" >> /tmp/notif_toto.txt'
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Remove File    /tmp/notif_toto.txt
    Clear Retention
    ${start}    Get Current Date
    Start Engine
    Start Broker

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    FOR    ${i}    IN RANGE    3
        Process Service Check Result    host_1    service_1    2    critical
    END

    Wait Until Created    /tmp/notif_toto.txt    30s

    ${grep_res}    Grep File
    ...    /tmp/notif_toto.txt
    ...    ResourceFile: /tmp/etc/centreon-engine/resource.cfg - LogFile: /tmp/var/log/centreon-engine/centengine.log - AdminEmail: titus@bidibule.com - AdminPager: admin

    Stop Engine
    Kindly Stop Broker

EMACROS_SEMICOLON
    [Documentation]    Macros with a semicolon are used even if they contain a semicolon.
    [Tags]    engine    external_cmd    macros    MON-15765
    Config Engine    ${1}
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Engine Config Set Value    0    log_level_checks    trace    True
    Engine Config Set Value In Hosts    0    host_1    _KEY2    VAL1;val3;
    Engine Config Change Command
    ...    0
    ...    \\d+
    ...    /bin/echo "KEY2=$_HOSTKEY2$"
    Clear Retention
    ${start}    Get Current Date
    Start Engine
    Start Broker

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    Schedule Forced Svc Check    host_1    service_1
    Sleep    5s

    ${content}    Create List    KEY2=VAL1;val3;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    VAL1;val3; not found in log.

    Stop Engine
    Kindly Stop Broker

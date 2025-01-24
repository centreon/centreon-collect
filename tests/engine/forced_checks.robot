*** Settings ***
Documentation       Centreon Engine forced checks tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
EFHC1
    [Documentation]    Engine is configured with hosts and we force check one 5 times with bbdo2
    [Tags]    engine    external_cmd    log_v2
    Ctn Config Engine    ${1}
    # We force the check command of host_1 to return 2 as status.
    Ctn Config Host Command Status    ${0}    checkh1    2
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_events    info
    Ctn Engine Config Set Value    ${0}    log_flush_period    0

    Ctn Clear Retention
    Ctn Clear Db    hosts
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker
    ${result}    Ctn Check Host Status    host_1    4    1    False
    Should Be True    ${result}    host_1 should be pending

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    Ctn Process Host Check Result    host_1    0    host_1 UP
    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    ${content}    Create List
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;1;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;2;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;HARD;3;

    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Message about SCHEDULE FORCED CHECK and HOST ALERT should be available in log.

    ${result}    Ctn Check Host Status    host_1    1    1    False
    Should Be True    ${result}    host_1 should be down/hard
    Ctn Stop Engine
    Ctn Kindly Stop Broker

EFHC2
    [Documentation]    Engine is configured with hosts and we force check on one 5 times on bbdo2
    [Tags]    engine    external_cmd    log_v2
    Ctn Config Engine    ${1}

    # We force the check command of host_1 to return 2 as status.
    Ctn Config Host Command Status    ${0}    checkh1    2
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}

    Ctn Clear Retention
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Start Broker
    Ctn Wait For Engine To Be Ready    ${start}

    Ctn Process Host Check Result    host_1    0    host_1 UP
    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    ${content}    Create List
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;1;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;2;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;HARD;3;

    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Message about SCHEDULE FORCED CHECK and HOST ALERT should be available in log.

    ${result}    Ctn Check Host Status    host_1    1    1    False
    Should Be True    ${result}    host_1 should be down/hard
    Ctn Stop Engine
    Ctn Kindly Stop Broker

EFHCU1
    [Documentation]    Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior. resources table is cleared before starting broker.
    [Tags]    engine    external_cmd
    Ctn Config Engine    ${1}

    # We force the check command of host_1 to return 2 as status.
    Ctn Config Host Command Status    ${0}    checkh1    2
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Broker Config Log    module0    neb    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    central    sql    debug
    Ctn Config BBDO3    1

    Ctn Clear Retention
    Ctn Clear Db    resources
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker
    ${result}    Ctn Check Host Status    host_1    4    1    True
    Should Be True    ${result}    host_1 should be pending
    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    Ctn Process Host Check Result    host_1    0    host_1 UP
    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    ${content}    Create List
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;1;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;2;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;HARD;3;

    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Message about SCHEDULE FORCED CHECK and HOST ALERT should be available in log.

    ${result}    Ctn Check Host Status    host_1    1    1    True
    Should Be True    ${result}    host_1 should be down/hard
    Ctn Stop Engine
    Ctn Kindly Stop Broker

EFHCU2
    [Documentation]    Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior.
    [Tags]    engine    external_cmd
    Ctn Config Engine    ${1}

    # We force the check command of host_1 to return 2 as status.
    Ctn Config Host Command Status    ${0}    checkh1    2
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Broker Config Log    module0    neb    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    central    sql    debug
    Ctn Config BBDO3    1

    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker
    ${result}    Ctn Check Host Status    host_1    4    1    True
    Should Be True    ${result}    host_1 should be pending
    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    Ctn Process Host Check Result    host_1    0    host_1 UP
    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    ${content}    Create List
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;1;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;SOFT;2;
    ...    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ...    HOST ALERT: host_1;DOWN;HARD;3;

    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Message about SCHEDULE FORCED CHECK and HOST ALERT should be available in log.

    ${result}    Ctn Check Host Status    host_1    1    1    True
    Should Be True    ${result}    host_1 should be down/hard
    Ctn Stop Engine
    Ctn Kindly Stop Broker

EMACROS
    [Documentation]    macros ADMINEMAIL and ADMINPAGER are replaced in check outputs
    [Tags]    engine    external_cmd    macros
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    0    log_level_checks    trace    True
    Ctn Engine Config Change Command
    ...    0
    ...    \\d+
    ...    /bin/echo "ResourceFile: $RESOURCEFILE$ - LogFile: $LOGFILE$ - AdminEmail: $ADMINEMAIL$ - AdminPager: $ADMINPAGER$"
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    Ctn Schedule Forced Service Check    host_1    service_1
    Sleep    5s

    ${content}    Create List
    ...    ResourceFile: /tmp/etc/centreon-engine/config0/resource.cfg - LogFile: /tmp/var/log/centreon-engine/config0/centengine.log - AdminEmail: titus@bidibule.com - AdminPager: admin
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    AdminEmail: titus@bidibule.com - AdminPager: admin not found in log.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

EMACROS_NOTIF
    [Documentation]    macros ADMINEMAIL and ADMINPAGER are replaced in notification commands
    [Tags]    engine    external_cmd    macros
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    0    log_level_checks    trace    True
    Ctn Engine Config Add Value    0    cfg_file    ${EtcRoot}/centreon-engine/config0/contacts.cfg
    Ctn Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /bin/sh -c '/bin/echo "ResourceFile: $RESOURCEFILE$ - LogFile: $LOGFILE$ - AdminEmail: $ADMINEMAIL$ - AdminPager: $ADMINPAGER$" >> /tmp/notif_toto.txt'
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Remove File    /tmp/notif_toto.txt
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    FOR    ${i}    IN RANGE    3
        Ctn Process Service Check Result    host_1    service_1    2    critical
    END

    Wait Until Created    /tmp/notif_toto.txt    30s

    ${grep_res}    Grep File
    ...    /tmp/notif_toto.txt
    ...    ResourceFile: /tmp/etc/centreon-engine/resource.cfg - LogFile: /tmp/var/log/centreon-engine/centengine.log - AdminEmail: titus@bidibule.com - AdminPager: admin

    Ctn Stop Engine
    Ctn Kindly Stop Broker

EMACROS_SEMICOLON
    [Documentation]    Macros with a semicolon are used even if they contain a semicolon.
    [Tags]    engine    external_cmd    macros    MON-15765
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    0    log_level_checks    trace    True
    Ctn Engine Config Set Value In Hosts    0    host_1    _KEY2    VAL1;val3;
    Ctn Engine Config Change Command
    ...    0
    ...    \\d+
    ...    /bin/echo "KEY2=$_HOSTKEY2$"
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    Ctn Schedule Forced Service Check    host_1    service_1
    Sleep    5s

    ${content}    Create List    KEY2=VAL1;val3;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    VAL1;val3; not found in log.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

E_HOST_DOWN_DISABLE_SERVICE_CHECKS
    [Documentation]    host_down_disable_service_checks is set to 1, host down switch all services to UNKNOWN
    [Tags]    engine    MON-32780
    Ctn Config Engine    ${1}  2  20
    Ctn Set Hosts Passive  0  host_.*
    Ctn Engine Config Set Value  0  host_down_disable_service_checks  ${1}  ${True}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}

    Ctn Clear Retention
    ${start}    Get Current Date

    Ctn Start Engine
    Ctn Start Broker  only_central=${True}

    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    FOR    ${i}    IN RANGE    ${4}
        Ctn Process Host Check Result    host_1    1    host_1 DOWN
    END

    ${result}    Ctn Check Host Status    host_1    1    1    False  30
    Should Be True    ${result}    host_1 should be down/hard

    # After some time services should be in hard state
    FOR  ${index}  IN RANGE  ${19}
        ${result}    Ctn Check Service Status With Timeout    host_1  service_${index+1}    3    30  HARD
        Should Be True    ${result}    service_${index+1} should be UNKNOWN hard
    END

    # host_1 check returns UP
    Ctn Set Command Status   checkh1  0
    Ctn Process Host Check Result    host_1    0    host_1 UP

    # After some time services should be in ok hard state
    FOR  ${index}  IN RANGE  ${19}
        Ctn Process Service Check Result  host_1  service_${index+1}  0  output
    END
    FOR  ${index}  IN RANGE  ${19}
        ${result}    Ctn Check Service Status With Timeout    host_1  service_${index+1}    0    30  HARD
        Should Be True    ${result}    service_${index+1} should be OK hard
    END

    [Teardown]  Ctn Stop Engine Broker And Save Logs  only_central=${True}

E_HOST_UNREACHABLE_DISABLE_SERVICE_CHECKS
    [Documentation]    host_down_disable_service_checks is set to 1, host unreachable switch all services to UNKNOWN
    [Tags]    engine    MON-32780
    Ctn Config Engine    ${1}  2  20
    Ctn Engine Config Set Value  0  host_down_disable_service_checks  ${1}  ${True}
    Ctn Engine Config Set Value  0  log_level_runtime  trace
    Ctn Engine Config Set Value  0  log_level_checks  trace
    Ctn Add Parent To Host  0  host_1  host_2
    Ctn Set Hosts Passive  0  host_.*
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}

    Ctn Clear Retention
    ${start}    Get Current Date

    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker  only_central=${True}

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.


    FOR    ${i}    IN RANGE    ${4}
        Ctn Process Host Check Result    host_2    1    host_2 down
    END

    ${result}    Ctn Check Host Status    host_2    1    1    False  30
    Should Be True    ${result}    host_2 should be down/hard

    FOR    ${i}    IN RANGE    ${4}
        Ctn Process Host Check Result    host_1    1    host_1 down
    END

    ${result}    Ctn Check Host Status    host_1    2    1    False  30
    Should Be True    ${result}    host_1 should be unreachable/hard

    #after some time services should be in hard state
    FOR  ${index}  IN RANGE  ${19}
        ${result}    Ctn Check Service Status With Timeout    host_1  service_${index+1}    3    30  HARD
        Should Be True    ${result}    service_${index+1} should be UNKNOWN hard
    END

    [Teardown]  Ctn Stop Engine Broker And Save Logs  only_central=${True}

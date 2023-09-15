*** Settings ***
Documentation       Centreon notification

Resource            ../resources/resources.robot
Library             Process
Library             DatabaseLibrary
Library             OperatingSystem
Library             DateTime
Library             Collections
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save logs If Failed


*** Test Cases ***
not1
    [Documentation]    1 services id configurd and,checking that the non-ok notification is sent.
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${1}    ${1}
    Engine Config Set Value    0    enable_notifications    1    True
    Engine Config Set Value    0    execute_host_checks    1    True
    Engine Config Set Value    0    execute_service_checks    1    True
    Engine Config Set Value    0    log_notifications    1    True
    Engine Config Set Value    0    log_level_notifications    trace    True
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    error
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    Engine Config Add Value    0    cfg_file   ${EtcRoot}/centreon-engine/config0/contacts.cfg
    Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ## Time to set the service to CRITICAL HARD.
    FOR   ${i}    IN RANGE    ${3}
        Process Service Check Result    host_1    service_1    2    critical
        Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;critical
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification is not sent

    Stop Engine
    Kindly Stop Broker

not2
    [Documentation]    1 services id configurd and,checking that the recovery notification is sent.
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${1}    ${1}
    Engine Config Set Value    0    enable_notifications    1    True
    Engine Config Set Value    0    execute_host_checks    1    True
    Engine Config Set Value    0    execute_service_checks    1    True
    Engine Config Set Value    0    log_notifications    1    True
    Engine Config Set Value    0    log_level_notifications    trace    True
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    error
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    Engine Config Add Value    0    cfg_file   ${EtcRoot}/centreon-engine/config0/contacts.cfg
    Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ## Time to set the service to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Process Service Check result    host_1    service_1    2    critical
    Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;critical
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification is sent

    ## Time to set the service to UP  hard

    FOR   ${i}    IN RANGE    ${3}
        Process Service Check result    host_1    service_1    0    ok
    Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_1    service_1    ${0}    60    HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;ok
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification recovery is not sent

    Stop Engine
    Kindly Stop Broker

not3
    [Documentation]    1 services id configurd and,checking that the non-ok notification is sent after the downtimes is finished.
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${1}    ${1}
    Engine Config Set Value    0    enable_notifications    1    True
    Engine Config Set Value    0    execute_host_checks    1    True
    Engine Config Set Value    0    execute_service_checks    1    True
    Engine Config Set Value    0    log_notifications    1    True
    Engine Config Set Value    0    log_level_notifications    trace    True
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    error
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    Engine Config Add Value    0    cfg_file   ${EtcRoot}/centreon-engine/config0/contacts.cfg
    Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # It's time to schedule a downtime
    Schedule Service Downtime    host_1    service_1    ${60}

    ${result}    Check Number Of Downtimes    ${1}    ${start}    ${60}
    Should Be True    ${result}    We should have 1 downtime enabled.

     ## Time to set the service to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Process Service Check result    host_1    service_1    2    critical
    Sleep    10s
    END

    ${result}=    Check Service Status With Timeout    host_1    service_1    ${2}    70    HARD

    # Let's wait for the external command check start
    ${content}    Create List    SERVICE DOWNTIME ALERT: host_1;service_1;STOPPED; Service has exited from a period of scheduled downtime
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling downtime has not finished .

    Process Service Check Result    host_1    service_1    2    critical

    ${result}    Check Service Status With Timeout    host_1    service_1    ${0}    60    HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;critical
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification is not sent

    Stop Engine
    Kindly Stop Broker

not4
    [Documentation]    1 services id configurd and,checking that the non-ok notification is sent when the acknowledge is finished.
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${1}    ${1}
    Engine Config Set Value    0    enable_notifications    1    True
    Engine Config Set Value    0    execute_host_checks    1    True
    Engine Config Set Value    0    execute_service_checks    1    True
    Engine Config Set Value    0    log_notifications    1    True
    Engine Config Set Value    0    log_level_notifications    trace    True
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Config BBDO3    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    error
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    Engine Config Add Value    0    cfg_file   ${EtcRoot}/centreon-engine/config0/contacts.cfg
    Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # Time to set the service to CRITICAL HARD.
    FOR   ${i}    IN RANGE    ${3}
        Process Service Check Result    host_1    service_1    2    critical
    Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD

    # Acknowledge the service with critical status
    Acknowledge Service Problem    host_1    service_1    STICKY

    # Let's wait for the external command check start
    ${content}    Create List    ACKNOWLEDGE_SVC_PROBLEM;host_1;service_1;2;0;0;admin;Service (host_1,service_1) acknowledged
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    Process Service Check Result    host_1    service_1    0    ok

    ${result}    Check Service Status With Timeout    host_1    service_1    ${0}    60    HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;ok
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification is not sent

    Stop Engine
    Kindly Stop Broker

not5
    [Documentation]    2 services with 2 different users beeing notified when the services turn into critical state.
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${2}    ${1}
    Engine Config Set Value    0    enable_notifications    1    True
    Engine Config Set Value    0    execute_host_checks    1    True
    Engine Config Set Value    0    execute_service_checks    1    True
    Engine Config Set Value    0    log_notifications    1    True
    Engine Config Set Value    0    log_level_notifications    trace    True
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    error
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    Engine Config Add Value    0    cfg_file   ${EtcRoot}/centreon-engine/config0/contacts.cfg
    Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Hosts    0    host_2    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_2    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_2    contacts    U2
    Engine Config Set Value In Services    0    service_2    contacts    U2
    Engine Config Set Value In Services    0    service_2    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_2    notifications_enabled    1
    Engine Config Set Value In Services    0    service_2    notification_period    24x7
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    U2    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    U2    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ## Time to set the service to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Process Service Check result    host_1    service_1    2    critical
    Sleep    1s
        Process Service Check result    host_2    service_2    2    critical
    Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    70    HARD

    ${result}    Check Service Status With Timeout    host_2    service_2    ${2}    70    HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;critical
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification is not sent

    ${content}    Create List    SERVICE NOTIFICATION: U2;host_2;service_2;CRITICAL;command_notif;critical
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification is not sent

    Stop Engine
    Kindly Stop Broker

not6
    [Documentation]    timeperiod null
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${1}    ${1}
    Engine Config Set Value    0    enable_notifications    1    True
    Engine Config Set Value    0    execute_host_checks    1    True
    Engine Config Set Value    0    execute_service_checks    1    True
    Engine Config Set Value    0    log_notifications    1    True
    Engine Config Set Value    0    log_level_notifications    trace    True
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    error
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    Engine Config Add Value    0    cfg_file   ${EtcRoot}/centreon-engine/config0/contacts.cfg
    Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.

    ## Time to set the service to CRITICAL HARD.
    FOR   ${i}    IN RANGE    ${3}
        Process Service Check result    host_1    service_1    2    critical
    Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;critical
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification is not sent

    Engine Config Set Value In Services    0    service_1    notification_period    none
    Sleep    5s

    ${start}    Get Current Date
    Reload Broker
    Reload Engine

    ## Time to set the service to UP  hard

    FOR   ${i}    IN RANGE    ${3}
        Process Service Check result    host_1    service_1    0    ok
    Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_1    service_1    ${0}    90    HARD

    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Sleep    5s

    ${result}    Check Service Status With Timeout    host_1    service_1    ${0}    90    HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;ok
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification recovery is not sent

    Stop Engine
    Kindly Stop Broker

not7
    [Documentation]    host alert
    [Tags]    broker    engine    host    hosts    notification
    Config Engine    ${1}    ${1}
    Engine Config Set Value    0    enable_notifications    1    True
    Engine Config Set Value    0    execute_host_checks    1    True
    Engine Config Set Value    0    execute_service_checks    1    True
    Engine Config Set Value    0    log_notifications    1    True
    Engine Config Set Value    0    log_level_notifications    trace    True
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    error
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    Engine Config Add Value    0    cfg_file   ${EtcRoot}/centreon-engine/config0/contacts.cfg
    Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    ## Time to set the host to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Process host check result    host_1    1    host_1 DOWN
    Sleep    1s
    END

    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced HOST CHECK    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST ALERT: host_1;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification is not sent

    Stop Engine
    Kindly Stop Broker

not8
    [Documentation]    critical host notification
    [Tags]    broker    engine    host    notification
    Config Engine    ${1}    ${1}
    Engine Config Set Value    0    enable_notifications    1    True
    Engine Config Set Value    0    execute_host_checks    1    True
    Engine Config Set Value    0    execute_service_checks    1    True
    Engine Config Set Value    0    log_notifications    1    True
    Engine Config Set Value    0    log_level_notifications    trace    True
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    error
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    Engine Config Add Value    0    cfg_file   ${EtcRoot}/centreon-engine/config0/contacts.cfg
    Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    ## Time to set the host to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Process host check result    host_1    1    host_1 DOWN
    Sleep    1s
    END

    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced HOST CHECK    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;host_1 DOWN;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification is not sent

    Stop Engine
    Kindly Stop Broker

not9
   [Documentation]    recovery host notification
    [Tags]    broker    engine    host    notification
    Config Engine    ${1}    ${1}
    Engine Config Set Value    0    enable_notifications    1    True
    Engine Config Set Value    0    execute_host_checks    1    True
    Engine Config Set Value    0    execute_service_checks    1    True
    Engine Config Set Value    0    log_notifications    1    True
    Engine Config Set Value    0    log_level_notifications    trace    True
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    error
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    Engine Config Add Value    0    cfg_file   ${EtcRoot}/centreon-engine/config0/contacts.cfg
    Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    ## Time to set the host to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Process host check result    host_1    1    host_1 DOWN
    Sleep    1s
    END

    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced HOST CHECK    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;RECOVERY (UP);command_notif;Host
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification is not sent

    Stop Engine
    Kindly Stop Broker

not10
    [Documentation]    downtime on a down host and after the downtimes is finished and the host is still critical we should recieve a critical notification
    [Tags]    broker    engine    host    notification
    Config Engine    ${1}    ${1}    ${1}
    Engine Config Set Value    0    enable_notifications    1    True
    Engine Config Set Value    0    execute_host_checks    1    True
    Engine Config Set Value    0    execute_service_checks    1    True
    Engine Config Set Value    0    log_notifications    1    True
    Engine Config Set Value    0    log_level_notifications    trace    True
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    error
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    Engine Config Add Value    0    cfg_file   ${EtcRoot}/centreon-engine/config0/contacts.cfg
    Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    ## Time to set the host to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Process host check result    host_1    0    host_1 UP
    Sleep    1s
    END

    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced HOST CHECK    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END


    Schedule host downtime    ${0}    host_1    ${3600}

    Sleep    10s

    FOR   ${i}    IN RANGE    ${3}
        Process host check result    host_1    1    host_1 DOWN
    Sleep    1s
    END

    Delete host downtimes    ${0}    host_1

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;host_1 DOWN;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification is not sent

    Stop Engine
    Kindly Stop Broker

not11
    [Documentation]    downtime on a down host that already had a critical notification then put it on up state after the downtimes is finished and the host is up we should recieve a recovery notification
    [Tags]    broker    engine    host    notification
    Config Engine    ${1}    ${1}    ${1}
    Engine Config Set Value    0    enable_notifications    1    True
    Engine Config Set Value    0    execute_host_checks    1    True
    Engine Config Set Value    0    execute_service_checks    1    True
    Engine Config Set Value    0    log_notifications    1    True
    Engine Config Set Value    0    log_level_notifications    trace    True
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    error
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    Engine Config Add Value    0    cfg_file   ${EtcRoot}/centreon-engine/config0/contacts.cfg
    Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    ## Time to set the host to UP HARD.

    FOR   ${i}    IN RANGE    ${3}
        Process host check result    host_1    1    host_1 DOWN
    Sleep    1s
    END

    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced HOST CHECK    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END


    Schedule host downtime    ${0}    host_1    ${3600}

    Sleep    10s
    ## Time to set the host to CRITICAL HARD.
    FOR   ${i}    IN RANGE    ${3}
        Process host check result    host_1    0    host_1 UP
    Sleep    1s
    END

    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced HOST CHECK    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    Delete host downtimes    ${0}    host_1

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;RECOVERY (UP);command_notif;Host
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification is not sent

    Stop Engine
    Kindly Stop Broker


not12
    [Documentation]    1 services id configurd and,checking that the non-ok notification is sent.
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${1}    ${1}
    Engine Config Set Value    0    enable_notifications    1    True
    Engine Config Set Value    0    execute_host_checks    1    True
    Engine Config Set Value    0    execute_service_checks    1    True
    Engine Config Set Value    0    log_notifications    1    True
    Engine Config Set Value    0    log_level_notifications    trace    True
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    error
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    Engine Config Add Value    0    cfg_file   ${EtcRoot}/centreon-engine/config0/contacts.cfg
    Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ## Time to set the service to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Process Service Check result    host_1    service_1    2    critical
    Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD

    ${content}    Create List    SERVICE ALERT: host_1;service_1;CRITICAL;SOFT;1;critical
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that an alert is sent

    ${content}    Create List    SERVICE ALERT: host_1;service_1;CRITICAL;SOFT;2;critical
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that an alert is sent

    ${content}    Create List    SERVICE ALERT: host_1;service_1;CRITICAL;HARD;3;critical
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that an alert is sent

    Stop Engine
    Kindly Stop Broker


not13
    [Documentation]    escalations
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${2}    ${1}
    Engine Config Set Value    0    enable_notifications    1    True
    Engine Config Set Value    0    execute_host_checks    1    True
    Engine Config Set Value    0    execute_service_checks    1    True
    Engine Config Set Value    0    log_notifications    1    True
    Engine Config Set Value    0    log_level_notifications    trace    True
    Engine Config Set Value    0    interval_length    10    True
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    error
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    Engine Config Add Value    0    cfg_file   ${EtcRoot}/centreon-engine/config0/contacts.cfg
    Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Add service Group    ${0}    ${1}    ["host_1","service_1", "host_2","service_2"]
    Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true
    Engine Config Set Value In Services    0    service_1    notification_options    c
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    first_notification_delay    0
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Services    0    service_1    contact_groups    contactgroup_1
    Engine Config Replace Value In Services    0    service_1    active_checks_enabled    0
    Engine Config Replace Value In Services    0    service_1    max_check_attempts     1
    Engine Config Replace Value In Services    0    service_1    retry_interval     1
    Engine Config Set Value In Services    0    service_1    notification_interval    1
    Engine Config Replace Value In Services    0    service_1    check_interval     1
    Engine Config Replace Value In Services    0    service_1    check_command    command_4
    Engine Config Set Value In Services    0    service_2    contact_groups    contactgroup_1
    Engine Config Replace Value In Services    0    service_2    max_check_attempts     1
    Engine Config Set Value In Services    0    service_2    notification_options    c
    Engine Config Set Value In Services    0    service_2    notifications_enabled    1
    Engine Config Set Value In Services    0    service_2    first_notification_delay    0
    Engine Config Set Value In Services    0    service_2    notification_period    24x7
    Engine Config Set Value In Services    0    service_2    notification_interval    1
    Engine Config Replace Value In Services    0    service_2    first_notification_delay    0
    Engine Config Replace Value In Services    0    service_2    check_interval     1
    Engine Config Replace Value In Services    0    service_2    active_checks_enabled    0
    Engine Config Replace Value In Services    0    service_2    retry_interval     1
    Engine Config Replace Value In Services    0    service_2    check_command    command_4
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Add Contact Group    ${0}    ${1}    ["U1"]
    Add Contact Group    ${0}    ${2}    ["U2","U3"]
    Add Contact Group    ${0}    ${3}    ["U4"]

    Create Escalations File    0    1    servicegroup_1    contactgroup_2
    Create Escalations File    0    2    servicegroup_1    contactgroup_3

    Engine Config Set Value In Escalations    0    esc1    first_notification    2
    Engine Config Set Value In Escalations    0    esc1    last_notification    2
    Engine Config Set Value In Escalations    0    esc1    notification_interval    1
    Engine Config Set Value In Escalations    0    esc2    first_notification    3
    Engine Config Set Value In Escalations    0    esc2    last_notification    0
    Engine Config Set Value In Escalations    0    esc2    notification_interval    1

    ${start}    Get Current Date
    Start Broker
    Start Engine

  # Let's wait for the external command check start
    ${content}    Create List    check
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.


    FOR   ${i}    IN RANGE    ${4}
        Process Service Check result    host_1    service_1    2    critical
    Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD

    FOR   ${i}    IN RANGE    ${4}
        Process Service Check result    host_2    service_2    2    critical
    Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_2    service_2    ${2}    60    HARD

    # Let's wait for the first notification of the user U1
    ${content}    Create List    SERVICE NOTIFICATION: U1;host_1;service_1;CRITICAL;command_notif;critical_0;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification is not sent
    # Let's wait for the first notification of the contact group 1
    ${content}    Create List    SERVICE NOTIFICATION: U1;host_2;service_2;CRITICAL;command_notif;critical_0;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification is not sent

    Process Service Check result    host_2    service_2    2    critical
    Process Service Check result    host_1    service_1    2    critical

    ${result}    Check Service Status With Timeout    host_2    service_2    ${2}    60    HARD
    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD

    Process Service Check result    host_2    service_2    2    critical
    Process Service Check result    host_1    service_1    2    critical

    ${result}    Check Service Status With Timeout    host_2    service_2    ${2}    60    HARD
    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD

    Process Service Check result    host_2    service_2    2    critical
    Process Service Check result    host_1    service_1    2    critical

    ${result}    Check Service Status With Timeout    host_2    service_2    ${2}    60    HARD
    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD

    ${result}    Check Service Status With Timeout    host_2    service_2    ${2}    60    HARD
    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD

    # Let's wait for the first notification of the contact group 2 U3 ET U2

    ${content}    Create List     SERVICE NOTIFICATION: U2;host_1;service_1;CRITICAL;command_notif;critical_0;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    A message telling that notification is not sent

    ${content}    Create List    SERVICE NOTIFICATION: U3;host_1;service_1;CRITICAL;command_notif;critical_0;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    A message telling that notification is not sent

    FOR   ${i}    IN RANGE    ${4}
        Process Service Check result    host_1    service_1    2    critical
    Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD

    FOR   ${i}    IN RANGE    ${4}
        Process Service Check result    host_2    service_2    2    critical
    Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_2    service_2    ${2}    60    HARD

    # Let's wait for the second notification of the contact group 2 U3 ET U2

    ${content}    Create List    SERVICE NOTIFICATION: U2;host_2;service_2;CRITICAL;command_notif;critical_0;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    A message telling that notification is not sent

    ${content}    Create List    SERVICE NOTIFICATION: U3;host_2;service_2;CRITICAL;command_notif;critical_0;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    A message telling that notification is not sent

    FOR   ${i}    IN RANGE    ${4}
        Process Service Check result    host_1    service_1    2    critical
    Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD

    FOR   ${i}    IN RANGE    ${4}
        Process Service Check result    host_2    service_2    2    critical
    Sleep    1s
    END

    # Let's wait for the first notification of the contact group 3 U4

    ${content}    Create List    SERVICE NOTIFICATION: U4;host_1;service_1;CRITICAL;command_notif;critical_0;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    A message telling that notification is not sent

    ${content}    Create List    SERVICE NOTIFICATION: U4;host_2;service_2;CRITICAL;command_notif;critical_0;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    A message telling that notification is not sent
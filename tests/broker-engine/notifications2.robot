*** Settings ***
Documentation       Centreon notification

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***

not10
    [Documentation]    This test case involves scheduling downtime on a down host that already had
    ...    a critical notification. When The Host return to UP state we should receive a recovery
    ...    notification.
    [Tags]    broker    engine    host    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Notifications
    Ctn Config Host Command Status    ${0}    checkh1    2
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    Ctn Broker Config Log    module0    core    trace

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ## Time to set the host to CRITICAL HARD.
    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    Ctn Schedule Host Downtime    ${0}    host_1    ${60}
    ${content}    Create List    Notifications for the service will not be sent out during that time period.
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
    Should Be True    ${result}    The downtime has not been sent.

    Ctn Process Host Check Result    host_1    2    host_1 DOWN

    ${content}    Create List    We shouldn't notify about DOWNTIME events for this notifier
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The down notification of host_1 is sent

    Ctn Delete Host Downtimes    ${0}    host_1
    ${content}    Create List    cmd_delete_downtime_full() args = host_1
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
    Should Be True    ${result}    Downtimes not removed in host_1

    ## Time to set the host to UP HARD.
    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The down notification of host_1 is not sent

    ## Time to set the host to UP HARD.
    ${start}    Ctn Get Round Current Date

    Ctn Process Host Check Result    host_1    0    host_1 UP

    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    Ctn Process Host Check Result    host_1    0    host_1 UP

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;RECOVERY (UP);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The recovery notification of host_1 is not sent

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not11
    [Documentation]    This test case involves configuring one service and checking that three alerts are sent for it.
    [Tags]    broker    engine    services    hosts    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_1}    ${2}

    ## Time to set the service to CRITICAL HARD.

    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${content}    Create List    SERVICE ALERT: host_1;service_1;CRITICAL;SOFT;1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The first service alert SOFT1 is not sent

    ${content}    Create List    SERVICE ALERT: host_1;service_1;CRITICAL;SOFT;2;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The second service alert SOFT2 is not sent

    ${content}    Create List    SERVICE ALERT: host_1;service_1;CRITICAL;HARD;3;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The third service alert hard is not sent

    Ctn Stop Engine
    Ctn Kindly Stop Broker


not12
    [Documentation]    Escalations
    [Tags]    broker    engine    services    hosts    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${2}    ${1}
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Engine Config Set Value    0    interval_length    1    True
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Ctn Engine Config Set Value    ${0}    log_level_config    trace
    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1", "host_2","service_2"]
    Ctn Config Notifications
    Ctn Config Escalations

    Ctn Add Contact Group    ${0}    ${1}    ["U1"]
    Ctn Add Contact Group    ${0}    ${2}    ["U2","U3"]
    Ctn Add Contact Group    ${0}    ${3}    ["U4"]

    Ctn Create Escalations File    0    1    servicegroup_1    contactgroup_2
    Ctn Create Escalations File    0    2    servicegroup_1    contactgroup_3

    Ctn Engine Config Set Value In Escalations    0    esc1    first_notification    2
    Ctn Engine Config Set Value In Escalations    0    esc1    last_notification    2
    Ctn Engine Config Set Value In Escalations    0    esc1    notification_interval    1
    Ctn Engine Config Set Value In Escalations    0    esc2    first_notification    3
    Ctn Engine Config Set Value In Escalations    0    esc2    last_notification    0
    Ctn Engine Config Set Value In Escalations    0    esc2    notification_interval    1

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    ${cmd_service_2}    Ctn Get Service Command Id    ${2}
    Ctn Set Command Status    ${cmd_service_1}    ${2}
    Ctn Set Command Status    ${cmd_service_2}    ${2}

    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    Ctn Process Service Result Hard    host_2    service_2    ${2}    The service_2 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${result}    Ctn Check Service Resource Status With Timeout    host_2    service_2    ${2}    60    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD

    # Let's wait for the first notification of the user U1
    ${content}    Create List    SERVICE NOTIFICATION: U1;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The first notification of U1 is not sent
    # Let's wait for the first notification of the contact group 1
    ${content}    Create List    SERVICE NOTIFICATION: U1;host_2;service_2;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The first notification of contact group 1 is not sent

    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    Ctn Process Service Result Hard    host_2    service_2    ${2}    The service_2 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${result}    Ctn Check Service Resource Status With Timeout    host_2    service_2    ${2}    60    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD

    # Let's wait for the first notification of the contact group 2 U3 ET U2
    ${content}    Create List     SERVICE NOTIFICATION: U2;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The first notification of U2 is not sent

    ${content}    Create List    SERVICE NOTIFICATION: U3;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The first notification of U3 is not sent

    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    Ctn Process Service Result Hard    host_2    service_2    ${2}    The service_2 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${result}    Ctn Check Service Resource Status With Timeout    host_2    service_2    ${2}    60    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD

    # Let's wait for the second notification of the contact group 2 U3 ET U2
    ${content}    Create List    SERVICE NOTIFICATION: U2;host_2;service_2;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The second notification of U2 is not sent

    ${content}    Create List    SERVICE NOTIFICATION: U3;host_2;service_2;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The second notification of U3 is not sent

    ${start}    Ctn Get Round Current Date
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    Ctn Process Service Result Hard    host_2    service_2    ${2}    The service_1 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${result}    Ctn Check Service Resource Status With Timeout    host_2    service_2    ${2}    60    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD

    # Let's wait for the first notification of the contact group 3 U4
    ${content}    Create List    SERVICE NOTIFICATION: U4;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The first notification of U4 is not sent

    ${content}    Create List    SERVICE NOTIFICATION: U4;host_2;service_2;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The second notification of U4 is not sent

not13
    [Documentation]    notification for a dependencies host
    [Tags]    broker    engine    host
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${2}    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_config    trace
    Ctn Config Notifications
    Ctn Config Engine Add Cfg File    ${0}    dependencies.cfg
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r,s
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Hosts    0    host_1    first_notification_delay    0
    Ctn Engine Config Set Value In Hosts    0    host_1    recovery_notification_delay    0
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_interval    0
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    n
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_1    first_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_1    recovery_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_1    notification_interval    0
    Ctn Engine Config Set Value In Hosts    0    host_2    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_2    notification_options    d,r,s
    Ctn Engine Config Set Value In Hosts    0    host_2    contacts    John_Doe
    Ctn Engine Config Set Value In Hosts    0    host_2    first_notification_delay    0
    Ctn Engine Config Set Value In Hosts    0    host_2    recovery_notification_delay    0
    Ctn Engine Config Set Value In Hosts    0    host_2    notification_interval    0
    Ctn Engine Config Set Value In Services    0    service_2    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_2    notification_options    n
    Ctn Engine Config Set Value In Services    0    service_2    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_2    notification_period    24x7
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    Ctn Config Host Command Status    ${0}    checkh1    2
    Ctn Config Host Command Status    ${0}    checkh2    2

    Ctn Create Dependencieshst File    0    host_2    host_1

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

     ## Time to set the host to CRITICAL HARD.

    FOR    ${i}    IN RANGE    ${3}
        Ctn Schedule Forced Host Check    host_2    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_2;DOWN;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_2 is not sent

    ${start}    Ctn Get Round Current Date
    Ctn Process Host Check Result    host_2    0    host_2 UP

    FOR    ${i}    IN RANGE    ${3}
        Ctn Schedule Forced Host Check    host_2    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_2;RECOVERY (UP);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_2 is not sent

    FOR    ${i}    IN RANGE    ${3}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_1 is not sent

    ${new_date}    Get Current Date

    Ctn Process Host Check Result    host_2    1    host_2 DOWN

    FOR    ${i}    IN RANGE    ${3}
        Ctn Schedule Forced Host Check    host_2    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    This notifier won't send any notification since it depends on another notifier that has already sent one
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${new_date}    ${content}    60
    Should Be True    ${result}    The down notification of host_2 is sent dependency not working

    Ctn Process Host Check Result    host_1    0    host_1 UP

    FOR    ${i}    IN RANGE    ${3}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;RECOVERY (UP);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${new_date}    ${content}    60
    Should Be True    ${result}    The down notification of host_1 is not sent

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not14
    [Documentation]    notification for a Service dependency
    [Tags]    broker    engine    services    unified_sql
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${2}    ${1}
    Ctn Config Notifications
    Ctn Config Engine Add Cfg File    ${0}    dependencies.cfg
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    n
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_1    first_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_1    recovery_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_1    notification_interval    0
    Ctn Engine Config Set Value In Hosts    0    host_2    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_2    notification_options    n
    Ctn Engine Config Set Value In Hosts    0    host_2    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_2    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_2    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_2    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_2    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_2    first_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_2    recovery_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_2    notification_interval    0
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Ctn Create Dependencies File    0    host_2    host_1    service_2    service_1

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    ${cmd_service_2}    Ctn Get Service Command Id    ${2}
    Ctn Set Command Status    ${cmd_service_2}    ${2}

    ## Time to set the service2 to CRITICAL HARD.
    Ctn Process Service Result Hard    host_2    service_2    ${2}    The service_2 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_2    service_2    ${2}    60    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_2;service_2;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent

    ## Time to set the service2 to OK  hard
    ${start}    Ctn Get Round Current Date
    Ctn Set Command Status    ${cmd_service_2}    ${0}

    Ctn Process Service Result Hard    host_2    service_2    ${0}    The service_2 is OK

    ${result}    Ctn Check Service Resource Status With Timeout    host_2    service_2    ${0}    60    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be OK HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_2;service_2;RECOVERY (OK);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent

   ## Time to set the service1 to CRITICAL HARD.
    ${start}    Ctn Get Round Current Date
    Ctn Set Command Status    ${cmd_service_1}    ${2}

    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent

    ${new_date}    Get Current Date
    ## Time to set the service2 to CRITICAL HARD.
    Ctn Set Command Status    ${cmd_service_2}    ${2}

    Ctn Process Service Result Hard    host_2    service_2    ${2}    The service_2 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_2    service_2    ${2}    60    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD

    ${content}    Create List    This notifier won't send any notification since it depends on another notifier that has already sent one
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${new_date}    ${content}    60
    Should Be True    ${result}     The dependency not working and the service_é has recieved a notification

    ## Time to set the service1 to OK  hard
    Ctn Set Command Status    ${cmd_service_1}    ${0}

    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be OK HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${new_date}    ${content}    60
    Should Be True    ${result}    The notification is not sent

    Ctn Stop Engine
    Ctn Kindly Stop Broker


not15
    [Documentation]    several notification commands for the same user.
    [Tags]    broker    engine    services    unified_sql
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Notifications
    Ctn Engine Config Add Command
    ...    0
    ...    command_notif1
    ...    /usr/bin/false
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif,command_notif1
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif,command_notif1

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_1}    ${2}

    ## Time to set the service to CRITICAL HARD.

    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that notification is not sent

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not16
    [Documentation]    notification for dependencies services group
    [Tags]    broker    engine    services    unified_sql
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${4}    ${1}
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Engine Config Set Value    0    interval_length    1    True
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Ctn Add service Group    ${0}    ${1}    ["host_1","service_1", "host_2","service_2"]
    Ctn Add service Group    ${0}    ${2}    ["host_3","service_3", "host_4","service_4"]
    Ctn Config Notifications
    Ctn Config Engine Add Cfg File    ${0}    dependencies.cfg
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    n
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_1    first_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_1    recovery_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_1    notification_interval    0
    Ctn Engine Config Set Value In Hosts    0    host_2    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_2    notification_options    n
    Ctn Engine Config Set Value In Hosts    0    host_2    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_2    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_2    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_2    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_2    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_2    first_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_2    recovery_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_2    notification_interval    0
    Ctn Engine Config Set Value In Hosts    0    host_3    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_3    notification_options    n
    Ctn Engine Config Set Value In Hosts    0    host_3    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_3    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_3    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_3    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_3    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_3    first_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_3    recovery_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_3    notification_interval    0
    Ctn Engine Config Set Value In Hosts    0    host_4    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_4    notification_options    n
    Ctn Engine Config Set Value In Hosts    0    host_4    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_4    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_4    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_4    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_4    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_4    first_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_4    recovery_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_4    notification_interval    0
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Ctn Create Dependenciesgrp File    0    servicegroup_2    servicegroup_1

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

    ## Time to set the service3 to CRITICAL HARD.

    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    ${cmd_service_3}    Ctn Get Service Command Id    ${3}
    ${cmd_service_4}    Ctn Get Service Command Id    ${4}


    Ctn Process Service Result Hard    host_3    service_3    ${0}    The service_3 is OK

    ${result}    Ctn Check Service Resource Status With Timeout    host_3    service_3    ${0}    60    HARD
    Should Be True    ${result}    Service (host_3,service_3) should be OK HARD

    ##Time to set the service3 to CRITICAL HARD.
    ${start}    Ctn Get Round Current Date

    Ctn Process Service Result Hard    host_3    service_3    ${2}    The service_3 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_3    service_3    ${2}    60    HARD
    Should Be True    ${result}    Service (host_3,service_3) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_3;service_3;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The notification is not sent for service3

    ## Time to set the service3 to OK hard
    ${start}    Ctn Get Round Current Date

    Ctn Process Service Result Hard    host_3    service_3    ${0}    The service_3 is OK

    ${result}    Ctn Check Service Resource Status With Timeout    host_3    service_3    ${0}    60    HARD
    Should Be True    ${result}    Service (host_3,service_3) should be OK HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_3;service_3;RECOVERY (OK);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The notification is not sent for service3

    ## Time to set the service1 to CRITICAL HARD.
    ${start}    Ctn Get Round Current Date
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The notification is not sent for service1

    ## Time to set the service3 to CRITICAL HARD.
    ${start}    Ctn Get Round Current Date

    Ctn Process Service Result Hard    host_3    service_3    ${2}    The service_3 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_3    service_3    ${2}    90    HARD
    Should Be True    ${result}    Service (host_3,service_3) should be CRITICAL HARD

    ${content}    Create List    This notifier won't send any notification since it depends on another notifier that has already sent one
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The notification is sent for service3: dependency not working

    ## Time to set the service4 to CRITICAL HARD.
    ${start}    Ctn Get Round Current Date

    Ctn Process Service Result Hard    host_4    service_4    ${2}    The service_4 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_4    service_4    ${2}    60    HARD
    Should Be True    ${result}    Service (host_4,service_4) should be CRITICAL HARD


    ${content}    Create List    This notifier won't send any notification since it depends on another notifier that has already sent one
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The notification is sent for service4: dependency not working

    ## Time to set the service1 to OK hard
    ${start}    Ctn Get Round Current Date

    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be OK HARD


    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The notification is not sent for service1

    Ctn Stop Engine
    Ctn Kindly Stop Broker


not17
    [Documentation]    notification for a dependensies host group
    [Tags]    broker    engine    host    unified_sql
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${4}    ${0}
    Ctn Engine Config Set Value    0    interval_length    10    True
    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2"]
    Ctn Add Host Group    ${0}    ${2}    ["host_3", "host_4"]
    Ctn Config Notifications
    Ctn Config Engine Add Cfg File    ${0}    dependencies.cfg
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r,s
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Hosts    0    host_1    first_notification_delay    0
    Ctn Engine Config Set Value In Hosts    0    host_1    recovery_notification_delay    0
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_interval    0
    Ctn Engine Config Set Value In Hosts    0    host_2    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_2    notification_options    d,r,s
    Ctn Engine Config Set Value In Hosts    0    host_2    contacts    John_Doe
    Ctn Engine Config Set Value In Hosts    0    host_2    first_notification_delay    0
    Ctn Engine Config Set Value In Hosts    0    host_2    recovery_notification_delay    0
    Ctn Engine Config Set Value In Hosts    0    host_2    notification_interval    0
    Ctn Engine Config Set Value In Hosts    0    host_3    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_3    notification_options    d,r,s
    Ctn Engine Config Set Value In Hosts    0    host_3    contacts    John_Doe
    Ctn Engine Config Set Value In Hosts    0    host_3    first_notification_delay    0
    Ctn Engine Config Set Value In Hosts    0    host_3    recovery_notification_delay    0
    Ctn Engine Config Set Value In Hosts    0    host_3    notification_interval    0
    Ctn Engine Config Set Value In Hosts    0    host_4    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_4    notification_options    d,r,s
    Ctn Engine Config Set Value In Hosts    0    host_4    contacts    John_Doe
    Ctn Engine Config Set Value In Hosts    0    host_4    first_notification_delay    0
    Ctn Engine Config Set Value In Hosts    0    host_4    recovery_notification_delay    0
    Ctn Engine Config Set Value In Hosts    0    host_4    notification_interval    0
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Ctn Create Dependencieshstgrp File    0    hostgroup_2    hostgroup_1

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

    # Time to set the host to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_3    1    host_3 DOWN
        Sleep    1s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_3;DOWN;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_3 is not sent

    ${start}    Ctn Get Round Current Date
    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_3    0    host_3 UP
        Sleep    1s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_3;RECOVERY (UP);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The recovery notification of host_3 is not sent

    ${start}    Ctn Get Round Current Date
    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    1    host_1 DOWN
        Sleep    1s
    END
    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_3    1    host_3 DOWN
        Sleep    1s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_1 is not sent

    ${new_date}    Get Current Date

    ${content}    Create List    This notifier won't send any notification since it depends on another notifier that has already sent one
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${new_date}    ${content}    90
    Should Be True    ${result}    The down notification of host_3 is sent dependency not working

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_4    1    host_4 DOWN
        Sleep    1s
    END

    ${content}    Create List    This notifier won't send any notification since it depends on another notifier that has already sent one
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${new_date}    ${content}    60
    Should Be True    ${result}    The down notification of host_4 is sent dependency not working

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    0    host_1 UP
        Sleep    1s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;RECOVERY (UP);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${new_date}    ${content}    60
    Should Be True    ${result}    The recovery notification of host_1 is not sent

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not18
    [Documentation]    notification delay where first notification delay equal retry check
    [Tags]    broker    engine    services    hosts    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Engine Config Set Value    0    interval_length    1    True
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_1    first_notification_delay    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_1}    ${2}

    #let time to first check to be done in order to not do following checks at the same timestamp
    Sleep    5

    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No notification has been sent concerning a critical service

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not19
    [Documentation]    notification delay where first notification delay greater than retry check 
    [Tags]    broker    engine    services    hosts    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Engine Config Set Value    0    interval_length    1    True
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_1    first_notification_delay    3
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    0    service_1    retry_interval    2
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_1}    ${2}

    #let time to first check to be done in order to not do following checks at the same timestamp
    Sleep    5

    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    100
    Should Be True    ${result}    No notification has been sent concerning a critical service

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not20
    [Documentation]    notification delay where first notification delay samller than retry check
    [Tags]    broker    engine    services    hosts    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Engine Config Set Value    0    interval_length    1    True
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_1    first_notification_delay    1
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    0    service_1    retry_interval    2
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_1}    ${2}

    #let time to first check to be done in order to not do following checks at the same timestamp
    Sleep    5

    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No notification has been sent concerning a critical service

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not_in_timeperiod_without_send_recovery_notifications_anyways
    [Documentation]    This test case configures a single service and verifies that a notification is sent when the service is in a non-OK state and OK is not sent outside timeperiod when _send_recovery_notifications_anyways is not set
    [Tags]    MON-33121  broker    engine    services    hosts    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    short_time_period
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    ${cmd_1}    Ctn Get Service Command Id    1
    Log To Console    service_1 has command id ${cmd_1}
    Ctn Set Command Status    ${cmd_1}    2

    ${start}    Get Current Date
    Ctn Create Single Day Time Period    0    short_time_period    ${start}    2

    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${start}

    ## Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard    host_1    service_1    2    critical

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;critical
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent

    Sleep    3m
    Ctn Set Command Status    ${cmd_1}    0
    Ctn Process Service Check Result    host_1    service_1    0    ok

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;ok
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Not Be True    ${result}    The notification is sent out of time period

not_in_timeperiod_with_send_recovery_notifications_anyways
    [Documentation]    This test case configures a single service and verifies that a notification is sent when the service is in a non-OK state and OK is sent outside timeperiod when _send_recovery_notifications_anyways is set
    [Tags]    MON-33121   broker    engine    services    hosts    notification    mon-33121
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    short_time_period
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    Create File    /tmp/centengine_extend.json    {"send_recovery_notifications_anyways": true}

    ${cmd_1}    Ctn Get Service Command Id    1
    Log To Console    service_1 has command id ${cmd_1}
    Ctn Set Command Status    ${cmd_1}    2

    ${start}    Get Current Date
    Ctn Create Single Day Time Period    0    short_time_period    ${start}    2

    Ctn Start Broker
    Ctn Start Engine With Extend Conf

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${start}

    ## Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard    host_1    service_1    2    critical

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;critical
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent

    Sleep    3m
    Ctn Set Command Status    ${cmd_1}    0
    Ctn Process Service Check Result    host_1    service_1    0    ok

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;ok
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent outside time period

not21
    [Documentation]    No notifications should be sent for UNKNOWN services when host goes DOWN.
    ...    When a host transitions to DOWN (soft or hard), services become UNKNOWN due to
    ...    host unreachability. These services should not trigger notifications.
    [Tags]    broker    engine    host    services    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${3}
    Ctn Config Notifications
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Set Value    0    host_down_disable_service_checks    1    ${True}
    
    # Configure host with notifications enabled
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Hosts    0    host_1    first_notification_delay    0
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_interval    1
    Ctn Engine Config Replace Value In Hosts    0    host_1    max_check_attempts    2
    Ctn Engine Config Replace Value In Hosts    0    host_1    check_interval    1
    Ctn Engine Config Replace Value In Hosts    0    host_1    retry_interval    1
    Ctn Engine Config Replace Value In Hosts    0    host_1    check_command    command_1
    
    # Configure services with notifications enabled
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,u,c
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_1    first_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_1    notification_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    max_check_attempts    2
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    check_command    command_2

    
    Ctn Engine Config Set Value In Services    0    service_2    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_2    notification_options    w,u,c
    Ctn Engine Config Set Value In Services    0    service_2    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_2    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_2    first_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_2    notification_interval    1
    Ctn Engine Config Replace Value In Services    0    service_2    max_check_attempts    2
    Ctn Engine Config Replace Value In Services    0    service_2    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_2    retry_interval    1
    Ctn Engine Config Replace Value In Services    0    service_2    check_command    command_3
    
    Ctn Engine Config Set Value In Services    0    service_3    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_3    notification_options    w,u,c
    Ctn Engine Config Set Value In Services    0    service_3    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_3    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_3    first_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_3    notification_interval    1
    Ctn Engine Config Replace Value In Services    0    service_3    max_check_attempts    2
    Ctn Engine Config Replace Value In Services    0    service_3    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_3    retry_interval    1
    Ctn Engine Config Replace Value In Services    0    service_3    check_command    command_4
    
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    Ctn Broker Config Log    module0    core    warning
    Ctn Broker Config Log    module0    bbdo    warning
    Ctn Broker Config Log    module0    neb    warning
    Ctn Engine Config Set Value    0    log_level_functions    warning
    Ctn Engine Config Set Value    0    log_level_notifications    info
    Ctn Engine Config Set Value    0    log_level_checks    info
    

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Log To Console    Waiting for engine to be ready...
    # Ctn Wait For Engine To Be Ready    ${start}    ${1}
    
    # Get command IDs and set initial statuses
    ${cmd_host_1}    Set Variable    1
    ${cmd_service_1}    Set Variable    2
    ${cmd_service_2}    Set Variable    3
    ${cmd_service_3}    Set Variable    4
    
    # Set service_2 to WARNING and service_3 to CRITICAL
    Log To Console    Setting service command statuses: service_1=OK, service_2=WARNING, service_3=CRITICAL
    Ctn Set Command Status    ${cmd_service_1}    ${0}
    Ctn Set Command Status    ${cmd_service_2}    ${1}
    Ctn Set Command Status    ${cmd_service_3}    ${2}
    
    # Host is initially UP
    Ctn Set Command Status    ${cmd_host_1}    ${0}
    

    # Verify services are initially OK
    Log To Console    Checking service_1 status (should be OK)...
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be OK

    Log To Console    Checking service_2 status (should be WARNING HARD)...
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_2    ${1}    60    HARD
    Should Be True    ${result}    Service (host_1,service_2) should be WARNING HARD
    
    Log To Console    Checking service_3 status (should be CRITICAL HARD)...
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_3    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_3) should be CRITICAL HARD

    # Verify notifications are sent for WARNING and CRITICAL states
    Log To Console    Checking for WARNING notification for service_2...
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_2;WARNING;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    WARNING notification should be sent for service_2

    Log To Console    Checking for CRITICAL notification for service_3...
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_3;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    CRITICAL notification should be sent for service_3


    # host to DOWN SOFT
    Log To Console    ===== Testing host DOWN SOFT state =====
    ${start}    Get Current Date
    Log To Console    Setting host command status to DOWN...
    Ctn Set Command Status    ${cmd_host_1}    ${2}
    
    # Wait for host to be checked and go DOWN SOFT
    Log To Console    Checking for host DOWN SOFT alert...
    ${content}    Create List    HOST ALERT: host_1;DOWN;SOFT;1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Host should be in DOWN SOFT state


    # Verify services are in UNKNOWN state (due to host DOWN)
    Log To Console    Checking service_1 for UNKNOWN state...
    ${content}    Create List    SERVICE ALERT:.*service_1;UNKNOWN;
    ${result}    ${msg}    Ctn Find Regex In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Service service_1 should be in UNKNOWN state

    Log To Console    Checking for UNKNOWN notifications during DOWN (should be none)
    # Verify NO service UNKNOWN notifications are sent during SOFT DOWN
    ${content}    Create List    SERVICE NOTIFICATION:.*service_1;UNKNOWN
    ${result}    ${msg}    Ctn Find Regex In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Not Be True    ${result}    No UNKNOWN notification should be sent for service_1 during host DOWN

    ${content}    Create List    SERVICE NOTIFICATION:.*service_2;UNKNOWN
    ${result}    ${msg}    Ctn Find Regex In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Not Be True    ${result}    No UNKNOWN notification should be sent for service_2 during host DOWN

    ${content}    Create List    SERVICE NOTIFICATION:.*service_3;UNKNOWN
    ${result}    ${msg}    Ctn Find Regex In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Not Be True    ${result}    No UNKNOWN notification should be sent for service_3 during host DOWN

    Log To Console    ===== Test completed successfully =====

    Ctn Stop Engine
    Ctn Kindly Stop Broker

*** Keywords ***
Ctn Config Notifications
    [Documentation]    Configuring engine notification settings.
    Ctn Engine Config Set Value    0    enable_notifications    1    True
    Ctn Engine Config Set Value    0    execute_host_checks    1    True
    Ctn Engine Config Set Value    0    execute_service_checks    1    True
    Ctn Engine Config Set Value    0    log_notifications    1    True
    Ctn Engine Config Set Value    0    log_level_notifications    trace    True
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.1
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.1
    Ctn Broker Config Add Item    central    bbdo_version    3.0.1
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    tcp    error
    Ctn Broker Config Log    central    sql    error
    Ctn Broker Config Log    module0    processing    error
    Ctn Broker Config Log    module0    core    error
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker Remove Rrd Output    central
    Ctn Clear Retention
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    Ctn Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true

Ctn Config Escalations
    [Documentation]    Configuring engine notification escalations settings.
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    c
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    first_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_1    contact_groups    contactgroup_1
    Ctn Engine Config Replace Value In Services    0    service_1    active_checks_enabled    0
    Ctn Engine Config Replace Value In Services    0    service_1    max_check_attempts     1
    Ctn Engine Config Replace Value In Services    0    service_1    retry_interval     1
    Ctn Engine Config Set Value In Services    0    service_1    notification_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval     1
    Ctn Engine Config Replace Value In Services    0    service_1    check_command    command_4
    Ctn Engine Config Set Value In Services    0    service_2    contact_groups    contactgroup_1
    Ctn Engine Config Replace Value In Services    0    service_2    max_check_attempts     1
    Ctn Engine Config Set Value In Services    0    service_2    notification_options    c
    Ctn Engine Config Set Value In Services    0    service_2    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_2    first_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_2    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_2    notification_interval    1
    Ctn Engine Config Replace Value In Services    0    service_2    first_notification_delay    0
    Ctn Engine Config Replace Value In Services    0    service_2    check_interval     1
    Ctn Engine Config Replace Value In Services    0    service_2    active_checks_enabled    0
    Ctn Engine Config Replace Value In Services    0    service_2    retry_interval     1
    Ctn Engine Config Replace Value In Services    0    service_2    check_command    command_4
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

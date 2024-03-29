*** Settings ***
Documentation       Centreon notification

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
not1
    [Documentation]    This test case configures a single service and verifies that a notification is sent when the service is in a non-OK state.
    [Tags]    broker    engine    services    hosts    notification
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
    Ctn Start engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    check_for_external_commands() should be available.

    ## Time to set the service to CRITICAL HARD.
    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Service Check Result    host_1    service_1    2    critical
        Sleep    1s
    END

    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;critical
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent

    Ctn Stop engine
    Ctn Kindly Stop Broker

not2
    [Documentation]    This test case configures a single service and verifies that a recovery notification is sent after a service recovers from a non-OK state.
    [Tags]    broker    engine    services    hosts    notification
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
    Ctn Start engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    check_for_external_commands() should be available.

    ## Time to set the service to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Service Check Result    host_1    service_1    2    critical
        Sleep    1s
    END

    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;critical
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No notification has been sent concerning a critical service

    ## Time to set the service to UP  hard

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Service Check Result    host_1    service_1    0    ok
        Sleep    1s
    END

    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;ok
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification recovery is not sent

    Ctn Stop engine
    Ctn Kindly Stop Broker

not3
    [Documentation]    This test case configures a single service and verifies that a non-OK notification is sent after the service exits downtime.
    [Tags]    broker    engine    services    hosts    notification
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
    Ctn Start engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands() should be available.

    # It's time to schedule a downtime
    Ctn Schedule Service Downtime    host_1    service_1    ${60}

    ${result}    Ctn Check Number Of Downtimes    ${1}    ${start}    ${60}
    Should Be True    ${result}    We should have 1 downtime enabled.

     ## Time to set the service to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Service Check Result    host_1    service_1    2    critical
        Sleep    10s
    END

    # Let's wait for the external command check start
    ${content}    Create List    SERVICE DOWNTIME ALERT: host_1;service_1;STOPPED; Service has exited from a period of scheduled downtime
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The downtime has not finished .

    Ctn Process Service Check Result    host_1    service_1    2    critical

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;critical
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The critical notification is not sent

    Ctn Stop engine
    Ctn Kindly Stop Broker

not4
    [Documentation]    This test case configures a single service and verifies that a non-OK notification is sent when the acknowledgement is completed.
    [Tags]    broker    engine    services    hosts    notification
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
    Ctn Start engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands() should be available.

    # Time to set the service to CRITICAL HARD.
    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Service Check Result    host_1    service_1    2    critical
        Sleep    1s
    END

    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    # Acknowledge the service with critical status
    Ctn Acknowledge Service Problem    host_1    service_1    STICKY

    # Let's wait for the external command check start
    ${content}    Create List    ACKNOWLEDGE_SVC_PROBLEM;host_1;service_1;2;0;0;admin;Service (host_1,service_1) acknowledged
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands() should be available.

    Ctn Process Service Check Result    host_1    service_1    0    ok

    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;ok
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The recovery notification for service_1 is not sent

    Ctn Stop engine
    Ctn Kindly Stop Broker

not5
    [Documentation]    This test case configures two services with two different users being notified when the services transition to a critical state.
    [Tags]    broker    engine    services    hosts    notification
    Ctn Config Engine    ${1}    ${2}    ${1}
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Set Value In Hosts    0    host_2    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_2    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_2    contacts    U2
    Ctn Engine Config Set Value In Services    0    service_2    contacts    U2
    Ctn Engine Config Set Value In Services    0    service_2    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_2    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_2    notification_period    24x7
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    U2    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    U2    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands() should be available.

    ## Time to set the service to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Service Check Result    host_1    service_1    2    critical
        Sleep    1s
        Ctn Process Service Check Result    host_2    service_2    2    critical
        Sleep    1s
    END

    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${2}    70    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${result}    Ctn Check Service Status With Timeout    host_2    service_2    ${2}    70    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;critical
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The critical notification of service_1 is not sent

    ${content}    Create List    SERVICE NOTIFICATION: U2;host_2;service_2;CRITICAL;command_notif;critical
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The critical notification of service_2 is not sent

    Ctn Stop engine
    Ctn Kindly Stop Broker

not6
    [Documentation]     This test case validates the behavior when the notification time period is set to null.
    [Tags]    broker    engine    services    hosts    notification
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
    Ctn Start engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands() should be available.

    ## Time to set the service to CRITICAL HARD.
    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Service Check Result    host_1    service_1    2    critical
        Sleep    1s
    END

    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;critical
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The critical notification of service_1 is not sent

    Ctn Engine Config Replace Value In Services    0    service_1    notification_period    none
    Sleep    5s

    ${start}    Get Current Date
    Ctn Reload Broker
    Ctn Reload Engine

    ## Time to set the service to UP  hard

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Service Check Result    host_1    service_1    0    ok
        Sleep    1s
    END

    ${content}    Create List    This notifier shouldn't have notifications sent out at this time
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The timeperiod is not working

    Ctn Stop engine
    Ctn Kindly Stop Broker

not7
    [Documentation]    This test case simulates a host alert scenario.
    [Tags]    broker    engine    host    hosts    notification
    Ctn Config Engine    ${1}    ${1}
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    ## Time to set the host to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    1    host_1 DOWN
        Sleep    1s
    END

    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST ALERT: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    the host alert is not sent

    Ctn Stop engine
    Ctn Kindly Stop Broker

not8
    [Documentation]    This test validates the critical host notification.
    [Tags]    broker    engine    host    notification
    Ctn Config Engine    ${1}    ${1}
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    ## Time to set the host to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    1    host_1 DOWN
        Sleep    1s
    END

    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;host_1 DOWN;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_1 is not sent

    Ctn Stop engine
    Ctn Kindly Stop Broker

not9
    [Documentation]    This test case configures a single host and verifies that a recovery notification is sent after the host recovers from a non-OK state.
    [Tags]    broker    engine    host    notification
    Ctn Config Engine    ${1}    ${1}
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    ## Time to set the host to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    1    host_1 DOWN
        Sleep    1s
    END

    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;RECOVERY (UP);command_notif;Host
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The recovery notification of host_1 is not sent

    Ctn Stop engine
    Ctn Kindly Stop Broker

not10
    [Documentation]    This test case involves scheduling downtime on a down host. After the downtime is finished and the host is still critical, we should receive a critical notification.
    [Tags]    broker    engine    host    notification
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    ## Time to set the host to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    0    host_1 UP
        Sleep    1s
    END

    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END


    Ctn Schedule Host Downtime    ${0}    host_1    ${3600}

    Sleep    10s

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    1    host_1 DOWN
        Sleep    1s
    END

    Ctn Delete Host Downtimes    ${0}    host_1

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;host_1 DOWN;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_1 is not sent

    Ctn Stop engine
    Ctn Kindly Stop Broker

not11
    [Documentation]    This test case involves scheduling downtime on a down host that already had a critical notification. After putting it in the UP state when the downtime is finished and the host is UP, we should receive a recovery notification.
    [Tags]    broker    engine    host    notification
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    ## Time to set the host to UP HARD.

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    1    host_1 DOWN
        Sleep    1s
    END

    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END


    Ctn Schedule Host Downtime    ${0}    host_1    ${3600}

    Sleep    10s
    ## Time to set the host to CRITICAL HARD.
    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    0    host_1 UP
        Sleep    1s
    END

    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    Ctn Delete Host Downtimes    ${0}    host_1

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;RECOVERY (UP);command_notif;Host
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The recovery notification of host_1 is not sent

    Ctn Stop engine
    Ctn Kindly Stop Broker


not12
    [Documentation]    This test case involves configuring one service and checking that three alerts are sent for it.
    [Tags]    broker    engine    services    hosts    notification
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
    Ctn Start engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands() is not available.

    ## Time to set the service to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Service Check Result    host_1    service_1    2    critical
        Sleep    1s
    END

    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE ALERT: host_1;service_1;CRITICAL;SOFT;1;critical
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The first service alert SOFT1 is not sent 

    ${content}    Create List    SERVICE ALERT: host_1;service_1;CRITICAL;SOFT;2;critical
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The second service alert SOFT2 is not sent

    ${content}    Create List    SERVICE ALERT: host_1;service_1;CRITICAL;HARD;3;critical
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The third service alert hard is not sent

    Ctn Stop engine
    Ctn Kindly Stop Broker


not13
    [Documentation]    Escalations
    [Tags]    broker    engine    services    hosts    notification
    Ctn Config Engine    ${1}    ${2}    ${1}
    Ctn Engine Config Set Value    0    interval_length    10    True
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
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

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine

  # Let's wait for the external command check start
    ${content}    Create List    check
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands() is not available.

    Ctn Service Check

    # Let's wait for the first notification of the user U1
    ${content}    Create List    SERVICE NOTIFICATION: U1;host_1;service_1;CRITICAL;command_notif;critical_0;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The first notification of U1 is not sent
    # Let's wait for the first notification of the contact group 1
    ${content}    Create List    SERVICE NOTIFICATION: U1;host_2;service_2;CRITICAL;command_notif;critical_0;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The first notification of contact group 1 is not sent

    Ctn Service Check

    # Let's wait for the first notification of the contact group 2 U3 ET U2

    ${content}    Create List     SERVICE NOTIFICATION: U2;host_1;service_1;CRITICAL;command_notif;critical_0;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The first notification of U2 is not sent

    ${content}    Create List    SERVICE NOTIFICATION: U3;host_1;service_1;CRITICAL;command_notif;critical_0;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The first notification of U3 is not sent

    Ctn Service Check

    # Let's wait for the second notification of the contact group 2 U3 ET U2

    ${content}    Create List    SERVICE NOTIFICATION: U2;host_2;service_2;CRITICAL;command_notif;critical_0;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The second notification of U2 is not sent

    ${content}    Create List    SERVICE NOTIFICATION: U3;host_2;service_2;CRITICAL;command_notif;critical_0;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The second notification of U3 is not sent

    Ctn Service Check

    # Let's wait for the first notification of the contact group 3 U4

    ${content}    Create List    SERVICE NOTIFICATION: U4;host_1;service_1;CRITICAL;command_notif;critical_0;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The first notification of U4 is not sent

    ${content}    Create List    SERVICE NOTIFICATION: U4;host_2;service_2;CRITICAL;command_notif;critical_0;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The second notification of U4 is not sent




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
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.1
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.1
    Ctn Broker Config Add Item    central    bbdo_version    3.0.1
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    tcp    error
    Ctn Broker Config Log    central    sql    error
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

Ctn Service Check
    FOR   ${i}    IN RANGE    ${4}
        Ctn Process Service Check Result    host_1    service_1    2    critical
        Sleep    1s
    END

    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    FOR   ${i}    IN RANGE    ${4}
        Ctn Process Service Check Result    host_2    service_2    2    critical
        Sleep    1s
    END

    ${result}    Ctn Check Service Status With Timeout    host_2    service_2    ${2}    60    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD
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
Test Teardown       Save Logs If Failed


*** Test Cases ***
not1
    [Documentation]    This test case configures a single service and verifies that a notification is sent when the service is in a non-OK state.
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${1}    ${1}
    Config Notifications
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Replace Value In Services    0    service_1    check_interval    1
    Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    check_for_external_commands() should be available.

    Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    
    ## Time to set the service to CRITICAL HARD.
    Set Service state    ${30}    ${2}

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    90    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent

    Stop Engine
    Kindly Stop Broker

not2
    [Documentation]    This test case configures a single service and verifies that a recovery notification is sent after a service recovers from a non-OK state.
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${1}    ${1}
    Config Notifications
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Replace Value In Services    0    service_1    check_interval    1
    Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    check_for_external_commands() should be available.

    ## Time to set the service to CRITICAL HARD.

    Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ## Time to set the service to CRITICAL HARD.
    Set Service state    ${38}    ${2}

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No notification has been sent concerning a critical service

    ## Time to set the service to UP  hard

    Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK

    Set Service state    ${30}    ${0}

    ${result}    Check Service Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification recovery is not sent

    Stop Engine
    Kindly Stop Broker

not3
    [Documentation]    This test case configures a single service and verifies that a non-OK notification is sent after the service exits downtime.
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${1}    ${1}
    Config Notifications
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Replace Value In Services    0    service_1    check_interval    1
    Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands() should be available.

    # It's time to schedule a downtime
    Schedule Service Downtime    host_1    service_1    ${60}

    ${result}    Check Number Of Downtimes    ${1}    ${start}    ${60}
    Should Be True    ${result}    We should have 1 downtime enabled.

     ## Time to set the service to CRITICAL HARD.

    Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    Set Service state    ${38}    ${2}
    Sleep    60s

    # Let's wait for the external command check start
    ${content}    Create List    SERVICE DOWNTIME ALERT: host_1;service_1;STOPPED; Service has exited from a period of scheduled downtime
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The downtime has not finished .

    Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The critical notification is not sent

    Stop Engine
    Kindly Stop Broker

not4
    [Documentation]    This test case configures a single service and verifies that a non-OK notification is sent when the acknowledgement is completed.
    [Tags]    broker    engine    services    acknowledgement    notification
    Config Engine    ${1}    ${1}    ${1}
    Config Notifications
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Replace Value In Services    0    service_1    check_interval    1
    Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands() should be available.

    # Time to set the service to CRITICAL HARD.
    Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    Set Service state    ${38}    ${2}
    Sleep    60s

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    # Acknowledge the service with critical status
    Acknowledge Service Problem    host_1    service_1    STICKY

    # Let's wait for the external command check start
    ${content}    Create List    ACKNOWLEDGE_SVC_PROBLEM;host_1;service_1;2;0;0;admin;Service (host_1,service_1) acknowledged
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands() should be available.

    # Time to set the service to OK HARD.
    Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK

    Set Service state    ${30}    ${0}

    ${result}    Check Service Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be OK HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The recovery notification for service_1 is not sent

    Stop Engine
    Kindly Stop Broker

not5
    [Documentation]    This test case configures two services with two different users being notified when the services transition to a critical state.
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${2}    ${1}
    Config Notifications
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Replace Value In Services    0    service_1    check_interval    1
    Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Engine Config Set Value In Hosts    0    host_2    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_2    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_2    contacts    U2
    Engine Config Set Value In Services    0    service_2    contacts    U2
    Engine Config Set Value In Services    0    service_2    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_2    notifications_enabled    1
    Engine Config Set Value In Services    0    service_2    notification_period    24x7
    Engine Config Replace Value In Services    0    service_2    check_interval    1
    Engine Config Replace Value In Services    0    service_2    retry_interval    1
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
    Should Be True    ${result}    check_for_external_commands() should be available.

    ## Time to set the services to CRITICAL HARD.

    Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    Process Service Result Hard    host_2    service_2    ${2}    The service_2 is CRITICAL

    Set Service state    ${38}    ${2}

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    70    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${result}    Check Service Status With Timeout    host_2    service_2    ${2}    70    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD
    
    # Notification for the first user john
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The critical notification of service_1 is not sent
    
    # Notification for the second user U2
    ${content}    Create List    SERVICE NOTIFICATION: U2;host_2;service_2;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The critical notification of service_2 is not sent

    Stop Engine
    Kindly Stop Broker

not6
    [Documentation]     This test case validate the behavior when the notification time period is set to null.
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${1}    ${1}
    Config Notifications
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Replace Value In Services    0    service_1    check_interval    1
    Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands() should be available.

    ## Time to set the service to CRITICAL HARD.
    Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    Set Service state    ${38}    ${2}

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The critical notification of service_1 is not sent

    Engine Config Replace Value In Services    0    service_1    notification_period    none
    Sleep    5s

    ${start}    Get Current Date
    Reload Broker
    Reload Engine

    ## Time to set the service to OK hard

    Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK

    Set Service state    ${30}    ${0}

    ${content}    Create List    This notifier shouldn't have notifications sent out at this time
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The timeperiod is not working

    Stop Engine
    Kindly Stop Broker

not7
    [Documentation]    This test case simulates a host alert scenario.
    [Tags]    broker    engine    host    hosts    notification
    Config Engine    ${1}    ${1}
    Config Notifications
    Config Host Command Status    ${0}    checkh1    2
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

    ## Time to set the host to DOWN HARD

    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST ALERT: host_1;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    the host alert is not sent

    Stop Engine
    Kindly Stop Broker

not8
    [Documentation]    This test validates the critical host notification.
    [Tags]    broker    engine    host    notification
    Config Engine    ${1}    ${1}
    Config Notifications
    Config Host Command Status    ${0}    checkh1    2
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

    ## Time to set the host to DOWN HARD.
    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_1 is not sent

    Stop Engine
    Kindly Stop Broker

not9
    [Documentation]    This test case configures a single host and verifies that a recovery notification is sent after the host recovers from a non-OK state.
    [Tags]    broker    engine    host    notification
    Config Engine    ${1}    ${1}
    Config Notifications
    Config Host Command Status    ${0}    checkh1    2
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
    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_1 is not sent

    ## Time to set the host to UP HARD.
    Process Host Check Result    host_1    0    host_1 UP

    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;RECOVERY (UP);command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The recovery notification of host_1 is not sent

    Stop Engine
    Kindly Stop Broker

not10
    [Documentation]    This test case involves scheduling downtime on a down host. After the downtime is finished and the host is still critical, we should receive a critical notification.
    [Tags]    broker    engine    host    notification
    Config Engine    ${1}    ${1}    ${1}
    Config Notifications
    Config Host Command Status    ${0}    checkh1    2
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
    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    ## Time to set the host on Downtime.
    Schedule Host Downtime    ${0}    host_1    ${60}
    ## Time to delete the host Downtime.
    Delete Host Downtimes    ${0}    host_1

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_1 is not sent

    Stop Engine
    Kindly Stop Broker

not11
    [Documentation]    This test case involves scheduling downtime on a down host that already had a critical notification.When The Host return to UP state we should receive a recovery notification.
    [Tags]    broker    engine    host    notification
    Config Engine    ${1}    ${1}    ${1}
    Config Notifications
    Config Host Command Status    ${0}    checkh1    2
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
    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The down notification of host_1 is not sent

    Schedule Host Downtime    ${0}    host_1    ${30}

    ## Time to set the host to UP HARD.
    Process Host Check Result    host_1    0    host_1 UP

    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List     HOST DOWNTIME ALERT: host_1;STOPPED; Host has exited from a period of scheduled downtime
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The recovery notification of host_1 is not sent

    ## Time to set the host to UP HARD.
    
    Process Host Check Result    host_1    0    host_1 UP

    FOR    ${i}    IN RANGE    ${4}
        Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;RECOVERY (UP);command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The down notification of host_1 is not sent

    Stop Engine
    Kindly Stop Broker


not12
    [Documentation]    This test case involves configuring one service and checking that three alerts are sent for it.
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${1}    ${1}
    Config Notifications
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
    Should Be True    ${result}    check_for_external_commands() is not available.

    ## Time to set the service to CRITICAL HARD.

    Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    Set Service state    ${38}    ${2}

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE ALERT: host_1;service_1;CRITICAL;SOFT;1;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The first service alert SOFT1 is not sent 

    ${content}    Create List    SERVICE ALERT: host_1;service_1;CRITICAL;SOFT;2;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The second service alert SOFT2 is not sent

    ${content}    Create List    SERVICE ALERT: host_1;service_1;CRITICAL;HARD;3;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The third service alert hard is not sent

    Stop Engine
    Kindly Stop Broker


not13
    [Documentation]    Escalations
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${2}    ${1}
    Engine Config Set Value    0    interval_length    1    True
    Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Add Service Group    ${0}    ${1}    ["host_1","service_1", "host_2","service_2"]
    Config Notifications
    Config Escalations

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
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands() is not available.

    Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    Process Service Result Hard    host_2    service_2    ${2}    The service_2 is CRITICAL
    Set Service state    ${38}    ${2}

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD
    ${result}    Check Service Status With Timeout    host_2    service_2    ${2}    60    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD

    # Let's wait for the first notification of the user U1
    ${content}    Create List    SERVICE NOTIFICATION: U1;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The first notification of U1 is not sent
    # Let's wait for the first notification of the contact group 1
    ${content}    Create List    SERVICE NOTIFICATION: U1;host_2;service_2;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The first notification of contact group 1 is not sent

    Service Check

    # Let's wait for the first notification of the contact group 2 U3 ET U2
    ${content}    Create List     SERVICE NOTIFICATION: U2;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The first notification of U2 is not sent

    ${content}    Create List    SERVICE NOTIFICATION: U3;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The first notification of U3 is not sent

    Service Check

    # Let's wait for the second notification of the contact group 2 U3 ET U2
    ${content}    Create List    SERVICE NOTIFICATION: U2;host_2;service_2;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The second notification of U2 is not sent

    ${content}    Create List    SERVICE NOTIFICATION: U3;host_2;service_2;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The second notification of U3 is not sent

    Service Check

    # Let's wait for the first notification of the contact group 3 U4
    ${content}    Create List    SERVICE NOTIFICATION: U4;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The first notification of U4 is not sent

    ${content}    Create List    SERVICE NOTIFICATION: U4;host_2;service_2;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The second notification of U4 is not sent

not14
    [Documentation]    notification for a dependensies host
    [Tags]    broker    engine    host    unified_sql
    Config Engine    ${1}    ${2}    ${1}
    Config Notifications
    Config Engine Add Cfg File    ${0}    dependencies.cfg
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r,s
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Hosts    0    host_1    first_notification_delay    0
    Engine Config Set Value In Hosts    0    host_1    recovery_notification_delay    0
    Engine Config Set Value In Hosts    0    host_1    notification_interval    0
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    n
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Services    0    service_1    first_notification_delay    0
    Engine Config Set Value In Services    0    service_1    recovery_notification_delay    0
    Engine Config Set Value In Services    0    service_1    notification_interval    0
    Engine Config Set Value In Hosts    0    host_2    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_2    notification_options    d,r,s
    Engine Config Set Value In Hosts    0    host_2    contacts    John_Doe
    Engine Config Set Value In Hosts    0    host_2    first_notification_delay    0
    Engine Config Set Value In Hosts    0    host_2    recovery_notification_delay    0
    Engine Config Set Value In Hosts    0    host_2    notification_interval    0
    Engine Config Set Value In Services    0    service_2    contacts    John_Doe
    Engine Config Set Value In Services    0    service_2    notification_options    n
    Engine Config Set Value In Services    0    service_2    notifications_enabled    1
    Engine Config Set Value In Services    0    service_2    notification_period    24x7
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    Config Host Command Status    ${0}    checkh1    2
    Config Host Command Status    ${0}    checkh2    2

    Create Dependencieshst File    0    host_2    host_1

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

     ## Time to set the host to CRITICAL HARD.

    FOR    ${i}    IN RANGE    ${3}
        Schedule Forced Host Check    host_2    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_2;DOWN;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_2 is not sent
    

    Process Host Check Result    host_2    0    host_2 UP

    FOR    ${i}    IN RANGE    ${3}
        Schedule Forced Host Check    host_2    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_2;RECOVERY (UP);command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_2 is not sent

    FOR    ${i}    IN RANGE    ${3}
        Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_1 is not sent

    ${new_date}    Get Current Date

    Process Host Check Result    host_2    1    host_2 DOWN

    FOR    ${i}    IN RANGE    ${3}
        Schedule Forced Host Check    host_2    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END
    
    ${content}    Create List    This notifier won't send any notification since it depends on another notifier that has already sent one
    ${result}    Find In Log With Timeout    ${engineLog0}    ${new_date}    ${content}    90
    Should Be True    ${result}    The down notification of host_2 is sent dependency not working

    Process Host Check Result    host_1    0    host_1 UP

    FOR    ${i}    IN RANGE    ${3}
        Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;RECOVERY (UP);command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${new_date}    ${content}    60
    Should Be True    ${result}    The down notification of host_1 is not sent

    Stop Engine
    Kindly Stop Broker

not15
    [Documentation]    notification for a Service dependency
    [Tags]    broker    engine    services    unified_sql
    Config Engine    ${1}    ${2}    ${1}
    Config Notifications
    Config Engine Add Cfg File    ${0}    dependencies.cfg
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    n
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Services    0    service_1    first_notification_delay    0
    Engine Config Set Value In Services    0    service_1    recovery_notification_delay    0
    Engine Config Set Value In Services    0    service_1    notification_interval    0
    Engine Config Set Value In Hosts    0    host_2    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_2    notification_options    n
    Engine Config Set Value In Hosts    0    host_2    contacts    John_Doe
    Engine Config Set Value In Services    0    service_2    contacts    John_Doe
    Engine Config Set Value In Services    0    service_2    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_2    notifications_enabled    1
    Engine Config Set Value In Services    0    service_2    notification_period    24x7
    Engine Config Set Value In Services    0    service_2    first_notification_delay    0
    Engine Config Set Value In Services    0    service_2    recovery_notification_delay    0
    Engine Config Set Value In Services    0    service_2    notification_interval    0
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Create Dependencies File    0    host_2    host_1    service_2    service_1

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ## Time to set the service2 to CRITICAL HARD.
    Service2 Check Critical

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_2;service_2;CRITICAL;command_notif;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent

    ## Time to set the service2 to OK  hard
    Service2 Check OK

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_2;service_2;RECOVERY (OK);command_notif;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent

   ## Time to set the service1 to CRITICAL HARD.

    Service1 Check Critical

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent

    ${new_date}    Get Current Date
    ## Time to set the service2 to CRITICAL HARD.

    Service2 Check Critical

    ${content}    Create List    This notifier won't send any notification since it depends on another notifier that has already sent one
    ${result}    Find In Log with Timeout    ${engineLog0}    ${new_date}    ${content}    60
    Should Be True    ${result}     the dependency not working and the service_é has recieved a notification

    ## Time to set the service1 to OK  hard

    Service1 Check OK

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${new_date}    ${content}    60
    Should Be True    ${result}    The notification is not sent

    Stop Engine
    Kindly Stop Broker


not16
    [Documentation]    several notification commands for the same user.
    [Tags]    broker    engine    services    unified_sql
    Config Engine    ${1}    ${1}    ${1}
    Config Notifications
    engine_config_add_command
    ...    0
    ...    command_notif1
    ...    /usr/bin/false
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif,command_notif1
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif,command_notif1


    ${start}=    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.
    ## Time to set the service to CRITICAL HARD.

    Service1 Check Critical

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    A message telling that notification is not sent

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif1;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The notification is not sent

    Stop Engine
    Kindly Stop Broker

not17
    [Documentation]    notification for a dependensies services group
    [Tags]    broker    engine    services    unified_sql
    Config Engine    ${1}    ${4}    ${1}
    Engine Config Set Value    0    interval_length    1    True
    Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Add service Group    ${0}    ${1}    ["host_1","service_1", "host_2","service_2"]
    Add service Group    ${0}    ${2}    ["host_3","service_3", "host_4","service_4"]
    Config Notifications
    Config Engine Add Cfg File    ${0}    dependencies.cfg
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    n
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Services    0    service_1    first_notification_delay    0
    Engine Config Set Value In Services    0    service_1    recovery_notification_delay    0
    Engine Config Set Value In Services    0    service_1    notification_interval    0
    Engine Config Set Value In Hosts    0    host_2    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_2    notification_options    n
    Engine Config Set Value In Hosts    0    host_2    contacts    John_Doe
    Engine Config Set Value In Services    0    service_2    contacts    John_Doe
    Engine Config Set Value In Services    0    service_2    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_2    notifications_enabled    1
    Engine Config Set Value In Services    0    service_2    notification_period    24x7
    Engine Config Set Value In Services    0    service_2    first_notification_delay    0
    Engine Config Set Value In Services    0    service_2    recovery_notification_delay    0
    Engine Config Set Value In Services    0    service_2    notification_interval    0
    Engine Config Set Value In Hosts    0    host_3    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_3    notification_options    n
    Engine Config Set Value In Hosts    0    host_3    contacts    John_Doe
    Engine Config Set Value In Services    0    service_3    contacts    John_Doe
    Engine Config Set Value In Services    0    service_3    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_3    notifications_enabled    1
    Engine Config Set Value In Services    0    service_3    notification_period    24x7
    Engine Config Set Value In Services    0    service_3    first_notification_delay    0
    Engine Config Set Value In Services    0    service_3    recovery_notification_delay    0
    Engine Config Set Value In Services    0    service_3    notification_interval    0
    Engine Config Set Value In Hosts    0    host_4    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_4    notification_options    n
    Engine Config Set Value In Hosts    0    host_4    contacts    John_Doe
    Engine Config Set Value In Services    0    service_4    contacts    John_Doe
    Engine Config Set Value In Services    0    service_4    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_4    notifications_enabled    1
    Engine Config Set Value In Services    0    service_4    notification_period    24x7
    Engine Config Set Value In Services    0    service_4    first_notification_delay    0
    Engine Config Set Value In Services    0    service_4    recovery_notification_delay    0
    Engine Config Set Value In Services    0    service_4    notification_interval    0
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Create Dependenciesgrp File    0    servicegroup_2    servicegroup_1

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands() should be available.

    ## Time to set the service3 to CRITICAL HARD.
    FOR   ${i}    IN RANGE    ${4}
        Process Service Result Hard    host_3    service_3    ${2}    The service_3 is CRITICAL
        Sleep    1s
    END

    Set Service state    ${38}    ${2}

    ${result}    Check Service Status With Timeout    host_3    service_3    ${2}    90    HARD
    Should Be True    ${result}    Service (host_3,service_3) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_3;service_3;CRITICAL;command_notif;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent for service3

    ## Time to set the service3 to OK hard
    FOR   ${i}    IN RANGE    ${4}
        Process Service Result Hard    host_3    service_3    ${0}    The service_3 is OK
        Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_3    service_3    ${0}    90    HARD
    Should Be True    ${result}    Service (host_3,service_3) should be OK HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_3;service_3;RECOVERY (OK);command_notif;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent for service3

    ## Time to set the service1 to CRITICAL HARD.
    FOR   ${i}    IN RANGE    ${4}
        Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
        Sleep    1s
    END

    Set Service state    ${38}    ${2}

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    90    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent for service1

    ## Time to set the service3 to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${4}
        Process Service Result Hard    host_3    service_3    ${2}    The service_3 is CRITICAL
        Sleep    1s
    END
    
    Set Service state    ${38}    ${2}

    ${result}    Check Service Status With Timeout    host_3    service_3    ${2}    90    HARD
    Should Be True    ${result}    Service (host_3,service_3) should be CRITICAL HARD

    ${content}    Create List    This notifier won't send any notification since it depends on another notifier that has already sent one
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is sent for service3 dependency not working

    ## Time to set the service4 to CRITICAL HARD.
    FOR   ${i}    IN RANGE    ${4}
        Process Service Result Hard    host_4    service_4    ${2}    The service_4 is CRITICAL
        Sleep    1s
    END

    Set Service state    ${38}    ${2}

    ${result}    Check Service Status With Timeout    host_4    service_4    ${2}    90    HARD
    Should Be True    ${result}    Service (host_4,service_4) should be CRITICAL HARD

    ${content}    Create List    This notifier won't send any notification since it depends on another notifier that has already sent one
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is sent for service4 dependency not working

    ## Time to set the service1 to OK hard
    FOR   ${i}    IN RANGE    ${4}
        Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK
        Sleep    1s
    END
    
    ${result}    Check Service Status With Timeout    host_1    service_1    ${0}    90    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be OK HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification is not sent for service1

    Stop Engine
    Kindly Stop Broker


not18
    [Documentation]    notification for a dependensies host group
    [Tags]    broker    engine    host    unified_sql
    Config Engine    ${1}    ${4}    ${0}
    Engine Config Set Value    0    interval_length    10    True
    Add Host Group    ${0}    ${1}    ["host_1", "host_2"]
    Add Host Group    ${0}    ${2}    ["host_3", "host_4"]
    Config Notifications
    Config Engine Add Cfg File    ${0}    dependencies.cfg
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r,s
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Hosts    0    host_1    first_notification_delay    0
    Engine Config Set Value In Hosts    0    host_1    recovery_notification_delay    0
    Engine Config Set Value In Hosts    0    host_1    notification_interval    0
    Engine Config Set Value In Hosts    0    host_2    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_2    notification_options    d,r,s
    Engine Config Set Value In Hosts    0    host_2    contacts    John_Doe
    Engine Config Set Value In Hosts    0    host_2    first_notification_delay    0
    Engine Config Set Value In Hosts    0    host_2    recovery_notification_delay    0
    Engine Config Set Value In Hosts    0    host_2    notification_interval    0
    Engine Config Set Value In Hosts    0    host_3    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_3    notification_options    d,r,s
    Engine Config Set Value In Hosts    0    host_3    contacts    John_Doe
    Engine Config Set Value In Hosts    0    host_3    first_notification_delay    0
    Engine Config Set Value In Hosts    0    host_3    recovery_notification_delay    0
    Engine Config Set Value In Hosts    0    host_3    notification_interval    0
    Engine Config Set Value In Hosts    0    host_4    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_4    notification_options    d,r,s
    Engine Config Set Value In Hosts    0    host_4    contacts    John_Doe
    Engine Config Set Value In Hosts    0    host_4    first_notification_delay    0
    Engine Config Set Value In Hosts    0    host_4    recovery_notification_delay    0
    Engine Config Set Value In Hosts    0    host_4    notification_interval    0
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    Create Dependencieshstgrp File    0    hostgroup_2    hostgroup_1

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # Time to set the host to CRITICAL HARD.

    FOR   ${i}    IN RANGE    ${3}
        Process Host Check Result    host_3    1    host_3 DOWN
        Sleep    1s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_3;DOWN;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_3 is not sent

    FOR   ${i}    IN RANGE    ${3}
        Process Host Check Result    host_3    0    host_3 UP
        Sleep    1s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_3;RECOVERY (UP);command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The recovery notification of host_3 is not sent

    FOR   ${i}    IN RANGE    ${3}
        Process Host Check Result    host_1    1    host_1 DOWN
        Sleep    1s
    END
    FOR   ${i}    IN RANGE    ${3}
        Process Host Check Result    host_3    1    host_3 DOWN
        Sleep    1s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_1 is not sent

    ${new_date}    Get Current Date
    
    ${content}    Create List    This notifier won't send any notification since it depends on another notifier that has already sent one
    ${result}    Find In Log With Timeout    ${engineLog0}    ${new_date}    ${content}    60
    Should Be True    ${result}    The down notification of host_3 is sent dependency not working

    FOR   ${i}    IN RANGE    ${3}
        Process Host Check Result    host_4    1    host_4 DOWN
        Sleep    1s
    END

    ${content}    Create List    This notifier won't send any notification since it depends on another notifier that has already sent one
    ${result}    Find In Log With Timeout    ${engineLog0}    ${new_date}    ${content}    60
    Should Be True    ${result}    The down notification of host_4 is sent dependency not working

    FOR   ${i}    IN RANGE    ${3}
        Process Host Check Result    host_1    0    host_1 UP
        Sleep    1s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;RECOVERY (UP);command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${new_date}    ${content}    60
    Should Be True    ${result}    The recovery notification of host_1 is not sent

    Stop Engine
    Kindly Stop Broker

not19
    [Documentation]    notification delay where first notification delay equal retry check
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${1}    ${1}
    Engine Config Set Value    0    interval_length    1    True
    Config Notifications
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Services    0    service_1    first_notification_delay    1
    Engine Config Set Value In Services    0    service_1    notification_interval    1
    Engine Config Replace Value In Services    0    service_1    check_interval    1
    Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    check_for_external_commands() should be available.

    FOR   ${i}    IN RANGE    ${4}
        Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
        Sleep    1s
    END
    
    Set Service state    ${38}    ${2}

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    No notification has been sent concerning a critical service

    Stop Engine
    Kindly Stop Broker

not20
    [Documentation]    notification delay where first notification delay greater than retry check 
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${1}    ${1}
    Engine Config Set Value    0    interval_length    1    True
    Config Notifications
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Services    0    service_1    first_notification_delay    3
    Engine Config Replace Value In Services    0    service_1    check_interval    2
    Engine Config Replace Value In Services    0    service_1    retry_interval    2
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    check_for_external_commands() should be available.

    FOR   ${i}    IN RANGE    ${4}
        Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
        Sleep    1s
    END
    
    Set Service state    ${38}    ${2}
    
    FOR   ${i}    IN RANGE    ${4}
        ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
        Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD
    END

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    100
    Should Be True    ${result}    No notification has been sent concerning a critical service

    Stop Engine
    Kindly Stop Broker

not21
    [Documentation]    notification delay where first notification delay greater than retry check by 1 min
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${1}    ${1}
    Engine Config Set Value    0    interval_length    1    True
    Config Notifications
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Services    0    service_1    first_notification_delay    2
    Engine Config Replace Value In Services    0    service_1    check_interval    1
    Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    check_for_external_commands() should be available.

    FOR   ${i}    IN RANGE    ${4}
        Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
        Sleep    1s
    END
    
    Set Service state    ${38}    ${2}

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    No notification has been sent concerning a critical service

    Stop Engine
    Kindly Stop Broker

not22
    [Documentation]    notification delay where first notification delay samller than retry check
    [Tags]    broker    engine    services    hosts    notification
    Config Engine    ${1}    ${1}    ${1}
    Engine Config Set Value    0    interval_length    1    True
    Config Notifications
    Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Engine Config Set Value In Services    0    service_1    first_notification_delay    1
    Engine Config Replace Value In Services    0    service_1    check_interval    2
    Engine Config Replace Value In Services    0    service_1    retry_interval    2
    Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    check_for_external_commands() should be available.

    FOR   ${i}    IN RANGE    ${4}
        Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
        Sleep    1s
    END
    
    Set Service state    ${38}    ${2}

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    No notification has been sent concerning a critical service

    Stop Engine
    Kindly Stop Broker

*** Keywords ***
Config Notifications
    [Documentation]    Configuring engine notification settings.
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
    Config Engine Add Cfg File    ${0}    contacts.cfg
    Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Config Engine Add Cfg File    ${0}    escalations.cfg
    Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true

Config Escalations
    [Documentation]    Configuring engine notification escalations settings
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

Service Check
    FOR   ${i}    IN RANGE    ${4}
        Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
        Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    FOR   ${i}    IN RANGE    ${4}
        Process Service Result Hard    host_2    service_2    ${2}    The service_1 is CRITICAL
        Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_2    service_2    ${2}    60    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD

Service1 Check Critical
    FOR   ${i}    IN RANGE    ${3}
        Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
        Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

Service1 Check OK
    FOR   ${i}    IN RANGE    ${3}
        Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK
        Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be OK HARD

Service2 Check Critical
    FOR   ${i}    IN RANGE    ${3}
        Process Service Result Hard    host_2    service_2    ${2}    The service_2 is CRITICAL
        Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_2    service_2    ${2}    60    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD

Service2 Check OK
    FOR   ${i}    IN RANGE    ${3}
        Process Service Result Hard    host_2    service_2    ${0}    The service_2 is OK
    Sleep    1s
    END

    ${result}    Check Service Status With Timeout    host_2    service_2    ${0}    60    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be OK HARD
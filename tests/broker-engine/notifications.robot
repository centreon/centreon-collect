*** Settings ***
Documentation       Centreon notification

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
not1_reload
    [Documentation]    This test case configures a single service and set the service in a non-OK HARD state so engine sends a notification. Then the service is removed from the configuration and Engine is reloaded. And Engine doesn't crash.
    [Tags]    broker    engine    services    hosts    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${2}
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_2    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_2    notification_options    w,c,r,s
    Ctn Engine Config Set Value In Services    0    service_2    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_2    notification_period    24x7
    Ctn Engine Config Replace Value In Services    0    service_2    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_2    retry_interval    1
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_options    w,c,r,s

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${cmd_service_2}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_2}    ${2}
    ## Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard    host_1    service_2    ${2}    The service_2 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_2    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_2) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_2;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No notification has been sent concerning a critical service

    # It's time to schedule a downtime
    Ctn Schedule Service Fixed Downtime    host_1    service_2    60

    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_2    1    90
    Should Be True    ${result}    service must be in downtime

    Ctn Config Engine    ${1}    ${1}    ${1}

    Ctn Reload Engine
    Ctn Reload Broker

    Sleep    20s

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not1
    [Documentation]    This test case configures a single service and verifies that a notification is sent when the service is in a non-OK HARD state.
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
    ## Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No notification has been sent concerning a critical service

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not1_2
    [Documentation]    Scenario: Service notification command expands contact macros
    ...    Given a single service 'service_1' is assigned to contact 'John_Doe' with notifications enabled, options (w,c,r), and period 24x7
    ...    And 'service_1' has check/retry intervals of 1 and max check attempts of 1
    ...    And contact 'John_Doe' uses notification command 'command_with_macro' with macros
    ...    When the broker and engine are started and the service is driven to a HARD CRITICAL state
    ...    Then the service reaches HARD CRITICAL
    ...    And a service notification is logged for 'service_1' in CRITICAL state using 'command_with_macro'
    ...    And the processed notification command in logs is "/bin/echo pager address5 " showing macros are expanded
    [Tags]    broker    engine    services    hosts    notification    MON-192591
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Notifications
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    max_check_attempts    1
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_with_macro
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_with_macro
    Ctn Engine Config Set Value In Contacts    0    John_Doe    pager    pager
    Ctn Engine Config Set Value In Contacts    0    John_Doe    address5    address5

    Ctn Engine Config Add Command    ${0}    command_with_macro   /bin/echo $CONTACTPAGER$ $CONTACTADDRESS5$ $CONTACTADDRESS1$ $CONTACTADDRESS6$   ${None}

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

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_with_macro;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No notification has been sent concerning a critical service


    ## Check for processed notification command
    ${content}    Create List    Processed notification command: /bin/echo pager address5 
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Notification command with macros was not processed correctly


    Ctn Stop Engine
    Ctn Kindly Stop Broker

not1_WL_OK
    [Documentation]    This test case configures a single service. When it is in non-OK HARD state
    ...    a notification is sent because it is allowed by the whitelist
    [Tags]    broker    engine    services    hosts    notification    whitelist    MON-75741
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    # create non matching file with /tmp/var/lib/centreon-engine/check.pl 0 1.0.0.0
    ${whitelist_content}    Catenate
    ...    {"whitelist":{"wildcard":["/tmp/var/lib/centreon-engine/toto* * *"], "regex":["/usr/bin/true", "/tmp/var/lib/centreon-engine/check.pl .*"]}}
    Create File    /etc/centreon-engine-whitelist/test    ${whitelist_content}

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

    ${cmd_id}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_id}    ${2}
    ## Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;    my_system_r
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No notification has been sent concerning a critical service

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not1_WL_KO
    [Documentation]    This test case configures a single service. When it is in non-OK HARD state
    ...    a notification should be sent but it is not allowed by the whitelist
    [Tags]    broker    engine    services    hosts    notification    whitelist    MON-75741
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    # create non matching file with /tmp/var/lib/centreon-engine/check.pl 0 1.0.0.0
    ${whitelist_content}    Catenate
    ...    {"whitelist":{"wildcard":["/tmp/var/lib/centreon-engine/toto* * *"], "regex":["/usr/bin/good", "/tmp/var/lib/centreon-engine/check.pl .*"]}}
    Create File    /etc/centreon-engine-whitelist/test    ${whitelist_content}

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

    ${cmd_id}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_id}    ${2}
    ## Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;    Error: can't execute service notification for contact 'John_Doe' : it is not allowed by the whitelist
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No notification has been sent concerning a critical service

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not2
    [Documentation]    This test case configures a single service and verifies that a recovery notification is sent
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
    ## Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No notification has been sent concerning a critical service

    ## Time to set the service to UP hard
    ${start}    Ctn Get Round Current Date

    Ctn Set Command Status    ${cmd_service_1}    ${0}
    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be OK HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The notification recovery is not sent

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not3
    [Documentation]    This test case configures a single service and verifies the notification system's behavior during and after downtime
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

    # It's time to schedule a downtime
    Ctn Schedule Service Fixed Downtime    host_1    service_1    60

    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_1    1    90
    Should Be True    ${result}    service must be in downtime

    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${content}    Create List    We shouldn't notify about DOWNTIME events for this notifier
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The critical notification is sent while downtime

    ${start}    Ctn Get Round Current Date
    Ctn Delete Service Downtime    host_1    service_1

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The critical notification is not sent

    ${start}    Ctn Get Round Current Date
    Ctn Set Command Status    ${cmd_service_1}    ${0}

    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be OK HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The notification recovery is not sent

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not4
    [Documentation]    This test case configures a single service and verifies the notification system's behavior during and after acknowledgement
    [Tags]    broker    engine    services    acknowledgement    notification
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

    # Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    # Acknowledge the service with critical status
    Ctn Acknowledge Service Problem    host_1    service_1    STICKY

    # Let's wait for the external command check start
    ${content}    Create List    ACKNOWLEDGE_SVC_PROBLEM;host_1;service_1;2;0;0;admin;Service (host_1,service_1) acknowledged
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    check_for_external_commands() should be available.

    # Time to set the service to OK HARD.
    ${start}    Ctn Get Round Current Date
    Ctn Set Command Status    ${cmd_service_1}    ${0}

    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be OK HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The recovery notification for service_1 is not sent

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not5
    [Documentation]    This test case configures two services with two different users being notified when the services transition to a critical state.
    [Tags]    broker    engine    services    hosts    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${2}    ${1}
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Ctn Engine Config Set Value In Hosts    0    host_2    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_2    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_2    contacts    U2
    Ctn Engine Config Set Value In Services    0    service_2    contacts    U2
    Ctn Engine Config Set Value In Services    0    service_2    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_2    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_2    notification_period    24x7
    Ctn Engine Config Replace Value In Services    0    service_2    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_2    retry_interval    1
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    U2    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    U2    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${1}

    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    ${cmd_service_2}    Ctn Get Service Command Id    ${2}

    Ctn Set Command Status    ${cmd_service_1}    ${2}
    Ctn Set Command Status    ${cmd_service_2}    ${2}

    ## Time to set the services to CRITICAL HARD.

    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    Ctn Process Service Result Hard    host_2    service_2    ${2}    The service_2 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    70    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${result}    Ctn Check Service Resource Status With Timeout    host_2    service_2    ${2}    70    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD

    # Notification for the first user john
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The critical notification of service_1 is not sent

    # Notification for the second user U2
    ${content}    Create List    SERVICE NOTIFICATION: U2;host_2;service_2;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The critical notification of service_2 is not sent

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not6
    [Documentation]     This test case validate the behavior when the notification time period is set to null.
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

    ## Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_2,service_2) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The critical notification of service_1 is not sent

    Ctn Engine Config Replace Value In Services    0    service_1    notification_period    none
    Sleep    5s

    ${start}    Ctn Get Round Current Date
    Ctn Reload Broker
    Ctn Reload Engine

    ## Time to set the service to OK hard
    Ctn Set Command Status    ${cmd_service_1}    ${0}

    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be OK HARD

    ${content}    Create List    This notifier shouldn't have notifications sent out at this time
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The timeperiod is not working

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not7
    [Documentation]    This test case simulates a host alert scenario.
    [Tags]    broker    engine    host    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}
    Ctn Config Notifications
    Ctn Config Host Command Status    ${0}    checkh1    2
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${1}

    ## Time to set the host to DOWN HARD.
    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST ALERT: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    the host alert is not sent

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not8
    [Documentation]    This test validates the critical host notification.
    [Tags]    broker    engine    host    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}
    Ctn Config Notifications
    Ctn Config Host Command Status    ${0}    checkh1    2
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${1}

    ## Time to set the host to DOWN HARD.
    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_1 is not sent

    Ctn Stop Engine
    Ctn Kindly Stop Broker

not9
    [Documentation]    This test case configures a single host and verifies that a recovery notification is sent after the host recovers from a non-OK state.
    [Tags]    broker    engine    host    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}
    Ctn Config Notifications
    Ctn Config Host Command Status    ${0}    checkh1    2
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_period    24x7
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${1}

     ## Time to set the host to CRITICAL HARD.
    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;DOWN;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The down notification of host_1 is not sent

    ## Time to set the host to UP HARD.
    ${start}    Ctn Get Round Current Date
    Ctn Process Host Check Result    host_1    0    host_1 UP

    FOR    ${i}    IN RANGE    ${4}
        Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
        Sleep    5s
    END

    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;RECOVERY (UP);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The recovery notification of host_1 is not sent

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

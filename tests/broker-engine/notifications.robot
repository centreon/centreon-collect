*** Settings ***
Documentation       Centreon notification

Resource    ../resources/import.resource

Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Stop Processes
Test Teardown    Ctn Save Logs If Failed


*** Test Cases ***
SRV_NOTIF_THEN_RM_AND_RELOAD
    [Documentation]    Scenario: removing a notified service that is in downtime and reloading does not crash Engine
    ...    Given a service in a non-OK HARD state that has sent a notification and is under a scheduled downtime
    ...    When the service is removed from the configuration and Engine and Broker are reloaded
    ...    Then Engine keeps running without crashing
    [Tags]    broker    engine    services    hosts    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${2}
    Ctn Clear Engine White List
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

SRV_CRIT_NOTIF
    [Documentation]    Scenario: a critical service triggers a notification
    ...    Given a service configured with notifications enabled
    ...    When the service enters a non-OK HARD state
    ...    Then a CRITICAL notification is sent to its contact
    [Tags]    broker    engine    services    hosts    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
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
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

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
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

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

SRV_NOTIF_ALLOWED_BY_WLIST
    [Documentation]    Scenario: a service notification command allowed by the whitelist is executed
    ...    Given a service in a non-OK HARD state and a whitelist allowing its notification command
    ...    When the notification is triggered
    ...    Then the notification command is executed
    [Tags]    broker    engine    services    hosts    notification    whitelist    MON-75741
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
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
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

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

SRV_NOTIF_BLOCKED_BY_WLIST
    [Documentation]    Scenario: a service notification command blocked by the whitelist is not executed
    ...    Given a service in a non-OK HARD state and a whitelist not allowing its notification command
    ...    When the notification is triggered
    ...    Then the notification command is rejected by the whitelist
    [Tags]    broker    engine    services    hosts    notification    whitelist    MON-75741
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
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
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

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

SRV_REC_NOTIF
    [Documentation]    Scenario: a service recovery triggers a recovery notification
    ...    Given a service that sent a CRITICAL notification
    ...    When the service returns to an OK HARD state
    ...    Then a RECOVERY notification is sent
    [Tags]    broker    engine    services    hosts    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
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
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

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

SRV_NOTIF_SUPPR_DURING_DT
    [Documentation]    Scenario: notifications are suppressed while a service is in downtime
    ...    Given a service with a scheduled downtime
    ...    When the service enters a CRITICAL state during the downtime
    ...    Then no notification is sent during the downtime
    ...    And once the downtime is removed the CRITICAL and then RECOVERY notifications are sent
    [Tags]    broker    engine    services    hosts    notification
    Ctn Clear Commands Status
    Ctn Clear Engine White List
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
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

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

SRV_REC_NOTIF_AFTER_ACK
    [Documentation]    Scenario: a recovery notification is sent after an acknowledged problem recovers
    ...    Given a CRITICAL service that has been acknowledged
    ...    When the service returns to an OK HARD state
    ...    Then a RECOVERY notification is sent
    [Tags]    broker    engine    services    acknowledgement    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
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
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

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

SRV_NOTIF_ROUTED_TO_CORRECT_CTCT
    [Documentation]    Scenario: each service notification is routed to its own contact
    ...    Given two services each assigned to a different contact
    ...    When both services enter a CRITICAL HARD state
    ...    Then each contact receives the notification for its own service
    [Tags]    broker    engine    services    hosts    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${2}    ${1}
    Ctn Clear Engine White List
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
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

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

SRV_NOTIF_SUPPR_BY_EMPTY_TP
    [Documentation]    Scenario: an empty notification period suppresses notifications
    ...    Given a CRITICAL service whose notification period is then set to none
    ...    When the service state changes
    ...    Then no notification is sent because the notifier is out of its notification period
    [Tags]    broker    engine    services    hosts    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
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
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

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

HOST_DOWN_ALERT
    [Documentation]    Scenario: a host going down raises a host alert
    ...    Given a host configured with notifications
    ...    When the host enters a DOWN HARD state
    ...    Then a HOST ALERT is logged
    [Tags]    broker    engine    host    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}
    Ctn Clear Engine White List
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

    Ctn Wait For Engine To Be Ready    ${start}    ${1}

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

HOST_DOWN_NOTIF
    [Documentation]    Scenario: a host going down triggers a DOWN notification
    ...    Given a host configured with notifications
    ...    When the host enters a DOWN HARD state
    ...    Then a DOWN notification is sent to its contact
    [Tags]    broker    engine    host    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}
    Ctn Clear Engine White List
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

    Ctn Wait For Engine To Be Ready    ${start}    ${1}

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

HOST_REC_NOTIF
    [Documentation]    Scenario: a host recovery triggers a recovery notification
    ...    Given a host that sent a DOWN notification
    ...    When the host returns to an UP HARD state
    ...    Then a RECOVERY notification is sent
    [Tags]    broker    engine    host    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}
    Ctn Clear Engine White List
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

    Ctn Wait For Engine To Be Ready    ${start}    ${1}

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

FLAPPING_NOTIF
    [Documentation]    Scenario: a flapping service triggers a FLAPPINGSTART notification
    ...    Given a service with flap detection and flapping notifications enabled
    ...    When the service state oscillates enough to start flapping
    ...    Then a FLAPPINGSTART notification is sent
    [Tags]    broker    engine    services    flapping    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
    Ctn Config Notifications
    Ctn Engine Config Set Value    0    enable_flap_detection    1    True
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r,f
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_1    flap_detection_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    low_flap_threshold    10
    Ctn Engine Config Set Value In Services    0    service_1    high_flap_threshold    20
    Ctn Engine Config Set Value In Services    0    service_1    flap_detection_options    all
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_options    w,c,r,f

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Generate flapping by oscillating the service state.
    FOR    ${index}    IN RANGE    21
        Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
        Ctn Process Service Check Result    host_1    service_1    ${0}    The service_1 is OK
        Sleep    1s
    END

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;FLAPPINGSTART
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The FLAPPINGSTART notification for service_1 is not sent

    Ctn Stop Engine
    Ctn Kindly Stop Broker

FORCED_NOTIF
    [Documentation]    Scenario: a forced custom notification bypasses the notification-enabled check
    ...    Given a host with notifications disabled
    ...    When a forced custom notification is sent for the host
    ...    Then the notification is sent despite notifications being disabled
    [Tags]    broker    engine    hosts    forced    notification
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
    Ctn Config Notifications
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    0
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_options    d,u,r,f,s

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # A forced custom notification (option 2 = forced) must be sent even though
    # notifications are disabled for the host.
    Ctn Send Custom Host Notification    host_1    2    admin    Forced custom notification
    ${content}    Create List    HOST NOTIFICATION: John_Doe;host_1;CUSTOM
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The forced custom notification for host_1 is not sent despite disabled notifications

    Ctn Stop Engine
    Ctn Kindly Stop Broker

SRV_CRIT_NOTIF_BROKER
    [Documentation]    Scenario: in notification_mode=broker, Broker decides the notification and dispatches its execution to the poller
    ...    Given a service with a contact, in notification_mode=broker (BBDO3)
    ...    When the service enters a CRITICAL HARD state
    ...    Then Broker dispatches the notification execution to the supervising poller
    ...    And the poller runs the contact notification command (no local Engine decision)
    [Tags]    broker    engine    services    notification    MON-187019
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
    Ctn Config Notifications
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    core    info
    # The "dispatched notification execution" message asserted below is logged on
    # the notifications channel at info level, so enable it.
    Ctn Broker Config Log    central    notifications    info
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
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_1}    ${2}
    ## Time to set the service to CRITICAL HARD.
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    # Broker (not Engine) makes the decision and dispatches the execution.
    ${content}    Create List    dispatched notification execution for resource
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker did not dispatch the notification execution to the poller

    # The poller ran the dispatched notification command.
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The poller did not run the dispatched notification command

    Ctn Stop Engine
    Ctn Kindly Stop Broker

SRV_NOTIF_SUPPR_DURING_DT_BROKER
    [Documentation]    Scenario: in notification_mode=broker, notifications are suppressed while a service is in downtime
    ...    Given a service with a contact, in notification_mode=broker (BBDO3)
    ...    And a scheduled downtime set on the service via Broker gRPC
    ...    When the service enters a CRITICAL HARD state during the downtime
    ...    Then Broker does not dispatch any notification execution
    ...    And once the downtime is removed Broker dispatches the CRITICAL then RECOVERY notifications
    [Tags]    broker    engine    services    notification    downtimes    MON-187019
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
    Ctn Config Notifications
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    core    info
    # The suppression message asserted below is logged on the notifications
    # channel at debug level, so enable it.
    Ctn Broker Config Log    central    notifications    debug
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
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Wait until Broker has loaded its downtime_manager so the gRPC
    # ScheduleDowntime endpoint is usable (avoids a startup race).
    ${content}    Create List    downtime management enabled, downtime manager loaded
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker did not enable downtime management (notification_mode=broker) in time

    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_1}    ${2}

    # Schedule a downtime on the service via Broker gRPC.
    ${dt_id}    Ctn Broker Schedule Service Downtime    host_1    service_1    ${3600}
    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_1    1    90
    Should Be True    ${result}    service must be in downtime

    # The service becomes CRITICAL HARD while in downtime.
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    # Broker decides not to notify because of the downtime.
    ${content}    Create List    This notifier is currently in a scheduled downtime, so we won't send notifications.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should have suppressed the notification during the downtime

    # And no execution was dispatched to the poller.
    ${content}    Create List    dispatched notification execution for resource
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    15
    Should Not Be True    ${result}    Broker must not dispatch a notification while the service is in downtime

    # Remove the downtime: the CRITICAL notification is now dispatched.
    ${start}    Ctn Get Round Current Date
    Ctn Broker Delete Downtime    ${dt_id}

    ${content}    Create List    dispatched notification execution for resource
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    90
    Should Be True    ${result}    Broker did not dispatch the CRITICAL notification after the downtime removal

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The poller did not run the dispatched CRITICAL notification command

    # Recovery: the service returns OK HARD and a RECOVERY notification is dispatched.
    ${start}    Ctn Get Round Current Date
    Ctn Set Command Status    ${cmd_service_1}    ${0}
    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be OK HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The poller did not run the dispatched RECOVERY notification command

    Ctn Stop Engine
    Ctn Kindly Stop Broker

SRV_REC_NOTIF_AFTER_ACK_BROKER
    [Documentation]    Scenario: in notification_mode=broker, a recovery notification is sent after an acknowledged problem recovers
    ...    Given a service with a contact, in notification_mode=broker (BBDO3)
    ...    When the service enters CRITICAL HARD, is acknowledged, then a new CRITICAL check occurs
    ...    Then Broker suppresses the notification because of the acknowledgement
    ...    And when the service returns to OK HARD Broker dispatches the RECOVERY notification
    [Tags]    broker    engine    services    acknowledgement    notification    MON-187019
    Ctn Clear Commands Status
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
    Ctn Config Notifications
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    core    info
    # The acknowledgement suppression message asserted below is logged on the
    # notifications channel at debug level, so enable it.
    Ctn Broker Config Log    central    notifications    debug
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
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_1}    ${2}

    # The service becomes CRITICAL HARD: Broker dispatches the critical notification.
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The poller did not run the dispatched CRITICAL notification command

    # Acknowledge the problem.
    Ctn Acknowledge Service Problem    host_1    service_1    STICKY
    ${content}    Create List    ACKNOWLEDGE_SVC_PROBLEM;host_1;service_1;2;0;0;admin;Service (host_1,service_1) acknowledged
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The acknowledgement external command was not processed

    # A new CRITICAL check while acknowledged: Broker suppresses the notification.
    ${start}    Ctn Get Round Current Date
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is still CRITICAL
    ${content}    Create List    This notifier problem has been acknowledged, so we won't send notifications.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should have suppressed the notification because of the acknowledgement

    # Recovery: the service returns OK HARD and a RECOVERY notification is dispatched.
    ${start}    Ctn Get Round Current Date
    Ctn Set Command Status    ${cmd_service_1}    ${0}
    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be OK HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    90
    Should Be True    ${result}    The poller did not run the dispatched RECOVERY notification command

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
    Ctn Engine Config Replace Value In Services    0    service_1    max_check_attempts    1
    Ctn Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    check_command    command_4
    Ctn Engine Config Set Value In Services    0    service_2    contact_groups    contactgroup_1
    Ctn Engine Config Replace Value In Services    0    service_2    max_check_attempts    1
    Ctn Engine Config Set Value In Services    0    service_2    notification_options    c
    Ctn Engine Config Set Value In Services    0    service_2    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_2    first_notification_delay    0
    Ctn Engine Config Set Value In Services    0    service_2    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_2    notification_interval    1
    Ctn Engine Config Replace Value In Services    0    service_2    first_notification_delay    0
    Ctn Engine Config Replace Value In Services    0    service_2    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_2    active_checks_enabled    0
    Ctn Engine Config Replace Value In Services    0    service_2    retry_interval    1
    Ctn Engine Config Replace Value In Services    0    service_2    check_command    command_4
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

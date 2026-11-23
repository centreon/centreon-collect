*** Settings ***
Documentation       Notifier settings via Broker gRPC (notification_mode = broker):
...                 notification number and custom notifications. The requests are posted on
...                 the notification dispatcher strand, serialized with the status batches,
...                 so they return before being applied.

Resource    ../resources/import.resource

Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Stop Processes
Test Teardown    Ctn Save Logs If Failed


*** Test Cases ***
BENOTSET1
    [Documentation]    Scenario: service notification number set through Broker
    ...    Given a service with a contact, in notification_mode=broker (BBDO3)
    ...    When the service goes CRITICAL HARD and is notified
    ...    Then services.notification_number is 1
    ...    When SetServiceNotificationNumber(5) is called on Broker
    ...    Then services.notification_number becomes 5 (the manager publishes it)
    ...    And a new CRITICAL result after the interval is notified with number 6
    [Tags]    broker    engine    services    notification    broker_notification_settings
    Ctn Clear Commands Status
    Ctn Config Centralized Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
    Ctn Config Notifications
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    notifications    debug
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_1    notification_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker    newGeneration=${True}
    Ctn Start Engine    newGeneration=${True}
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Wait Until Created    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/1.prot    timeout=30s
    Ctn Wait For Broker Notification Services    ${start}

    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_1}    ${2}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The CRITICAL notification should be executed by the poller
    ${result}    Ctn Check Service Notification Number With Timeout    host_1    service_1    1    30
    Should Be True    ${result}    notification_number should be 1 after the first notification

    Ctn Broker Set Service Notification Number    host_1    service_1    5
    ${result}    Ctn Check Service Notification Number With Timeout    host_1    service_1    5    30
    Should Be True    ${result}    notification_number should be 5 after the Broker call

    # Re-notification after the interval (1 x 60s): the number continues from 5.
    ${start_renotif}    Get Current Date
    Sleep    65s
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is still CRITICAL
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start_renotif}    ${content}    90
    Should Be True    ${result}    The service should be notified again after the interval
    ${result}    Ctn Check Service Notification Number With Timeout    host_1    service_1    6    30
    Should Be True    ${result}    notification_number should continue from the value set through Broker

BENOTSET2
    [Documentation]    Scenario: custom notifications through Broker, plain and forced
    ...    Given a service with a contact, in notification_mode=broker (BBDO3)
    ...    When SendCustomServiceNotification is called on Broker
    ...    Then the poller executes a CUSTOM notification with the given author and comment
    ...    When the service notifications are disabled through Broker and a custom notification is sent
    ...    Then it is refused by the decision
    ...    When the same custom notification is sent with forced set
    ...    Then it bypasses the switch and is executed
    [Tags]    broker    engine    services    notification    broker_notification_settings
    Ctn Clear Commands Status
    Ctn Config Centralized Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
    Ctn Config Notifications
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    notifications    debug
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    0    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    0    service_1    notification_period    24x7
    Ctn Engine Config Set Value In Services    0    service_1    notification_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    check_interval    1
    Ctn Engine Config Replace Value In Services    0    service_1    retry_interval    1
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    ${start}    Get Current Date
    Ctn Start Broker    newGeneration=${True}
    Ctn Start Engine    newGeneration=${True}
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Wait Until Created    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/1.prot    timeout=30s
    Ctn Wait For Broker Notification Services    ${start}

    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${0}    120    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be OK HARD

    ${start_custom}    Get Current Date
    Ctn Broker Send Custom Service Notification    host_1    service_1    admin    A custom notification from Broker
    # Like Engine, the log line does not carry the author/comment of a custom
    # notification (they only feed the $NOTIFICATIONAUTHOR$/$NOTIFICATIONCOMMENT$
    # macros): the executions are told apart by their time window.
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CUSTOM (OK);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start_custom}    ${content}    60
    Should Be True    ${result}    The CUSTOM notification decided by Broker should be executed by the poller

    Ctn Broker Set Service Notifications    host_1    service_1    ${False}
    ${result}    Ctn Check Resource Notifications Enabled With Timeout    host_1    service_1    ${False}    30
    Should Be True    ${result}    the service switch should be off

    ${start_refused}    Get Current Date
    Ctn Broker Send Custom Service Notification    host_1    service_1    admin    Refused custom notification
    ${content}    Create List    Notifications are disabled, so notifications will not be sent out.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start_refused}    ${content}    30
    Should Be True    ${result}    Broker should refuse the plain custom notification while the switch is off
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CUSTOM (OK);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start_refused}    ${content}    10
    Should Not Be True    ${result}    The refused custom notification must not reach the poller

    ${start_forced}    Get Current Date
    Ctn Broker Send Custom Service Notification    host_1    service_1    admin    Forced custom notification    forced=${True}
    ${content}    Create List    This is a forced notification, so we'll send it out.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start_forced}    ${content}    30
    Should Be True    ${result}    Broker should let the forced custom notification through
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CUSTOM (OK);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start_forced}    ${content}    60
    Should Be True    ${result}    The forced custom notification should be executed by the poller


*** Keywords ***
Ctn Wait For Broker Notification Services
    [Documentation]    Wait until Broker has loaded its notification_mode=broker
    ...    services (the acknowledgement manager is the last one loaded), so the
    ...    Broker gRPC notification endpoints are usable.
    [Arguments]    ${start}    ${timeout}=${60}
    ${content}    Create List    acknowledgement management enabled, acknowledgement manager loaded
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    ${timeout}
    Should Be True    ${result}    Broker did not enable notification_mode=broker services in time

Ctn Config Notifications
    [Documentation]    Configuring engine notification settings (same as notifications.robot).
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

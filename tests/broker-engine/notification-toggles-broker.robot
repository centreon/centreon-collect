*** Settings ***
Documentation       Notification switches via Broker gRPC (notification_mode = broker).
...                 Broker owns the notifications-enabled flag of hosts and services:
...                 SetHostNotifications / SetServiceNotifications toggle it in its cache,
...                 the database follows, the switch drives the notification decision and
...                 survives Broker and Engine restarts. Engine is never involved.

Resource    ../resources/import.resource

Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Stop Processes
Test Teardown    Ctn Save Logs If Failed


*** Test Cases ***
BENOTBRK1
    [Documentation]    Scenario: service notifications disabled then enabled through Broker
    ...    Given a service with a contact, in notification_mode=broker (BBDO3)
    ...    When its notifications are disabled through SetServiceNotifications
    ...    Then resources.notifications_enabled drops to 0
    ...    And a CRITICAL HARD result is not notified (Broker logs the disabled reason)
    ...    When its notifications are enabled again
    ...    Then the next CRITICAL result is notified by the poller
    [Tags]    broker    engine    services    notification    broker_notification_toggles
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
    ${result}    Ctn Check Resource Notifications Enabled With Timeout    host_1    service_1    ${True}    60
    Should Be True    ${result}    service_1 should start with notifications enabled

    Ctn Broker Set Service Notifications    host_1    service_1    ${False}
    ${result}    Ctn Check Resource Notifications Enabled With Timeout    host_1    service_1    ${False}    30
    Should Be True    ${result}    resources.notifications_enabled should be 0 after the Broker switch

    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_1}    ${2}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD
    ${content}    Create List    Notifications are disabled, so notifications will not be sent out.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should refuse the notification because the service switch is off
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Not Be True    ${result}    No notification must be executed while the switch is off

    ${start_enable}    Get Current Date
    Ctn Broker Set Service Notifications    host_1    service_1    ${True}
    ${result}    Ctn Check Resource Notifications Enabled With Timeout    host_1    service_1    ${True}    30
    Should Be True    ${result}    resources.notifications_enabled should be back to 1
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is still CRITICAL
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start_enable}    ${content}    90
    Should Be True    ${result}    The CRITICAL notification should be executed once the switch is on again

BENOTBRK2
    [Documentation]    Scenario: host and services switch through Broker survives Broker and Engine restarts
    ...    Given a BBDO3 platform with notification_mode=broker
    ...    When SetHostNotifications(disabled, HOST_AND_SERVICES) is called on host_1
    ...    Then host_1 and its 20 services have notify=0 in the database
    ...    When Broker is restarted, then Engine is restarted
    ...    Then the switches are still off: Broker restores them on the definitions Engine resends
    ...    When SetHostNotifications(enabled, HOST) is called on host_1
    ...    Then only the host is back to notify=1, the services stay off
    [Tags]    broker    engine    hosts    services    notification    broker_notification_toggles
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    cache    info
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Ctn Wait For Broker Notification Services    ${start}
    ${result}    Ctn Check Services Notify Count With Timeout    host_1    ${True}    20    60
    Should Be True    ${result}    the 20 services of host_1 should start with notify=1

    Ctn Broker Set Host Notifications    host_1    ${False}    HOST_AND_SERVICES
    ${result}    Ctn Check Resource Notifications Enabled With Timeout    host_1    ${EMPTY}    ${False}    30
    Should Be True    ${result}    host_1 should have notifications_enabled=0
    ${result}    Ctn Check Services Notify Count With Timeout    host_1    ${False}    20    30
    Should Be True    ${result}    the 20 services of host_1 should have notify=0

    # Broker restart: the switches are persisted in the cache file.
    ${start}    Get Current Date
    Ctn Kindly Stop Broker
    Ctn Start Broker
    Ctn Wait For Broker Notification Services    ${start}
    Sleep    5s
    ${result}    Ctn Check Resource Notifications Enabled With Timeout    host_1    ${EMPTY}    ${False}    5
    Should Be True    ${result}    host_1 switch must survive the Broker restart

    # Engine restart: it resends its definitions with the configured value (1);
    # Broker must put the switches back and re-assert them in the database.
    ${start_engine}    Get Current Date
    Ctn Stop Engine
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start_engine}    ${1}
    Sleep    5s
    ${result}    Ctn Check Resource Notifications Enabled With Timeout    host_1    ${EMPTY}    ${False}    30
    Should Be True    ${result}    host_1 switch must be restored on the definition Engine resent
    ${result}    Ctn Check Services Notify Count With Timeout    host_1    ${False}    20    30
    Should Be True    ${result}    the 20 service switches must be restored after the Engine restart

    Ctn Broker Set Host Notifications    host_1    ${True}    HOST
    ${result}    Ctn Check Resource Notifications Enabled With Timeout    host_1    ${EMPTY}    ${True}    30
    Should Be True    ${result}    host_1 should be back to notifications_enabled=1
    ${result}    Ctn Check Services Notify Count With Timeout    host_1    ${False}    20    5
    Should Be True    ${result}    the HOST scope must not touch the services

BENOTBRK3
    [Documentation]    Scenario: propagation to child hosts through Broker
    ...    Given host_2 and host_3 children of host_1, host_4 child of host_3 (BBDO3, notification_mode=broker)
    ...    When SetHostNotifications(disabled, HOST_AND_CHILDREN) is called on host_1
    ...    Then host_1, host_2, host_3 and host_4 have notifications_enabled=0 and their services are untouched
    ...    When SetHostNotifications(enabled, BEYOND_HOST) is called on host_1
    ...    Then host_2, host_3, host_4 and their services are enabled while host_1 stays disabled
    [Tags]    broker    engine    hosts    notification    broker_notification_toggles
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Engine Config Set Value In Hosts    0    host_2    parents    host_1
    Ctn Engine Config Set Value In Hosts    0    host_3    parents    host_1
    Ctn Engine Config Set Value In Hosts    0    host_4    parents    host_3
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    core    info
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Ctn Wait For Broker Notification Services    ${start}
    ${result}    Ctn Check Services Notify Count With Timeout    host_4    ${True}    20    60
    Should Be True    ${result}    the services of host_4 should start with notify=1

    # Disable the host and its descendants, hosts only.
    Ctn Broker Set Host Notifications    host_1    ${False}    HOST_AND_CHILDREN
    FOR    ${h}    IN    host_1    host_2    host_3    host_4
        ${result}    Ctn Check Resource Notifications Enabled With Timeout    ${h}    ${EMPTY}    ${False}    30
        Should Be True    ${result}    ${h} should be disabled by HOST_AND_CHILDREN on host_1
    END
    ${result}    Ctn Check Resource Notifications Enabled With Timeout    host_5    ${EMPTY}    ${True}    5
    Should Be True    ${result}    host_5 is not a descendant of host_1 and must stay enabled
    ${result}    Ctn Check Services Notify Count With Timeout    host_4    ${True}    20    5
    Should Be True    ${result}    HOST_AND_CHILDREN must not touch the services

    # Disable the services of host_4 first, so BEYOND_HOST has something to re-enable.
    Ctn Broker Set Host Notifications    host_4    ${False}    HOST_AND_SERVICES
    ${result}    Ctn Check Services Notify Count With Timeout    host_4    ${False}    20    30
    Should Be True    ${result}    the services of host_4 should be disabled

    # Enable everything beyond host_1: descendants and their services, not host_1.
    Ctn Broker Set Host Notifications    host_1    ${True}    BEYOND_HOST
    FOR    ${h}    IN    host_2    host_3    host_4
        ${result}    Ctn Check Resource Notifications Enabled With Timeout    ${h}    ${EMPTY}    ${True}    30
        Should Be True    ${result}    ${h} should be enabled by BEYOND_HOST on host_1
    END
    ${result}    Ctn Check Services Notify Count With Timeout    host_4    ${True}    20    30
    Should Be True    ${result}    the services of host_4 should be enabled by BEYOND_HOST
    Sleep    3s
    ${result}    Ctn Check Resource Notifications Enabled With Timeout    host_1    ${EMPTY}    ${False}    5
    Should Be True    ${result}    BEYOND_HOST must leave host_1 itself untouched


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

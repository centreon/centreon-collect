*** Settings ***
Documentation     Contact notification switches and notification timeperiods via Broker gRPC
...               (notification_mode = broker). Broker owns the contacts' host/service
...               notification switches and the notification timeperiods of contacts, hosts
...               and services: SetContact*Notifications, SetContactgroup*Notifications,
...               SetContact*NotificationPeriod and Set{Host,Service}NotificationPeriod
...               change them in its cache, the values drive the contact selection and
...               survive Broker and Engine restarts. Engine is never involved.

Resource          ../resources/import.resource

Suite Setup       Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup        Ctn Stop Processes
Test Teardown     Ctn Save Logs If Failed


*** Test Cases ***
BECNTBRK1
    [Documentation]    Scenario: contact service notifications switch and timeperiod through Broker
    ...    Given a service notifying contact John_Doe, in notification_mode=broker (BBDO3)
    ...    When John_Doe's service notifications are disabled through SetContactServiceNotifications
    ...    Then a CRITICAL HARD result is not notified (Broker filters the contact out)
    ...    When they are enabled again
    ...    Then the next CRITICAL result is notified by the poller
    ...    When John_Doe's service notification period is set to 'none' through SetContactServiceNotificationPeriod
    ...    Then the next CRITICAL result is not notified (contact outside its period)
    ...    When it is set back to '24x7'
    ...    Then the next CRITICAL result is notified
    ...    And an unknown timeperiod or contact is refused with NOT_FOUND
    [Tags]    broker    engine    services    notification    contacts    broker_notification_toggles
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
    Wait Until Created
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/1.prot
    ...    timeout=30s
    Ctn Wait For Broker Notification Services    ${start}
    ${result}    Ctn Check Resource Notifications Enabled With Timeout    host_1    service_1    ${True}    60
    Should Be True    ${result}    service_1 should start with notifications enabled

    # Error paths: unknown contact, unknown timeperiod.
    ${err}    Ctn Broker Set Contact Service Notifications    nobody    ${False}
    Should Contain    ${err}    unknown contact    an unknown contact must be refused
    ${err}    Ctn Broker Set Contact Service Notification Period    John_Doe    nowhere
    Should Contain    ${err}    unknown timeperiod    an unknown timeperiod must be refused

    # 1. Contact switch off: the contact is filtered out, nobody is notified.
    ${err}    Ctn Broker Set Contact Service Notifications    John_Doe    ${False}
    Should Be Empty    ${err}
    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_1}    ${2}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD
    ${content}    Create List    contact 'John_Doe' shouldn't be notified from service
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should filter John_Doe out because its service switch is off
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Not Be True    ${result}    No notification must be executed while the contact switch is off

    # 2. Contact switch on again: notified.
    ${start_enable}    Get Current Date
    ${err}    Ctn Broker Set Contact Service Notifications    John_Doe    ${True}
    Should Be Empty    ${err}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is still CRITICAL
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start_enable}    ${content}    90
    Should Be True    ${result}    The CRITICAL notification should be executed once the contact switch is on again

    # 3. Contact period 'none': the contact is never in its notification period.
    # A recovery closes the ongoing problem first, so the next CRITICAL opens a
    # new one and is evaluated at once (no re-notification interval gate).
    Ctn Set Command Status    ${cmd_service_1}    ${0}
    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be back to OK
    ${start_period}    Get Current Date
    ${err}    Ctn Broker Set Contact Service Notification Period    John_Doe    none
    Should Be Empty    ${err}
    Ctn Set Command Status    ${cmd_service_1}    ${2}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL again
    ${content}    Create List    contact 'John_Doe' shouldn't be notified at this time
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start_period}    ${content}    60
    Should Be True    ${result}    Broker should filter John_Doe out because its period is 'none'
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start_period}    ${content}    10
    Should Not Be True    ${result}    No notification must be executed while the contact period is 'none'

    # 4. Contact period back to 24x7: a new problem is notified.
    Ctn Set Command Status    ${cmd_service_1}    ${0}
    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK again
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be back to OK
    ${start_24x7}    Get Current Date
    ${err}    Ctn Broker Set Contact Service Notification Period    John_Doe    24x7
    Should Be Empty    ${err}
    Ctn Set Command Status    ${cmd_service_1}    ${2}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL once more
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start_24x7}    ${content}    60
    Should Be True    ${result}    The CRITICAL notification should be executed once the contact period is back to 24x7

BECNTBRK2
    [Documentation]    Scenario: contactgroup switch through Broker survives a Broker restart and an Engine restart
    ...    Given a service notifying contactgroup_1 = {John_Doe}, in notification_mode=broker (BBDO3)
    ...    When SetContactgroupServiceNotifications(disabled) is called on contactgroup_1
    ...    Then a CRITICAL HARD result is not notified
    ...    When Broker is restarted, then Engine is restarted (it resends its configuration)
    ...    Then the switch is still off: the next CRITICAL result is not notified
    ...    When SetContactgroupServiceNotifications(enabled) is called
    ...    Then the next CRITICAL result is notified
    [Tags]    broker    engine    services    notification    contacts    broker_notification_toggles
    Ctn Clear Commands Status
    Ctn Config Centralized Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
    Ctn Config Notifications
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    cache    info
    Ctn Broker Config Log    central    notifications    debug
    Ctn Add Contact Group    ${0}    ${1}    ["John_Doe"]
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    d,r
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    0    service_1    contact_groups    contactgroup_1
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
    Wait Until Created
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/1.prot
    ...    timeout=30s
    Ctn Wait For Broker Notification Services    ${start}
    ${result}    Ctn Check Resource Notifications Enabled With Timeout    host_1    service_1    ${True}    60
    Should Be True    ${result}    service_1 should start with notifications enabled

    ${err}    Ctn Broker Set Contactgroup Service Notifications    nogroup    ${False}
    Should Contain    ${err}    unknown contactgroup    an unknown contactgroup must be refused

    # Switch off through the group: its member gets the override.
    ${err}    Ctn Broker Set Contactgroup Service Notifications    contactgroup_1    ${False}
    Should Be Empty    ${err}
    ${content}    Create List    service notifications disabled on contactgroup 'contactgroup_1': 1 contact(s)
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    10
    Should Be True    ${result}    Broker should report one contact toggled through contactgroup_1
    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_1}    ${2}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD
    ${content}    Create List    contact 'John_Doe' shouldn't be notified from service
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should filter John_Doe out because its service switch is off
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Not Be True    ${result}    No notification must be executed while the contact switch is off

    # Broker restart: the contact override is persisted in the cache file.
    ${start_restart}    Get Current Date
    Ctn Kindly Stop Broker
    Ctn Start Broker    newGeneration=${True}
    Ctn Wait For Broker Notification Services    ${start_restart}
    # Engine restart: it resends its configuration, the contact is rebuilt
    # with the configured value; Broker must re-apply the override.
    Ctn Stop Engine
    Ctn Start Engine    newGeneration=${True}
    Ctn Wait For Engine To Be Ready    ${start_restart}    ${1}
    Sleep    5s
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is still CRITICAL
    ${content}    Create List    contact 'John_Doe' shouldn't be notified from service
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start_restart}    ${content}    90
    Should Be True    ${result}    The contact switch must survive the Broker and Engine restarts
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start_restart}    ${content}    10
    Should Not Be True    ${result}    No notification must be executed after the restarts either

    # Switch on again through the group: notified.
    ${start_enable}    Get Current Date
    ${err}    Ctn Broker Set Contactgroup Service Notifications    contactgroup_1    ${True}
    Should Be Empty    ${err}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is still CRITICAL
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start_enable}    ${content}    90
    Should Be True    ${result}    The CRITICAL notification should be executed once the group switch is on again

BECNTBRK3
    [Documentation]    Scenario: service notification timeperiod through Broker
    ...    Given a service notifying John_Doe with notification_period 24x7, in notification_mode=broker (BBDO3)
    ...    When SetServiceNotificationPeriod(none) is called on service_1
    ...    Then services.notification_period is 'none' in the database
    ...    And a CRITICAL HARD result is not notified (service outside its period)
    ...    When Engine is restarted (it resends the configured period)
    ...    Then services.notification_period is still 'none': Broker restores it
    ...    When SetServiceNotificationPeriod(24x7) is called
    ...    Then services.notification_period is back to '24x7' and the next CRITICAL result is notified
    [Tags]    broker    engine    services    notification    timeperiods    broker_notification_toggles
    Ctn Clear Commands Status
    Ctn Config Centralized Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
    Ctn Config Notifications
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    sql    debug
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
    Wait Until Created
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/1.prot
    ...    timeout=30s
    Ctn Wait For Broker Notification Services    ${start}
    ${result}    Ctn Check Service Notification Period With Timeout    host_1    service_1    24x7    60
    Should Be True    ${result}    service_1 should start with notification_period 24x7

    ${err}    Ctn Broker Set Service Notification Period    host_1    service_1    nowhere
    Should Contain    ${err}    unknown timeperiod    an unknown timeperiod must be refused

    # Period 'none': the database follows, the service is never in its period.
    ${err}    Ctn Broker Set Service Notification Period    host_1    service_1    none
    Should Be Empty    ${err}
    ${result}    Ctn Check Service Notification Period With Timeout    host_1    service_1    none    30
    Should Be True    ${result}    services.notification_period should be 'none' after the Broker call
    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_1}    ${2}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD
    ${content}    Create List    This notifier shouldn't have notifications sent out at this time.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should refuse the notification because the service is outside its period
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Not Be True    ${result}    No notification must be executed while the period is 'none'

    # Engine restart: it resends the definition with the configured period (24x7);
    # Broker must put 'none' back and re-assert it in the database.
    ${start_engine}    Get Current Date
    Ctn Stop Engine
    Ctn Start Engine    newGeneration=${True}
    Ctn Wait For Engine To Be Ready    ${start_engine}    ${1}
    Sleep    5s
    ${result}    Ctn Check Service Notification Period With Timeout    host_1    service_1    none    30
    Should Be True    ${result}    the period override must be restored on the definition Engine resent

    # Back to 24x7: notified.
    ${start_24x7}    Get Current Date
    ${err}    Ctn Broker Set Service Notification Period    host_1    service_1    24x7
    Should Be Empty    ${err}
    ${result}    Ctn Check Service Notification Period With Timeout    host_1    service_1    24x7    30
    Should Be True    ${result}    services.notification_period should be back to 24x7
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is still CRITICAL
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start_24x7}    ${content}    90
    Should Be True    ${result}    The CRITICAL notification should be executed once the period is back to 24x7


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

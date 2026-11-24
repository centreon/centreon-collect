*** Settings ***
Documentation     Poller-wide notification switch via Broker gRPC (notification_mode = broker).
...               Broker owns the program-wide "notifications enabled" flag of each poller:
...               SetPollerNotifications toggles it in its cache, the switch blocks every
...               notification decision of the poller's resources and survives Broker and
...               Engine restarts. Engine is never involved.

Resource          ../resources/import.resource

Suite Setup       Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup        Ctn Stop Processes
Test Teardown     Ctn Save Logs If Failed


*** Test Cases ***
BEPOLBRK1
    [Documentation]    Scenario: poller notifications switch through Broker survives restarts
    ...    Given a service notifying John_Doe on poller 1, in notification_mode=broker (BBDO3)
    ...    When the poller notifications are disabled through SetPollerNotifications
    ...    Then a CRITICAL HARD result is not notified (Broker logs that notifications are disabled)
    ...    When Broker is restarted, then Engine is restarted (it resends its configuration with enable_notifications=1)
    ...    Then the switch is still off: the next CRITICAL result is not notified
    ...    When the poller notifications are enabled again
    ...    Then the next CRITICAL result is notified by the poller
    ...    And an unknown poller is refused with NOT_FOUND
    [Tags]    broker    engine    services    notification    pollers    broker_notification_toggles
    Ctn Clear Commands Status
    Ctn Config Centralized Engine    ${1}    ${1}    ${1}
    Ctn Clear Engine White List
    Ctn Config Notifications
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    cache    info
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

    ${err}    Ctn Broker Set Poller Notifications    999    ${False}
    Should Contain    ${err}    unknown poller    an unknown poller must be refused

    # 1. Poller switch off: every decision of the poller's resources is refused.
    ${err}    Ctn Broker Set Poller Notifications    1    ${False}
    Should Be Empty    ${err}
    ${content}    Create List    notifications disabled on poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    10
    Should Be True    ${result}    Broker should log the poller switch
    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_1}    ${2}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD
    ${content}    Create List    Notifications are disabled, so notifications will not be sent out.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should refuse the notification because the poller switch is off
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Not Be True    ${result}    No notification must be executed while the poller switch is off

    # 2. Broker restart (switch persisted in the cache file), then Engine
    # restart (its configuration comes back with enable_notifications=1).
    ${start_restart}    Get Current Date
    Ctn Kindly Stop Broker
    Ctn Start Broker    newGeneration=${True}
    Ctn Wait For Broker Notification Services    ${start_restart}
    Ctn Stop Engine
    Ctn Start Engine    newGeneration=${True}
    Ctn Wait For Engine To Be Ready    ${start_restart}    ${1}
    Sleep    5s
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is still CRITICAL
    ${content}    Create List    Notifications are disabled, so notifications will not be sent out.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start_restart}    ${content}    90
    Should Be True    ${result}    The poller switch must survive the Broker and Engine restarts
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start_restart}    ${content}    10
    Should Not Be True    ${result}    No notification must be executed after the restarts either

    # 3. Poller switch on again: notified.
    ${start_enable}    Get Current Date
    ${err}    Ctn Broker Set Poller Notifications    1    ${True}
    Should Be Empty    ${err}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is still CRITICAL
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start_enable}    ${content}    90
    Should Be True    ${result}    The CRITICAL notification should be executed once the poller switch is on again


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

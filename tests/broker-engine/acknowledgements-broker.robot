*** Settings ***
Documentation       Acknowledgement management via Broker gRPC (notification_mode = broker).
...                 These tests are the counterpart of acknowledgement.robot but with Broker
...                 acting as the acknowledgement authority instead of Engine: Engine is never
...                 told about the acknowledgements.

Resource    ../resources/import.resource

Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Stop Processes
Test Teardown    Ctn Save Logs If Failed


*** Test Cases ***
BEACKBRK1
    [Documentation]    Scenario: Service acknowledgement via Broker gRPC, cleared on recovery
    ...    Given a BBDO3 platform with notification_mode=broker and a CRITICAL HARD service
    ...    When the service is acknowledged through the Broker AcknowledgeServiceProblem RPC
    ...    Then the acknowledgement is stored in the acknowledgements table and in the Broker cache
    ...    And the resources table shows the service acknowledged, and a logs entry is written
    ...    When the service comes back OK
    ...    Then the acknowledgement comment is deleted and the resource is no longer acknowledged
    [Tags]    broker    engine    services    acknowledgement    broker_acknowledgement
    Ctn Config Engine    ${1}    ${50}    ${20}
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
    Ctn Wait For Broker Acknowledgement Manager    ${start}

    ${cmd_id}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_id}    ${2}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    (1;1) is critical
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD

    # Floor, not round: the ack entry_time is compared with >= ${d}.
    ${d}    Ctn Get Round Current Date
    Ctn Broker Acknowledge Service Problem    host_1    service_1
    ${ack_id}    Ctn Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 1).
    ${result}    Ctn Check Acknowledgement In Cache With Timeout    1    1    51001    30
    Should Be True    ${result}    The acknowledgement should be in the Broker cache
    ${result}    Ctn Check Resource Acknowledged With Timeout    host_1    service_1    ${True}    30
    Should Be True    ${result}    resources.acknowledged should be set on service (1;1)
    ${result}    Ctn Check Acknowledgement In Logs Table    ${d}    30
    Should Be True    ${result}    A service acknowledgement entry should be in the logs table

    # Engine keeps sending statuses that know nothing about the acknowledgement:
    # they must not clear the flag written by Broker.
    Ctn Process Service Result Hard    host_1    service_1    ${2}    (1;1) is still critical
    Sleep    5s
    ${result}    Ctn Check Resource Acknowledged With Timeout    host_1    service_1    ${True}    5
    Should Be True    ${result}    An Engine status must not clear the acknowledgement owned by Broker

    Ctn Set Command Status    ${cmd_id}    ${0}
    Ctn Process Service Result Hard    host_1    service_1    ${0}    (1;1) is OK
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (1;1) should be OK HARD

    # Like Engine: on recovery the comment is deleted and the flag cleared.
    ${result}    Ctn Check Acknowledgement Is Deleted With Timeout    ${ack_id}    30
    Should Be True    ${result}    Acknowledgement ${ack_id} comment should be deleted.
    ${result}    Ctn Check Resource Acknowledged With Timeout    host_1    service_1    ${False}    30
    Should Be True    ${result}    resources.acknowledged should be cleared on service (1;1)
    ${result}    Ctn Check Acknowledgements Count With Timeout    0    51001    30
    Should Be True    ${result}    The Broker cache should hold no acknowledgement any more

BEACKBRK2
    [Documentation]    Scenario: Service acknowledgement removed via Broker gRPC
    ...    Given a CRITICAL HARD service acknowledged through Broker (notification_mode=broker)
    ...    When the acknowledgement is removed through the Broker RemoveServiceAcknowledgement RPC
    ...    Then both the acknowledgements and comments rows get their deletion_time
    ...    And the resource is no longer acknowledged
    [Tags]    broker    engine    services    acknowledgement    broker_acknowledgement
    Ctn Config Engine    ${1}    ${50}    ${20}
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
    Ctn Wait For Broker Acknowledgement Manager    ${start}

    ${cmd_id}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_id}    ${2}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    (1;1) is critical
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD

    # Floor, not round: the ack entry_time is compared with >= ${d}.
    ${d}    Ctn Get Round Current Date
    Ctn Broker Acknowledge Service Problem    host_1    service_1
    ${ack_id}    Ctn Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 1).
    ${result}    Ctn Check Resource Acknowledged With Timeout    host_1    service_1    ${True}    30
    Should Be True    ${result}    resources.acknowledged should be set on service (1;1)

    Ctn Broker Remove Service Acknowledgement    host_1    service_1

    ${result}    Ctn Check Acknowledgement Is Deleted With Timeout    ${ack_id}    30    BOTH
    Should Be True    ${result}    Acknowledgement ${ack_id} should be deleted in both tables.
    ${result}    Ctn Check Resource Acknowledged With Timeout    host_1    service_1    ${False}    30
    Should Be True    ${result}    resources.acknowledged should be cleared on service (1;1)

BEACKBRK3
    [Documentation]    Scenario: Sticky versus normal acknowledgement on a state change (Broker authority)
    ...    Given a CRITICAL HARD service with a normal acknowledgement set through Broker
    ...    When the service goes WARNING HARD
    ...    Then the normal acknowledgement is cleared
    ...    Given the WARNING service is then acknowledged sticky
    ...    When the service goes CRITICAL HARD
    ...    Then the sticky acknowledgement is kept
    ...    When the service comes back OK
    ...    Then the sticky acknowledgement is cleared
    [Tags]    broker    engine    services    acknowledgement    broker_acknowledgement
    Ctn Config Engine    ${1}    ${50}    ${20}
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
    Ctn Wait For Broker Acknowledgement Manager    ${start}

    ${cmd_id}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_id}    ${2}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    (1;1) is critical
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD

    # Normal acknowledgement: cleared by any state change.
    # Floor, not round: the ack entry_time is compared with >= ${d}.
    ${d}    Ctn Get Round Current Date
    Ctn Broker Acknowledge Service Problem    host_1    service_1
    ${ack_id}    Ctn Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 1).
    ${result}    Ctn Check Resource Acknowledged With Timeout    host_1    service_1    ${True}    30
    Should Be True    ${result}    resources.acknowledged should be set on service (1;1)

    Ctn Set Command Status    ${cmd_id}    ${1}
    Ctn Process Service Result Hard    host_1    service_1    ${1}    (1;1) is warning
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${1}    60    HARD
    Should Be True    ${result}    Service (1;1) should be warning HARD
    ${result}    Ctn Check Acknowledgement Is Deleted With Timeout    ${ack_id}    30
    Should Be True    ${result}    The normal acknowledgement ${ack_id} should be cleared on state change.
    ${result}    Ctn Check Resource Acknowledged With Timeout    host_1    service_1    ${False}    30
    Should Be True    ${result}    resources.acknowledged should be cleared after the state change

    # Sticky acknowledgement: kept until recovery.
    # Floor, not round: the ack entry_time is compared with >= ${d}.
    ${d}    Ctn Get Round Current Date
    Ctn Broker Acknowledge Service Problem    host_1    service_1    sticky=${True}
    ${ack_id}    Ctn Check Acknowledgement With Timeout    host_1    service_1    ${d}    1    60    HARD
    Should Be True    ${ack_id} > 0    No sticky acknowledgement on service (1, 1).
    ${result}    Ctn Check Resource Acknowledged With Timeout    host_1    service_1    ${True}    30
    Should Be True    ${result}    resources.acknowledged should be set on service (1;1)

    Ctn Set Command Status    ${cmd_id}    ${2}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    (1;1) is critical again
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD
    Sleep    5s
    ${result}    Ctn Check Resource Acknowledged With Timeout    host_1    service_1    ${True}    5
    Should Be True    ${result}    The sticky acknowledgement must survive a WARNING to CRITICAL change
    ${result}    Ctn Check Acknowledgement In Cache With Timeout    1    1    51001    5
    Should Be True    ${result}    The sticky acknowledgement should still be in the Broker cache

    Ctn Set Command Status    ${cmd_id}    ${0}
    Ctn Process Service Result Hard    host_1    service_1    ${0}    (1;1) is OK
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (1;1) should be OK HARD
    ${result}    Ctn Check Acknowledgement Is Deleted With Timeout    ${ack_id}    30
    Should Be True    ${result}    The sticky acknowledgement ${ack_id} should be cleared on recovery.
    ${result}    Ctn Check Resource Acknowledged With Timeout    host_1    service_1    ${False}    30
    Should Be True    ${result}    resources.acknowledged should be cleared on recovery

BEACKBRK4
    [Documentation]    Scenario: Host acknowledgement via Broker gRPC survives a Broker restart
    ...    Given a DOWN HARD host acknowledged through the Broker AcknowledgeHostProblem RPC
    ...    Then a host acknowledgement entry is written in the logs table
    ...    When Broker is restarted
    ...    Then the acknowledgement is still in the Broker cache and the host still acknowledged
    ...    And a new Engine status does not clear it
    ...    When Engine is restarted and resends its definitions
    ...    Then Broker restores the acknowledgement flag on the fresh host definition
    ...    When the acknowledgement is removed through the Broker RemoveHostAcknowledgement RPC
    ...    Then the host is no longer acknowledged
    [Tags]    broker    engine    hosts    acknowledgement    broker_acknowledgement
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
    Ctn Set Hosts Passive    ${0}    host_1
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    Ctn Wait For Broker Acknowledgement Manager    ${start}

    FOR    ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    1    host_1 is DOWN
    END
    ${result}    Ctn Check Host Status    host_1    1    1    ${True}    60
    Should Be True    ${result}    host_1 should be DOWN HARD

    # Floor, not round: the ack entry_time is compared with >= ${d}.
    ${d}    Ctn Get Round Current Date
    Ctn Broker Acknowledge Host Problem    host_1    sticky=${True}
    ${result}    Ctn Check Acknowledgement In Cache With Timeout    1    0    51001    30
    Should Be True    ${result}    The host acknowledgement should be in the Broker cache
    ${result}    Ctn Check Resource Acknowledged With Timeout    host_1    ${EMPTY}    ${True}    30
    Should Be True    ${result}    resources.acknowledged should be set on host_1
    ${result}    Ctn Check Acknowledgement In Logs Table    ${d}    30    11
    Should Be True    ${result}    A host acknowledgement entry should be in the logs table

    ${start}    Get Current Date
    Ctn Kindly Stop Broker
    Ctn Start Broker
    Ctn Wait For Broker Acknowledgement Manager    ${start}

    ${result}    Ctn Check Acknowledgement In Cache With Timeout    1    0    51001    60
    Should Be True    ${result}    The host acknowledgement should survive the Broker restart

    FOR    ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    1    host_1 is still DOWN
    END
    Sleep    5s
    ${result}    Ctn Check Resource Acknowledged With Timeout    host_1    ${EMPTY}    ${True}    5
    Should Be True    ${result}    An Engine status must not clear the acknowledgement after the restart

    # Engine restart: it resends its definitions, which know nothing about the
    # acknowledgement (type NONE). Broker must restore the flag it owns on the
    # fresh definition, in its cache and in the database.
    ${start_engine}    Get Current Date
    Ctn Stop Engine
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start_engine}    ${1}
    ${content}    Create List    acknowledgement flag restored on host 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start_engine}    ${content}    60
    Should Be True    ${result}    Broker should restore the acknowledgement flag on the definition Engine resent
    ${result}    Ctn Check Acknowledgement In Cache With Timeout    1    0    51001    30
    Should Be True    ${result}    The host acknowledgement should survive the Engine restart
    Sleep    5s
    ${result}    Ctn Check Resource Acknowledged With Timeout    host_1    ${EMPTY}    ${True}    5
    Should Be True    ${result}    resources.acknowledged must be set again after the Engine restart

    Ctn Broker Remove Host Acknowledgement    host_1
    ${result}    Ctn Check Resource Acknowledged With Timeout    host_1    ${EMPTY}    ${False}    30
    Should Be True    ${result}    resources.acknowledged should be cleared on host_1
    ${result}    Ctn Check Acknowledgements Count With Timeout    0    51001    30
    Should Be True    ${result}    The Broker cache should hold no acknowledgement any more

BEACKBRK5
    [Documentation]    Scenario: Acknowledgement drives the Broker notification decision
    ...    Given a service with a contact, in notification_mode=broker (BBDO3)
    ...    When the service goes CRITICAL HARD
    ...    Then Broker dispatches the CRITICAL notification
    ...    When the service is acknowledged through Broker with notify set
    ...    Then Broker dispatches the ACKNOWLEDGEMENT notification
    ...    And the next CRITICAL result is not notified because the problem is acknowledged
    ...    When the service comes back OK
    ...    Then Broker dispatches the RECOVERY notification
    [Tags]    broker    engine    services    notification    acknowledgement    broker_acknowledgement
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
    Ctn Wait For Broker Acknowledgement Manager    ${start}

    ${cmd_service_1}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_service_1}    ${2}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be CRITICAL HARD

    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The CRITICAL notification decided by Broker should be executed by the poller

    # Acknowledge with notify: Broker decides the acknowledgement notification,
    # the poller executes it.
    ${start_ack}    Get Current Date
    Ctn Broker Acknowledge Service Problem    host_1    service_1    notify=${True}
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;ACKNOWLEDGEMENT (CRITICAL);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start_ack}    ${content}    60
    Should Be True    ${result}    The ACKNOWLEDGEMENT notification decided by Broker should be executed by the poller

    # The problem stays CRITICAL: Broker must not re-notify while acknowledged.
    ${start_renotif}    Get Current Date
    Sleep    65s
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is still CRITICAL
    ${content}    Create List    This notifier problem has been acknowledged, so we won't send
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start_renotif}    ${content}    60
    Should Be True    ${result}    Broker should suppress the notification while the problem is acknowledged

    # Recovery: the acknowledgement is cleared and the RECOVERY notification dispatched.
    ${start_recovery}    Get Current Date
    Ctn Set Command Status    ${cmd_service_1}    ${0}
    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${0}    60    HARD
    Should Be True    ${result}    Service (host_1,service_1) should be OK HARD
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;RECOVERY (OK);command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start_recovery}    ${content}    60
    Should Be True    ${result}    The RECOVERY notification decided by Broker should be executed by the poller
    ${result}    Ctn Check Acknowledgements Count With Timeout    0    51001    30
    Should Be True    ${result}    The acknowledgement should be cleared on recovery


*** Keywords ***
Ctn Wait For Broker Acknowledgement Manager
    [Documentation]    Wait until Broker has loaded its acknowledgement manager
    ...    (notification_mode=broker) so the gRPC acknowledgement endpoints are
    ...    usable. Avoids a startup race where a gRPC call would be rejected with
    ...    "Acknowledgement management is not enabled".
    [Arguments]    ${start}    ${timeout}=${60}
    ${content}    Create List    acknowledgement management enabled, acknowledgement manager loaded
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    ${timeout}
    Should Be True    ${result}    Broker did not enable acknowledgement management (notification_mode=broker) in time

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

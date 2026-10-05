*** Settings ***
Documentation       Legacy external commands routed by Broker (ExecuteExternalCommand gRPC).
...                 PHP sends the legacy command line to Broker, which resolves the
...                 poller supervising the targeted host and forwards the line down
...                 the BBDO channel; the poller executes it with its command pipe
...                 parser. Passive check results run at once on the receiving thread,
...                 the other commands wait for the Engine event loop.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BEEXTBRK1
    [Documentation]    Scenario: forced checks and passive results reach the poller owning the host
    ...    Given two pollers in centralized configuration (host_1 on poller 1, host_26 on poller 2)
    ...    When SCHEDULE_FORCED_SVC_CHECK on host_26/service_501 is sent to Broker
    ...    Then poller 2 logs the external command and poller 1 does not
    ...    When SCHEDULE_FORCED_HOST_CHECK on host_1 is sent to Broker
    ...    Then poller 1 logs the external command
    ...    When PROCESS_SERVICE_CHECK_RESULT CRITICAL on host_26/service_501 is sent to Broker
    ...    Then the service becomes CRITICAL in the database
    [Tags]    broker    engine    external_commands    broker_external_commands
    Ctn Config Centralized Engine    ${2}    ${50}    ${20}
    Ctn Clear Prot Files
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${2}
    Ctn Config BBDO3    2
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${2}
    ${content}    Create List    BBDO: all engine peers have acknowledged their configuration
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    The two pollers did not acknowledge their configuration

    # 1. A service command on host_26 lands on poller 2 only.
    ${err}    Ctn Broker Execute External Command    SCHEDULE_FORCED_SVC_CHECK;host_26;service_501;${start}
    Should Be Empty    ${err}
    ${content}    Create List    external command SCHEDULE_FORCED_SVC_CHECK routed to poller 2
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    10
    Should Be True    ${result}    Broker should log the routing to poller 2
    ${content}    Create List    EXTERNAL COMMAND: SCHEDULE_FORCED_SVC_CHECK;host_26;service_501;
    ${result}    Ctn Find In Log With Timeout    ${engineLog1}    ${start}    ${content}    10
    Should Be True    ${result}    Poller 2 should have received the forced service check
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    5
    Should Not Be True    ${result}    Poller 1 must not receive a command on a host it does not supervise

    # 2. A host command on host_1 lands on poller 1.
    ${err}    Ctn Broker Execute External Command    SCHEDULE_FORCED_HOST_CHECK;host_1;${start}
    Should Be Empty    ${err}
    ${content}    Create List    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    Poller 1 should have received the forced host check

    # 3. A passive check result is executed by poller 2 (immediate path).
    ${err}    Ctn Broker Execute External Command    PROCESS_SERVICE_CHECK_RESULT;host_26;service_501;2;service_501 is CRITICAL via Broker
    Should Be Empty    ${err}
    ${result}    Ctn Check Service Resource Status With Timeout    host_26    service_501    ${2}    60
    Should Be True    ${result}    Service (host_26, service_501) should be CRITICAL after the passive result routed by Broker

BEEXTBRK2
    [Documentation]    Scenario: Broker refuses what it cannot route, synchronously
    ...    Given one poller in centralized configuration and notification_mode=broker
    ...    When an unknown command, a command on an unknown host or service, a malformed timestamp are sent
    ...    Then each is refused with an explicit error and nothing reaches the poller
    ...    When a hostgroup command is sent, or a global command without poller, or on an unknown poller
    ...    Then each is refused with an explicit error
    ...    When a native command is malformed or names an unknown host
    ...    Then Broker, which executes it itself in notification_mode=broker, refuses it with the conversion or RPC error
    [Tags]    broker    engine    external_commands    broker_external_commands
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Clear Prot Files
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    bbdo    info
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${content}    Create List    BBDO: all engine peers have acknowledged their configuration
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    The poller did not acknowledge its configuration

    ${err}    Ctn Broker Execute External Command    NOT_A_COMMAND;host_1
    Should Contain    ${err}    unknown external command
    ${err}    Ctn Broker Execute External Command    ${EMPTY}
    Should Contain    ${err}    empty external command
    ${err}    Ctn Broker Execute External Command    [abc] SCHEDULE_FORCED_HOST_CHECK;host_1;1
    Should Contain    ${err}    malformed timestamp
    ${err}    Ctn Broker Execute External Command    SCHEDULE_FORCED_HOST_CHECK
    Should Contain    ${err}    expects a host name
    ${err}    Ctn Broker Execute External Command    SCHEDULE_FORCED_HOST_CHECK;host_999;1
    Should Contain    ${err}    unknown host 'host_999'
    ${err}    Ctn Broker Execute External Command    SCHEDULE_FORCED_SVC_CHECK;host_1;service_999;1
    Should Contain    ${err}    unknown service
    ${err}    Ctn Broker Execute External Command    ENABLE_HOSTGROUP_HOST_CHECKS;hostgroup_1
    Should Contain    ${err}    not routed by Broker
    ${err}    Ctn Broker Execute External Command    DEL_HOST_DOWNTIME_FULL;host_1;1;2;1;0;60;admin;c
    Should Contain    ${err}    has no exact DeleteDowntime counterpart
    ${err}    Ctn Broker Execute External Command    ENABLE_EVENT_HANDLERS
    Should Contain    ${err}    the request must name the poller
    ${err}    Ctn Broker Execute External Command    ENABLE_EVENT_HANDLERS    poller=999
    Should Contain    ${err}    poller 999 is not connected
    ${err}    Ctn Broker Execute External Command    ENABLE_EVENT_HANDLERS    poller=nowhere
    Should Contain    ${err}    unknown poller 'nowhere'
    # Natives are executed by Broker itself: an argument error is a conversion
    # error, a wrong target is the typed RPC's error.
    ${err}    Ctn Broker Execute External Command    ACKNOWLEDGE_SVC_PROBLEM;host_1;service_1;2;1;1;admin
    Should Contain    ${err}    missing argument 'comment'
    ${err}    Ctn Broker Execute External Command    DISABLE_HOST_NOTIFICATIONS;host_999
    Should Contain    ${err}    could not find host

    ${content}    Create List    EXTERNAL COMMAND:
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    5
    Should Not Be True    ${result}    No refused command may reach the poller

    # The same forced check with an explicit timestamp prefix is accepted.
    ${err}    Ctn Broker Execute External Command    [${start}] SCHEDULE_FORCED_HOST_CHECK;host_1;${start}
    Should Be Empty    ${err}
    ${content}    Create List    EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    The forced host check should reach the poller

BEEXTBRK3
    [Documentation]    Scenario: global, process and contact commands find their pollers
    ...    Given two pollers in centralized configuration
    ...    When DISABLE_EVENT_HANDLERS is sent to Broker for poller 2
    ...    Then poller 2 logs the command and poller 1 does not
    ...    When DISABLE_CONTACT_HOST_NOTIFICATIONS is sent to Broker
    ...    Then both pollers log it (every poller has its copy of the contacts)
    ...    When a service command is sent with stale names but Broker knows the ids
    ...    Then the poller executes it on the right service (names rewritten from the ids)
    [Tags]    broker    engine    external_commands    broker_external_commands
    Ctn Config Centralized Engine    ${2}    ${50}    ${20}
    Ctn Clear Prot Files
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${1}    contacts.cfg
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${2}
    Ctn Config BBDO3    2
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    bbdo    info
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${2}
    ${content}    Create List    BBDO: all engine peers have acknowledged their configuration
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    The two pollers did not acknowledge their configuration

    # 1. Global command addressed to poller 2 only.
    ${err}    Ctn Broker Execute External Command    DISABLE_EVENT_HANDLERS    poller=2
    Should Be Empty    ${err}
    ${content}    Create List    EXTERNAL COMMAND: DISABLE_EVENT_HANDLERS;
    ${result}    Ctn Find In Log With Timeout    ${engineLog1}    ${start}    ${content}    10
    Should Be True    ${result}    Poller 2 should have received the global command
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    5
    Should Not Be True    ${result}    Poller 1 must not receive a global command addressed to poller 2

    # 2. Contact command broadcast to both pollers.
    ${err}    Ctn Broker Execute External Command    DISABLE_CONTACT_HOST_NOTIFICATIONS;John_Doe
    Should Be Empty    ${err}
    ${content}    Create List    external command DISABLE_CONTACT_HOST_NOTIFICATIONS broadcast to 2 poller(s)
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    10
    Should Be True    ${result}    Broker should log the broadcast
    ${content}    Create List    EXTERNAL COMMAND: DISABLE_CONTACT_HOST_NOTIFICATIONS;John_Doe
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    Poller 1 should have received the contact command
    ${result}    Ctn Find In Log With Timeout    ${engineLog1}    ${start}    ${content}    10
    Should Be True    ${result}    Poller 2 should have received the contact command

    # 3. Ids win over names: Broker resolved host_26/service_501, the poller
    # rewrites the names before parsing, so the log shows the real names.
    ${err}    Ctn Broker Execute External Command    PROCESS_SERVICE_CHECK_RESULT;host_26;service_501;2;critical via ids
    Should Be Empty    ${err}
    ${result}    Ctn Check Service Resource Status With Timeout    host_26    service_501    ${2}    60
    Should Be True    ${result}    Service (host_26, service_501) should be CRITICAL

BEEXTBRK4
    [Documentation]    Scenario: in notification_mode=broker, Broker executes the legacy natives itself
    ...    Given a BBDO3 platform with notification_mode=broker and a CRITICAL HARD service
    ...    When ACKNOWLEDGE_SVC_PROBLEM is sent as a legacy line to ExecuteExternalCommand
    ...    Then the acknowledgement is stored as if AcknowledgeServiceProblem had been called
    ...    When SCHEDULE_SVC_DOWNTIME and then DEL_SVC_DOWNTIME are sent as legacy lines
    ...    Then the downtime appears and disappears in the database
    ...    When DISABLE_SVC_NOTIFICATIONS is sent as a legacy line
    ...    Then the service notifications are disabled in the Broker cache and the database
    [Tags]    broker    engine    external_commands    broker_external_commands    broker_notification
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Clear Prot Files
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    Ctn Clear Db    downtimes

    ${start}    Get Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${content}    Create List    acknowledgement management enabled, acknowledgement manager loaded
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker did not enable its notification_mode=broker services in time

    ${cmd_id}    Ctn Get Service Command Id    ${1}
    Ctn Set Command Status    ${cmd_id}    ${2}
    Ctn Process Service Result Hard    host_1    service_1    ${2}    (1;1) is critical
    ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_1    ${2}    60    HARD
    Should Be True    ${result}    Service (1;1) should be critical HARD

    # 1. Acknowledgement through the legacy line.
    ${d}    Ctn Get Round Current Date
    ${err}    Ctn Broker Execute External Command    ACKNOWLEDGE_SVC_PROBLEM;host_1;service_1;2;1;1;admin;acked through the legacy line
    Should Be Empty    ${err}
    ${content}    Create List    external command ACKNOWLEDGE_SVC_PROBLEM executed by Broker as AcknowledgeServiceProblem
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    10
    Should Be True    ${result}    Broker should log that it executed the native itself
    ${ack_id}    Ctn Check Acknowledgement With Timeout    host_1    service_1    ${d}    2    60    HARD
    Should Be True    ${ack_id} > 0    No acknowledgement on service (1, 1).
    ${result}    Ctn Check Resource Acknowledged With Timeout    host_1    service_1    ${True}    30
    Should Be True    ${result}    resources.acknowledged should be set on service (1;1)
    ${content}    Create List    EXTERNAL COMMAND: ACKNOWLEDGE_SVC_PROBLEM
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    5
    Should Not Be True    ${result}    The native must not reach the poller

    # 2. Downtime through the legacy lines.
    ${now}    Ctn Get Round Current Date
    ${end}    Evaluate    ${now} + 3600
    ${err}    Ctn Broker Execute External Command    SCHEDULE_SVC_DOWNTIME;host_1;service_1;${now};${end};1;0;3600;admin;legacy downtime
    Should Be Empty    ${err}
    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_1    1    ${60}
    Should Be True    ${result}    Service (1;1) should be in downtime
    ${result}    Ctn Check Number Of Downtimes    ${1}    ${now}    ${60}
    Should Be True    ${result}    One downtime should be stored
    ${dt_id}    Ctn Get Downtime Id    host_1    service_1
    ${err}    Ctn Broker Execute External Command    DEL_SVC_DOWNTIME;${dt_id}
    Should Be Empty    ${err}
    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_1    0    ${60}
    Should Be True    ${result}    Service (1;1) should no longer be in downtime

    # 3. Notification switch through the legacy line.
    ${err}    Ctn Broker Execute External Command    DISABLE_SVC_NOTIFICATIONS;host_1;service_1
    Should Be Empty    ${err}
    ${result}    Ctn Check Resource Notifications Enabled With Timeout    host_1    service_1    ${False}    60
    Should Be True    ${result}    service_1 notifications should be disabled by the legacy line
    ${err}    Ctn Broker Execute External Command    ENABLE_SVC_NOTIFICATIONS;host_1;service_1
    Should Be Empty    ${err}
    ${result}    Ctn Check Resource Notifications Enabled With Timeout    host_1    service_1    ${True}    60
    Should Be True    ${result}    service_1 notifications should be enabled again

BEEXTBRK_ADAPTIVE
    [Documentation]    Scenario: a check toggle sent to a centralized poller reaches the database
    ...    Given one poller in centralized configuration, external commands routed by Broker
    ...    When DISABLE_HOST_CHECK on host_1 and DISABLE_SVC_CHECK on host_1/service_1 are sent to Broker
    ...    Then the poller applies them and sends the adaptive events back, so the hosts, services and resources tables show the checks disabled
    ...    When ENABLE_HOST_CHECK and ENABLE_SVC_CHECK are sent
    ...    Then the tables show the checks enabled again
    [Tags]    broker    engine    external_commands    broker_external_commands    MON-187019
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Clear Prot Files
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    neb    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    Ctn Clear Db    hosts
    Ctn Clear Db    services
    Ctn Clear Db    resources

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${content}    Create List    BBDO: all engine peers have acknowledged their configuration
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    The poller did not acknowledge its configuration

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    ${err}    Ctn Broker Execute External Command    DISABLE_HOST_CHECK;host_1
    Should Be Empty    ${err}
    ${err}    Ctn Broker Execute External Command    DISABLE_SVC_CHECK;host_1;service_1
    Should Be Empty    ${err}
    FOR    ${index}    IN RANGE    30
        ${output}    Query    SELECT h.active_checks, s.active_checks, r.active_checks_enabled FROM hosts h JOIN services s ON s.host_id=h.host_id JOIN resources r ON r.id=h.host_id AND r.parent_id=0 WHERE h.name='host_1' AND s.description='service_1'
        IF    "${output}" == "((0, 0, 0),)"    BREAK
        Sleep    1s
    END
    Should Be Equal As Strings    ${output}    ((0, 0, 0),)    The check toggles should reach the hosts, services and resources tables

    ${err}    Ctn Broker Execute External Command    ENABLE_HOST_CHECK;host_1
    Should Be Empty    ${err}
    ${err}    Ctn Broker Execute External Command    ENABLE_SVC_CHECK;host_1;service_1
    Should Be Empty    ${err}
    FOR    ${index}    IN RANGE    30
        ${output}    Query    SELECT h.active_checks, s.active_checks, r.active_checks_enabled FROM hosts h JOIN services s ON s.host_id=h.host_id JOIN resources r ON r.id=h.host_id AND r.parent_id=0 WHERE h.name='host_1' AND s.description='service_1'
        IF    "${output}" == "((1, 1, 1),)"    BREAK
        Sleep    1s
    END
    Should Be Equal As Strings    ${output}    ((1, 1, 1),)    The checks should be enabled again

    Disconnect From Database
    [Teardown]    Ctn Stop Engine Broker And Save Logs

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
    ...    When an acknowledgement or a notification switch is sent
    ...    Then it is refused because Broker owns it in notification_mode=broker, naming the gRPC method to use
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
    ${err}    Ctn Broker Execute External Command    DEL_SVC_DOWNTIME;12
    Should Contain    ${err}    use the DeleteDowntime gRPC method
    ${err}    Ctn Broker Execute External Command    ENABLE_EVENT_HANDLERS
    Should Contain    ${err}    the request must name the poller
    ${err}    Ctn Broker Execute External Command    ENABLE_EVENT_HANDLERS    poller=999
    Should Contain    ${err}    poller 999 is not connected
    ${err}    Ctn Broker Execute External Command    ENABLE_EVENT_HANDLERS    poller=nowhere
    Should Contain    ${err}    unknown poller 'nowhere'
    ${err}    Ctn Broker Execute External Command    ACKNOWLEDGE_SVC_PROBLEM;host_1;service_1;2;1;1;admin;ack
    Should Contain    ${err}    use the AcknowledgeServiceProblem gRPC method
    ${err}    Ctn Broker Execute External Command    DISABLE_HOST_NOTIFICATIONS;host_1
    Should Contain    ${err}    use the SetHostNotifications gRPC method

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

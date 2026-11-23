*** Settings ***
Documentation       Downstream remote actions over the BBDO stream (gorgone-less PoC):
...                 central cbd remote API -> poller cbmod -> Engine external commands.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
BERA1
    [Documentation]    Given a poller with remote_actions, when the web posts commands to the central remote API, then the
    ...    engine executes them and each command gets its own status (APPLIED, REJECTED for an unknown host).
    [Tags]    broker    engine    remote_actions
    Ctn Config Remote Actions    ${True}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}
    ${found}    Ctn Remote Api Wait Poller    1    60
    Should Be True    ${found}    The poller engine does not announce remote actions.

    ${code}    ${elapsed}    ${answer}    Ctn Remote Api Send Commands    1
    ...    [TS] SCHEDULE_FORCED_SVC_CHECK;host_1;service_1;TS
    ...    [TS] SCHEDULE_FORCED_SVC_CHECK;no_such_host;service_1;TS
    Should Be Equal As Integers    ${code}    200
    ${item}    Set Variable    ${answer}[items][0]
    Should Be Equal As Strings    ${item}[status]    REJECTED
    Should Be Equal As Strings    ${item}[results][0][status]    APPLIED
    Should Be Equal As Strings    ${item}[results][1][status]    REJECTED
    ${content}    Create List    EXTERNAL COMMAND: SCHEDULE_FORCED_SVC_CHECK;host_1;service_1
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    The forced check is not in the engine log.

BERA2
    [Documentation]    Given the poller engine is stopped, when the web posts a command, then the central remote API
    ...    answers NOT_CONNECTED at once, and nothing is replayed when the poller comes back.
    [Tags]    broker    engine    remote_actions
    Ctn Config Remote Actions    ${True}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}
    ${found}    Ctn Remote Api Wait Poller    1    60
    Should Be True    ${found}    The poller engine does not announce remote actions.
    Ctn Stop Engine

    ${code}    ${elapsed}    ${answer}    Ctn Remote Api Send Commands    1
    ...    [TS] SCHEDULE_FORCED_SVC_CHECK;host_1;service_2;TS
    Should Be Equal As Integers    ${code}    200
    Should Be Equal As Strings    ${answer}[items][0][status]    NOT_CONNECTED
    Should Be True    ${elapsed} < 30    The answer took ${elapsed}s.

    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}
    ${found}    Ctn Remote Api Wait Poller    1    60
    Should Be True    ${found}    The poller engine does not come back.
    ${content}    Create List    EXTERNAL COMMAND: SCHEDULE_FORCED_SVC_CHECK;host_1;service_2
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    15
    Should Not Be True    ${result}    The command sent while disconnected was replayed.

BERA3
    [Documentation]    Given a poller without remote_actions, when the web posts a command, then the central remote API
    ...    answers PEER_TOO_OLD.
    [Tags]    broker    engine    remote_actions
    Ctn Config Remote Actions    ${False}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}
    ${found}    Ctn Remote Api Wait Poller    1    60    ${False}
    Should Be True    ${found}    The poller engine is not connected.

    ${code}    ${elapsed}    ${answer}    Ctn Remote Api Send Commands    1
    ...    [TS] SCHEDULE_FORCED_SVC_CHECK;host_1;service_3;TS
    Should Be Equal As Integers    ${code}    200
    Should Be Equal As Strings    ${answer}[items][0][status]    PEER_TOO_OLD

BERA4
    [Documentation]    Given the poller connects over gRPC with TLS and verifies the central certificate against its CA
    ...    (remote_actions_insecure off), when the web posts a command, then the engine executes it.
    [Tags]    broker    engine    remote_actions    tls
    Ctn Config Remote Actions    ${True}    ${False}
    Ctn Create Key And Certificate    localhost    ${EtcRoot}/centreon-broker/ra-central.key
    ...    ${EtcRoot}/centreon-broker/ra-central.crt    ${True}
    Ctn Config Remote Actions Tls    ${EtcRoot}/centreon-broker/ra-central.crt
    ...    ${EtcRoot}/centreon-broker/ra-central.key    ${EtcRoot}/centreon-broker/ra-central.crt
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}
    ${found}    Ctn Remote Api Wait Poller    1    60
    Should Be True    ${found}    The poller engine does not announce remote actions over TLS.

    ${code}    ${elapsed}    ${answer}    Ctn Remote Api Send Commands    1
    ...    [TS] SCHEDULE_FORCED_SVC_CHECK;host_1;service_4;TS
    Should Be Equal As Integers    ${code}    200
    Should Be Equal As Strings    ${answer}[items][0][status]    APPLIED

BERA5
    [Documentation]    Given the central presents a certificate not signed by the CA of the poller, then the poller
    ...    does not connect and the web gets NOT_CONNECTED for it (remote_actions_insecure off).
    [Tags]    broker    engine    remote_actions    tls
    Ctn Config Remote Actions    ${True}    ${False}
    Ctn Create Key And Certificate    localhost    ${EtcRoot}/centreon-broker/ra-central.key
    ...    ${EtcRoot}/centreon-broker/ra-central.crt    ${True}
    Ctn Create Key And Certificate    localhost    ${EtcRoot}/centreon-broker/ra-rogue.key
    ...    ${EtcRoot}/centreon-broker/ra-rogue.crt    ${True}
    Ctn Config Remote Actions Tls    ${EtcRoot}/centreon-broker/ra-rogue.crt
    ...    ${EtcRoot}/centreon-broker/ra-rogue.key    ${EtcRoot}/centreon-broker/ra-central.crt
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}
    ${found}    Ctn Remote Api Wait Poller    1    30
    Should Not Be True    ${found}    The poller announced remote actions to a central with a rogue certificate.

    ${code}    ${elapsed}    ${answer}    Ctn Remote Api Send Commands    1
    ...    [TS] SCHEDULE_FORCED_SVC_CHECK;host_1;service_5;TS
    Should Be Equal As Integers    ${code}    200
    Should Be Equal As Strings    ${answer}[items][0][status]    NOT_CONNECTED


*** Keywords ***
Ctn Config Remote Actions
    [Arguments]    ${enabled}    ${insecure}=${True}
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Remote Api    central
    IF    ${enabled}
        Ctn Broker Config Add Item    module0    remote_actions    yes
    END
    IF    ${enabled} and ${insecure}
        # No TLS in these cases: PoC escape hatch.
        Ctn Broker Config Add Item    module0    remote_actions_insecure    yes
    END
    Ctn Clear Retention

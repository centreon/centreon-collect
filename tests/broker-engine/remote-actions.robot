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
    Ctn Create Self Signed Server Certificate    localhost    ${EtcRoot}/centreon-broker/ra-central.key
    ...    ${EtcRoot}/centreon-broker/ra-central.crt
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
    Ctn Create Self Signed Server Certificate    localhost    ${EtcRoot}/centreon-broker/ra-central.key
    ...    ${EtcRoot}/centreon-broker/ra-central.crt
    Ctn Create Self Signed Server Certificate    localhost    ${EtcRoot}/centreon-broker/ra-rogue.key
    ...    ${EtcRoot}/centreon-broker/ra-rogue.crt
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

BERA6
    [Documentation]    Given the central remote API, when the web asks GET /v1/ca, then it gets 404 CA_UNAVAILABLE
    ...    while the CA file is missing, then the CA PEM and the SHA-256 of its DER once the file exists.
    [Tags]    broker    remote_actions    tls
    Ctn Config Remote Actions    ${True}
    Remove File    ${EtcRoot}/centreon-broker/ra-ca.crt
    Ctn Broker Config Remote Api    central    ca_certificate=${EtcRoot}/centreon-broker/ra-ca.crt
    Ctn Start Broker
    FOR    ${i}    IN RANGE    30
        ${code}    ${answer}    Ctn Remote Api Request    GET    /v1/ca
        IF    ${code} != 0    BREAK
        Sleep    1s
    END
    Should Be Equal As Integers    ${code}    404
    Should Be Equal As Strings    ${answer}[status]    CA_UNAVAILABLE

    Ctn Create Self Signed Server Certificate    localhost    ${EtcRoot}/centreon-broker/ra-ca.key
    ...    ${EtcRoot}/centreon-broker/ra-ca.crt
    ${code}    ${answer}    Ctn Remote Api Request    GET    /v1/ca
    Should Be Equal As Integers    ${code}    200
    ${expected}    Ctn Certificate Fingerprint Sha256    ${EtcRoot}/centreon-broker/ra-ca.crt
    Should Be Equal As Strings    ${answer}[fingerprint_sha256]    ${expected}
    Should Start With    ${answer}[pem]    -----BEGIN CERTIFICATE-----
    Should Not Contain    ${answer}[pem]    PRIVATE KEY

BERA7
    [Documentation]    Given cbmod with an install command (CENTREON_PLUGINS_INSTALL), when a deployment commits a
    ...    new plugins.json, then the deployment is APPLIED and the install outcome reaches GET /v1/pollers and
    ...    GET /v1/deployments/{id}: one package installed, one up to date, one failed gives PARTIAL. The same
    ...    plugins.json again starts no install.
    [Tags]    broker    engine    remote_actions
    Ctn Config Remote Actions    ${True}
    ${stub}    Set Variable    ${VarRoot}/plugins-install-stub.sh
    Create File    ${stub}    \#!/bin/sh\necho "installed pkg-ok"\necho "uptodate pkg-up"\necho "failed pkg-ko unknown package"\n
    Run    chmod 755 ${stub}
    Set Environment Variable    CENTREON_PLUGINS_INSTALL    ${stub}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Remove Environment Variable    CENTREON_PLUGINS_INSTALL
    Ctn Wait For Engine To Be Ready    ${start}
    ${found}    Ctn Remote Api Wait Poller    1    60
    Should Be True    ${found}    The poller did not announce remote actions.

    ${since}    Get Current Date    result_format=epoch    exclude_millis=True
    ${plugins_json}    Set Variable    {"pkg-ok": "1", "pkg-up": "1", "pkg-ko": "1"}
    ${code}    ${id}    ${answer}    Ctn Remote Api Deploy    1    engine    plugins.json    ${plugins_json}
    Should Be Equal As Integers    ${code}    202
    ${plugins}    Ctn Remote Api Wait Plugins    1    ${since}    60
    Should Not Be Empty    ${plugins}    No plugin install outcome reported.
    Should Be Equal As Strings    ${plugins}[status]    PARTIAL
    Should Be Equal As Strings    ${plugins}[deployment_id]    ${id}
    Length Should Be    ${plugins}[packages]    3
    Should Be Equal As Strings    ${plugins}[packages][1][message]    already installed
    Should Be Equal As Strings    ${plugins}[packages][2][status]    failed

    ${code}    ${answer}    Ctn Remote Api Request    GET    /v1/deployments/${id}
    Should Be Equal As Integers    ${code}    200
    Should Be Equal As Strings    ${answer}[status]    APPLIED
    Should Be Equal As Strings    ${answer}[plugins][status]    PARTIAL

    ${code}    ${id2}    ${answer}    Ctn Remote Api Deploy    1    engine    plugins.json    ${plugins_json}
    Should Be Equal As Integers    ${code}    202
    ${content}    Create List    deployment '${id2}': 1 files installed
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    The second deployment was not installed.
    ${content}    Create List    deployment '${id2}': plugins.json changed
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    5
    Should Not Be True    ${result}    An unchanged plugins.json started an install.

BERA8
    [Documentation]    Given a poller with an exec allow-list, when the web posts executions to the central remote API,
    ...    then an allowed program returns its exit code, stdout and stderr tail; a large output is cut at the
    ...    requested cap; a program outside the allow-list is REJECTED; a long one is killed with TIMEOUT; and no
    ...    argument appears in the logs.
    [Tags]    broker    engine    remote_actions
    Ctn Config Remote Actions    ${True}
    Ctn Broker Config Remote Api    central    exec_insecure=${True}
    ${bin}    Set Variable    ${VarRoot}/exec-bin
    ${allow}    Set Variable    ${VarRoot}/exec-allow
    Create Directory    ${bin}
    Create Directory    ${allow}
    Create File    ${bin}/stub.sh
    ...    \#!/bin/sh\ncase "$1" in\nout) printf hello; echo oops >&2; exit 3 ;;\nbig) head -c 2097152 /dev/zero | tr '\\0' a ;;\nsleep) sleep 30 ;;\nesac\n
    Run    chmod 755 ${bin}/stub.sh
    Create File    ${allow}/default.json    {"exec": {"wildcard": ["${bin}/*"]}}
    Set Environment Variable    CENTREON_EXEC_ALLOWLIST_DIR    ${allow}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Remove Environment Variable    CENTREON_EXEC_ALLOWLIST_DIR
    Ctn Wait For Engine To Be Ready    ${start}
    ${found}    Ctn Remote Api Wait Poller    1    60
    Should Be True    ${found}    The poller did not announce remote actions.

    ${argv}    Create List    ${bin}/stub.sh    out    --password\=GLS_SECRET_MARKER
    ${code}    ${id}    ${answer}    Ctn Remote Api Exec    1    ${argv}
    Should Be Equal As Integers    ${code}    202
    ${e}    Ctn Remote Api Wait Exec    ${id}    30
    Should Be Equal As Strings    ${e}[status]    DONE
    Should Be Equal As Integers    ${e}[exit_code]    3
    Should Be Equal As Strings    ${e}[output]    hello
    Should Be Equal As Strings    ${e}[stderr_tail]    oops\n

    ${argv}    Create List    ${bin}/stub.sh    big
    ${code}    ${id}    ${answer}    Ctn Remote Api Exec    1    ${argv}    30    1048576
    ${e}    Ctn Remote Api Wait Exec    ${id}    60
    Should Be Equal As Strings    ${e}[status]    DONE
    Should Be True    ${e}[truncated]
    Should Be Equal As Integers    ${e}[output_bytes]    2097152
    ${len}    Get Length    ${e}[output]
    Should Be Equal As Integers    ${len}    1048576

    ${argv}    Create List    /bin/sh    -c    true
    ${code}    ${id}    ${answer}    Ctn Remote Api Exec    1    ${argv}
    ${e}    Ctn Remote Api Wait Exec    ${id}    30
    Should Be Equal As Strings    ${e}[status]    REJECTED
    Should Be Equal As Strings    ${e}[message]    program not in the allow-list

    ${argv}    Create List    ${bin}/stub.sh    sleep
    ${code}    ${id}    ${answer}    Ctn Remote Api Exec    1    ${argv}    2
    ${e}    Ctn Remote Api Wait Exec    ${id}    30
    Should Be Equal As Strings    ${e}[status]    TIMEOUT

    ${log}    Get File    ${engineLog0}
    Should Not Contain    ${log}    GLS_SECRET_MARKER
    ${log}    Get File    ${centralLog}
    Should Not Contain    ${log}    GLS_SECRET_MARKER


BERA9
    [Documentation]    Given cbmod with an install command (CENTREON_PLUGINS_INSTALL), when the web posts a plugin
    ...    install to the central remote API, then the requested packages are installed without any plugins.json
    ...    (names lowercased as check_plugins.py does);
    ...    forged package names and versions are REJECTED by the poller; a request over the cbd limits gets 400; a
    ...    long install is killed with TIMEOUT.
    [Tags]    broker    engine    remote_actions
    Ctn Config Remote Actions    ${True}
    Ctn Broker Config Remote Api    central    exec_insecure=${True}
    ${stub}    Set Variable    ${VarRoot}/plugin-install-stub.sh
    ${copy}    Set Variable    ${VarRoot}/plugin-install-request.json
    Remove File    ${copy}
    Create File    ${stub}
    ...    \#!/bin/sh\ncp "$1" ${copy}\nfor p in $(grep -o 'centreon-plugin-[a-z0-9.+-]*' "$1"); do\ncase "$p" in *slow*) sleep 30 ;; esac\necho "installed $p"\ndone\n
    Run    chmod 755 ${stub}
    Set Environment Variable    CENTREON_PLUGINS_INSTALL    ${stub}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Remove Environment Variable    CENTREON_PLUGINS_INSTALL
    Ctn Wait For Engine To Be Ready    ${start}
    ${found}    Ctn Remote Api Wait Poller    1    60
    Should Be True    ${found}    The poller did not announce remote actions.
    ${code}    ${answer}    Ctn Remote Api Request    GET    /v1/pollers
    Should Be True    ${answer}[pollers][0][plugin_install]

    ${code}    ${id}    ${answer}    Ctn Remote Api Install    1    {"centreon-plugin-B": "2025", "centreon-plugin-a": ""}
    Should Be Equal As Integers    ${code}    202
    ${i}    Ctn Remote Api Wait Install    ${id}    30
    Should Be Equal As Strings    ${i}[status]    INSTALLED
    Length Should Be    ${i}[packages]    2
    Should Be Equal As Strings    ${i}[packages][0][name]    centreon-plugin-a
    ${request}    Get File    ${copy}
    Should Be Equal As Strings    ${request}    {"centreon-plugin-a":"","centreon-plugin-b":"2025"}

    ${code}    ${id}    ${answer}    Ctn Remote Api Install    1    {"bash": ""}
    Should Be Equal As Integers    ${code}    202
    ${i}    Ctn Remote Api Wait Install    ${id}    30
    Should Be Equal As Strings    ${i}[status]    REJECTED
    Should Be Equal As Strings    ${i}[message]    invalid package name
    ${code}    ${id}    ${answer}    Ctn Remote Api Install    1    {"centreon-plugin-x": "1;id"}
    ${i}    Ctn Remote Api Wait Install    ${id}    30
    Should Be Equal As Strings    ${i}[status]    REJECTED
    Should Be Equal As Strings    ${i}[message]    invalid package version
    ${code}    ${id}    ${answer}    Ctn Remote Api Install    1    {"centreon-plugin-x": ""}    1801
    Should Be Equal As Integers    ${code}    400

    ${code}    ${id}    ${answer}    Ctn Remote Api Install    1    {"centreon-plugin-slow": ""}    2
    ${i}    Ctn Remote Api Wait Install    ${id}    30
    Should Be Equal As Strings    ${i}[status]    TIMEOUT
    Should Be Equal As Strings    ${i}[message]    killed after 2 s

    ${code}    ${answer}    Ctn Remote Api Request    GET    /v1/pollers
    Dictionary Should Not Contain Key    ${answer}[pollers][0]    plugins

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

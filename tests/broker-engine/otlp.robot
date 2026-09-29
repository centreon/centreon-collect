*** Settings ***
Documentation       End-to-end OTLP export to a Python gRPC collector. No SQL or external collector is needed.
...                 Run from tests/ after ./init-proto.sh. Requires Broker's OTLP module.

Resource            ../resources/import.resource
Library             ../resources/Otlp.py

Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Config OTLP Stack
Test Teardown       Ctn Stop OTLP Stack
Test Tags           broker    engine    otlp    macros


*** Test Cases ***
OTLP_DEFAULT_IDENTITY
    [Documentation]    Scenario: Without OTel macros, the service identity names Broker
    ...    Given host_1 defines neither _OTEL_SERVICE_NAME nor _OTEL_SERVICE_NAMESPACE
    ...    When a passive check result of service_1 is exported
    ...    Then its resource carries service.name centreon-broker and service.namespace centreon
    ...    And its resource carries a non-empty service.version
    ...    And centreon.service.description stays on the datapoint, not on the resource
    Ctn Start OTLP Stack
    Ctn Check OTLP Identity    centreon-broker    centreon

OTLP_HOST_MACROS
    [Documentation]    Scenario: Host macros override the service identity of each host independently
    ...    Given host_1 defines _OTEL_SERVICE_NAME, _OTEL_SERVICE_NAMESPACE and a _SECRET macro
    ...    And host_2 only defines _OTEL_SERVICE_NAMESPACE
    ...    And service_1 defines its own _OTEL_SERVICE_NAME
    ...    When passive check results of both hosts are exported
    ...    Then host_1 carries payments/shop without service.version
    ...    And host_2 carries centreon-broker/production
    ...    And no resource carries the secret macro
    ...    When the service_1 macro is changed at runtime
    ...    Then host_1 still carries payments/shop
    Ctn Set OTLP Host Macro    0    host_1    OTEL_SERVICE_NAME    payments
    Ctn Set OTLP Host Macro    0    host_1    OTEL_SERVICE_NAMESPACE    shop
    Ctn Set OTLP Host Macro    0    host_1    SECRET    do-not-export
    Ctn Set OTLP Host Macro    1    host_2    OTEL_SERVICE_NAMESPACE    production
    Ctn Engine Config Set Value In Services    0    service_1    _OTEL_SERVICE_NAME    wrong-service
    Ctn Start OTLP Stack
    Ctn Check OTLP Identity    payments    shop
    Ctn Check OTLP Identity    centreon-broker    production    1    host_2    service_2
    Ctn Change Custom Svc Var Command    host_1    service_1    OTEL_SERVICE_NAME    still-wrong
    Ctn Check OTLP Identity    payments    shop

OTLP_RUNTIME_MACROS
    [Documentation]    Scenario: Runtime host macro updates apply to the next exports
    ...    Given host_1 is configured with name and namespace "initial"
    ...    When both macros are changed at runtime with surrounding spaces
    ...    Then the next export carries the trimmed values payments/shop
    ...    When the name is changed to spaces only
    ...    Then service.name falls back to centreon-broker and the namespace is kept
    ...    When the namespace is changed to an empty value
    ...    Then the identity falls back to centreon-broker/centreon
    Ctn Set OTLP Host Macro    0    host_1    OTEL_SERVICE_NAME    initial
    Ctn Set OTLP Host Macro    0    host_1    OTEL_SERVICE_NAMESPACE    initial
    Ctn Start OTLP Stack
    Ctn Check OTLP Identity    initial    initial
    Ctn Change Custom Host Var Command    host_1    OTEL_SERVICE_NAME    ${SPACE}${SPACE}payments${SPACE}${SPACE}
    Ctn Change Custom Host Var Command    host_1    OTEL_SERVICE_NAMESPACE    ${SPACE}shop${SPACE}
    Ctn Check OTLP Identity    payments    shop
    Ctn Change Custom Host Var Command    host_1    OTEL_SERVICE_NAME    ${SPACE}${SPACE}
    Ctn Check OTLP Identity    centreon-broker    shop
    Ctn Change Custom Host Var Command    host_1    OTEL_SERVICE_NAMESPACE    ${EMPTY}
    Ctn Check OTLP Identity    centreon-broker    centreon

OTLP_RELOAD_AND_REMOVAL
    [Documentation]    Scenario: Configuration reloads update and then remove host macros
    ...    Given host_1 is configured with name "initial" and namespace "shop"
    ...    When the name is changed in the configuration and Engine is reloaded
    ...    Then the next export carries changed/shop
    ...    When the updated name is removed from the configuration and Engine is reloaded
    ...    Then the removal reaches Broker, since the update kept is_sent
    ...    And service.name falls back to centreon-broker
    ...    When the namespace is removed too and Engine is reloaded
    ...    Then the identity falls back to centreon-broker/centreon
    Ctn Set OTLP Host Macro    0    host_1    OTEL_SERVICE_NAME    initial
    Ctn Set OTLP Host Macro    0    host_1    OTEL_SERVICE_NAMESPACE    shop
    Ctn Start OTLP Stack
    Ctn Check OTLP Identity    initial    shop
    Ctn Set OTLP Host Macro    0    host_1    OTEL_SERVICE_NAME    changed
    Ctn Reload Engine And Wait    0
    Ctn Check OTLP Identity    changed    shop
    Ctn Engine Config Delete Value In Hosts    0    host_1    _OTEL_SERVICE_NAME${SPACE}
    Ctn Reload Engine And Wait    0
    Ctn Check OTLP Identity    centreon-broker    shop
    Ctn Engine Config Delete Value In Hosts    0    host_1    _OTEL_SERVICE_NAMESPACE${SPACE}
    Ctn Reload Engine And Wait    0
    Ctn Check OTLP Identity    centreon-broker    centreon

OTLP_BROKER_RESTART
    [Documentation]    Scenario: A runtime identity survives a Broker restart
    ...    Given host_1 is configured with name "configured"
    ...    When the name is changed to "runtime" at runtime
    ...    Then exports carry runtime/centreon
    ...    When Broker is restarted while Engine keeps running
    ...    Then exports still carry runtime/centreon
    Ctn Set OTLP Host Macro    0    host_1    OTEL_SERVICE_NAME    configured
    Ctn Start OTLP Stack
    Ctn Change Custom Host Var Command    host_1    OTEL_SERVICE_NAME    runtime
    Ctn Check OTLP Identity    runtime    centreon
    Ctn Restart Broker    only_central=${True}
    Ctn Check OTLP Identity    runtime    centreon

OTLP_ENGINE_RESTART_REMOVAL
    [Documentation]    Scenario: Macros removed while Engine is stopped do not survive its restart
    ...    Given host_1 is configured with name and namespace "removed"
    ...    And exports carry removed/removed
    ...    When Engine is stopped, both macros are removed from its configuration and Engine is started
    ...    Then its startup dump restores centreon-broker/centreon
    Ctn Set OTLP Host Macro    0    host_1    OTEL_SERVICE_NAME    removed
    Ctn Set OTLP Host Macro    0    host_1    OTEL_SERVICE_NAMESPACE    removed
    Ctn Start OTLP Stack
    Ctn Check OTLP Identity    removed    removed
    Ctn Stop Engine
    Ctn Engine Config Delete Value In Hosts    0    host_1    _OTEL_SERVICE_NAME${SPACE}
    Ctn Engine Config Delete Value In Hosts    0    host_1    _OTEL_SERVICE_NAMESPACE${SPACE}
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${2}
    Ctn Check OTLP Identity    centreon-broker    centreon

OTLP_POLLER_MIGRATION
    [Documentation]    Scenario: Delayed updates from the old poller cannot overwrite a moved host identity
    ...    Given host_1 is monitored by poller 0 with name and namespace "old"
    ...    When host_1 moves to poller 1 with name and namespace "new" and poller 1 is reloaded
    ...    Then exports through poller 1 carry new/new
    ...    When poller 0, not reloaded yet, sends "stale" runtime updates for host_1
    ...    Then exports through poller 0 still carry new/new
    ...    When poller 0 is reloaded
    ...    Then exports through poller 1 still carry new/new
    Ctn Set OTLP Host Macro    0    host_1    OTEL_SERVICE_NAME    old
    Ctn Set OTLP Host Macro    0    host_1    OTEL_SERVICE_NAMESPACE    old
    Ctn Start OTLP Stack
    Ctn Check OTLP Identity    old    old
    Ctn Engine Config Move Host To Engine    0    1    host_1
    Ctn Engine Config Move Services To Engine    0    1    host_1
    Ctn Set OTLP Host Macro    1    host_1    OTEL_SERVICE_NAME    new
    Ctn Set OTLP Host Macro    1    host_1    OTEL_SERVICE_NAMESPACE    new
    Ctn Reload Engine And Wait    1
    Ctn Check OTLP Identity    new    new    1
    # When poller 0, which keeps host_1 in memory until its reload, sends stale updates
    Ctn Change Custom Host Var Command    host_1    OTEL_SERVICE_NAME    stale
    Ctn Change Custom Host Var Command    host_1    OTEL_SERVICE_NAMESPACE    stale
    # Then a probe on the same old stream, queued after both updates, still sees new/new
    Ctn Check OTLP Identity    new    new    0
    Ctn Reload Engine And Wait    0
    Ctn Check OTLP Identity    new    new    1

OTLP_MIGRATION_WITHOUT_MACROS
    [Documentation]    Scenario: Moving a host to a poller without macros clears its old identity
    ...    Given host_1 is monitored by poller 0 with name and namespace "old"
    ...    When host_1 moves to poller 1 without OTel macros and poller 1 is reloaded
    ...    Then exports through poller 1 carry centreon-broker/centreon
    ...    When poller 0, not reloaded yet, sends a "stale" runtime name for host_1
    ...    Then exports through poller 0 still carry centreon-broker/centreon
    ...    When poller 0 is reloaded
    ...    Then exports through poller 1 still carry centreon-broker/centreon
    Ctn Set OTLP Host Macro    0    host_1    OTEL_SERVICE_NAME    old
    Ctn Set OTLP Host Macro    0    host_1    OTEL_SERVICE_NAMESPACE    old
    Ctn Start OTLP Stack
    Ctn Check OTLP Identity    old    old
    Ctn Engine Config Move Host To Engine    0    1    host_1
    Ctn Engine Config Move Services To Engine    0    1    host_1
    Ctn Engine Config Delete Value In Hosts    1    host_1    _OTEL_SERVICE_NAME${SPACE}
    Ctn Engine Config Delete Value In Hosts    1    host_1    _OTEL_SERVICE_NAMESPACE${SPACE}
    Ctn Reload Engine And Wait    1
    Ctn Check OTLP Identity    centreon-broker    centreon    1
    Ctn Change Custom Host Var Command    host_1    OTEL_SERVICE_NAME    stale
    Ctn Check OTLP Identity    centreon-broker    centreon    0
    Ctn Reload Engine And Wait    0
    Ctn Check OTLP Identity    centreon-broker    centreon    1

OTLP_STALE_CONFIGURATION_EVENTS
    [Documentation]    Scenario: Macro events are only accepted from the poller owning the host
    ...    Given a BBDO test peer replaces the pollers on the Broker input
    ...    When macros arrive before their host, or from a poller not owning it
    ...    Then they are ignored and the identity stays centreon-broker/centreon
    ...    When poller 10 owns the host and sends name and namespace "old"
    ...    Then exports carry old/old
    ...    When the host moves to poller 20
    ...    Then the identity falls back to centreon-broker/centreon
    ...    When poller 20 sends name and namespace "new"
    ...    And poller 10 sends stale updates and deletions of both macros
    ...    Then exports still carry new/new
    ...    When poller 20 removes both macros
    ...    Then the identity falls back to centreon-broker/centreon
    Ctn Start OTLP Event Stream
    Ctn Send OTLP Macro    CustomVariable    OTEL_SERVICE_NAME    before-host    10
    Ctn Send OTLP Host    0
    Ctn Send OTLP Macro    CustomVariableStatus    OTEL_SERVICE_NAME    unknown-owner    10
    Ctn Check BBDO OTLP Identity    centreon-broker    centreon
    Ctn Send OTLP Host    10
    Ctn Check BBDO OTLP Identity    centreon-broker    centreon
    Ctn Send OTLP Macro    CustomVariable    OTEL_SERVICE_NAME    old    10
    Ctn Send OTLP Macro    CustomVariable    OTEL_SERVICE_NAMESPACE    old    10
    Ctn Check BBDO OTLP Identity    old    old
    Ctn Send OTLP Host    20
    Ctn Check BBDO OTLP Identity    centreon-broker    centreon
    Ctn Send OTLP Macro    CustomVariable    OTEL_SERVICE_NAME    new    20
    Ctn Send OTLP Macro    CustomVariable    OTEL_SERVICE_NAMESPACE    new    20
    FOR    ${event}    IN    CustomVariable    CustomVariableStatus
        FOR    ${macro}    IN    OTEL_SERVICE_NAME    OTEL_SERVICE_NAMESPACE
            Ctn Send OTLP Macro    ${event}    ${macro}    stale    10
            Ctn Check BBDO OTLP Identity    new    new
            Ctn Send OTLP Macro    ${event}    ${macro}    ${EMPTY}    10    false
            Ctn Check BBDO OTLP Identity    new    new
        END
    END
    # When poller 20, the current owner, removes both macros
    Ctn Send OTLP Macro    CustomVariable    OTEL_SERVICE_NAME    ${EMPTY}    20    false
    Ctn Send OTLP Macro    CustomVariableStatus    OTEL_SERVICE_NAMESPACE    ${EMPTY}    20
    Ctn Check BBDO OTLP Identity    centreon-broker    centreon

OTLP_OLDER_PROTOBUF_SENDERS
    [Documentation]    Scenario: Macro events without instance_id from older senders are still accepted
    ...    Given a BBDO test peer whose transport source_id is not a poller ID
    ...    And the host moved from poller 10 to poller 20
    ...    When lowercase macro events without instance_id are received
    ...    Then exports carry legacy/compatibility
    Ctn Start OTLP Event Stream
    Ctn Send OTLP Host    10
    Ctn Send OTLP Host    20
    Ctn Send Otlp Bbdo Event    CustomVariable
    ...    {"host_id":101,"name":"otel_service_name","value":"legacy","enabled":true}
    Ctn Send Otlp Bbdo Event    CustomVariableStatus
    ...    {"host_id":101,"name":"otel_service_namespace","value":"compatibility"}
    Ctn Check BBDO OTLP Identity    legacy    compatibility

OTLP_CACHE_PERSISTENCE
    [Documentation]    Scenario: Broker restores both macros from its persisted cache
    ...    Given poller 10 owns the host
    ...    And it sends name "persisted" as configuration and namespace "runtime" as status
    ...    And exports carry persisted/runtime
    ...    When Broker is restarted and the test peer reconnects without replaying any event
    ...    Then exports still carry persisted/runtime
    Ctn Start OTLP Event Stream
    Ctn Send OTLP Host    10
    Ctn Send OTLP Macro    CustomVariable    OTEL_SERVICE_NAME    persisted    10
    Ctn Send OTLP Macro    CustomVariableStatus    OTEL_SERVICE_NAMESPACE    runtime    10
    Ctn Check BBDO OTLP Identity    persisted    runtime
    Ctn Disconnect Otlp Bbdo Peer
    Ctn Restart Broker    only_central=${True}
    Ctn Connect Otlp Bbdo Peer    127.0.0.1:5669
    Ctn Check BBDO OTLP Identity    persisted    runtime

OTLP_BBDO2_MACRO_CONVERSION
    [Documentation]    Scenario: Broker converts BBDO2 custom variable payloads
    ...    Given poller 10 owns the host
    ...    When BBDO2 configuration events set name "legacy" and namespace "initial"
    ...    Then exports carry legacy/initial
    ...    When a BBDO2 status event changes the namespace to "runtime"
    ...    Then exports carry legacy/runtime
    ...    When a disabled BBDO2 configuration event removes the name
    ...    Then exports carry centreon-broker/runtime
    Ctn Start OTLP Event Stream
    Ctn Send OTLP Host    10
    Ctn Send Otlp Legacy Macro    OTEL_SERVICE_NAME    legacy
    Ctn Send Otlp Legacy Macro    OTEL_SERVICE_NAMESPACE    initial
    Ctn Check BBDO OTLP Identity    legacy    initial
    Ctn Send Otlp Legacy Macro    OTEL_SERVICE_NAMESPACE    runtime    status=${True}
    Ctn Check BBDO OTLP Identity    legacy    runtime
    Ctn Send Otlp Legacy Macro    OTEL_SERVICE_NAME    ${EMPTY}    enabled=${False}
    Ctn Check BBDO OTLP Identity    centreon-broker    runtime

OTLP_HOST_AND_INSTANCE_CLEANUP
    [Documentation]    Scenario: Owner poller startup and host deletion clear the host identity
    ...    Given poller 10 owns the host and sends name "configured"
    ...    When poller 20 starts
    ...    Then exports still carry configured/centreon
    ...    When poller 10 starts
    ...    Then the identity falls back to centreon-broker/centreon
    ...    When poller 10 sends name "deleted"
    ...    Then exports carry deleted/centreon
    ...    When poller 10 deletes the host
    ...    And a status update arrives before the host is created again
    ...    Then exports carry centreon-broker/centreon
    Ctn Start OTLP Event Stream
    Ctn Send OTLP Host    10
    Ctn Send OTLP Macro    CustomVariable    OTEL_SERVICE_NAME    configured    10
    Ctn Send Otlp Bbdo Event    Instance    {"instance_id":20,"running":true}
    Ctn Check BBDO OTLP Identity    configured    centreon
    Ctn Send Otlp Bbdo Event    Instance    {"instance_id":10,"running":true}
    Ctn Check BBDO OTLP Identity    centreon-broker    centreon
    Ctn Send OTLP Macro    CustomVariable    OTEL_SERVICE_NAME    deleted    10
    Ctn Check BBDO OTLP Identity    deleted    centreon
    Ctn Send Otlp Bbdo Event    Host    {"host_id":101,"instance_id":10,"enabled":false}
    Ctn Send OTLP Macro    CustomVariableStatus    OTEL_SERVICE_NAME    after-deletion    10
    Ctn Send OTLP Host    10
    Ctn Check BBDO OTLP Identity    centreon-broker    centreon

OTLP_AGENT_HOST_METADATA
    [Documentation]    Scenario: CMA host information becomes host and os resource attributes
    ...    Given poller 10 owns the host and link-local filtering is left disabled
    ...    And exports carry no host.id
    ...    When an AgentHostInfo event is received for the host
    ...    Then exports carry typed host.* and os.* resource attributes
    ...    And host.ip keeps the IPv4 and IPv6 link-local addresses
    ...    When a later AgentHostInfo event is empty
    ...    Then all these attributes are removed
    Ctn Check Agent Host Metadata    ${False}

OTLP_EXCLUDE_LINK_LOCAL
    [Documentation]    Scenario: The opt-in filter removes link-local addresses from host.ip
    ...    Given host_ip_exclude_link_local is enabled on the OTLP output
    ...    When an AgentHostInfo event lists link-local and other IPv4 and IPv6 addresses
    ...    Then host.ip only keeps 192.0.2.10 and 2001:db8::1
    Ctn Check Agent Host Metadata    ${True}


*** Keywords ***
Ctn Config OTLP Stack
    [Documentation]    Given two passive pollers, host_1 on poller 0 and host_2 on poller 1
    ...    And a central Broker whose only output is the local OTLP collector, so neither SQL nor RRD is needed
    Ctn Stop Processes
    Ctn Clear Retention
    Ctn Clear Engine Logs
    Ctn Clear Broker Logs
    Ctn Config Engine    ${2}    ${2}    ${1}    check_active=${False}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${2}
    Ctn Config BBDO3    ${2}    only_engine=${True}
    Ctn Broker Config Add Item    central    bbdo_version    3.0.1
    # And every datapoint is captured in otlp.jsonl, saved with the Broker logs on failure
    ${collector}    Ctn Start Otlp Collector    ${BROKER_LOG}/otlp.jsonl
    ${output}    Create Dictionary    name=robot-otlp    type=otlp    endpoint=${collector}
    ...    max_datapoints_per_batch=1    max_send_interval=1
    ${outputs}    Create List    ${output}
    Ctn Broker Config Add Item    central    output    ${outputs}
    Ctn Broker Config Log    central    otl    trace
    Ctn Broker Config Log    central    core    debug
    FOR    ${poller}    IN RANGE    2
        Ctn Broker Config Output Set    module${poller}    central-module-master-output    retry_interval    1
        Ctn Engine Config Set Value    ${poller}    execute_host_checks    0    force=${True}
        Ctn Engine Config Set Value    ${poller}    execute_service_checks    0
        Ctn Engine Config Set Value    ${poller}    check_result_reaper_frequency    1
        Ctn Engine Config Set Value    ${poller}    retain_state_information    0    force=${True}
        # And Engine forwards every macro, secrets included, so the resource allowlist is exercised
        Ctn Engine Config Set Value    ${poller}    enable_macros_filter    0
        # And both pollers define the same command, so host_1 can move without a missing check_command
        Ctn Engine Config Add Command    ${poller}    otlp-passive    /bin/echo OK
        ${host}    Evaluate    ${poller} + 1
        Ctn Engine Config Set Host Value    ${poller}    host_${host}    check_command    otlp-passive
        Ctn Engine Config Replace Value In Services    ${poller}    service_${host}    check_command    otlp-passive
    END

Ctn Stop OTLP Stack
    TRY
        Ctn Disconnect Otlp Bbdo Peer
        Ctn Stop Engine Broker And Save Logs    only_central=${True}
    FINALLY
        Ctn Stop Otlp Collector
    END

Ctn Start OTLP Stack
    ${start}    Get Current Date
    Ctn Start Broker    only_central=${True}
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${2}

Ctn Reload Engine And Wait
    [Documentation]    When Engine is reloaded
    ...    Then "Reload configuration finished" is logged; check_for_external_commands() is logged
    ...    every second, so only this message proves the reload is applied
    [Arguments]    ${poller}
    ${start}    Get Current Date
    Ctn Reload Engine    ${poller}
    ${content}    Create List    Reload configuration finished
    ${result}    Ctn Find In Log With Timeout    ${ENGINE_LOG}/config${poller}/centengine.log    ${start}    ${content}    60
    Should Be True    ${result}    Engine ${poller} should finish its reload.

Ctn Set OTLP Host Macro
    [Arguments]    ${poller}    ${host}    ${name}    ${value}
    # The shared deletion helper matches substrings: include the separator so
    # OTEL_SERVICE_NAME cannot match OTEL_SERVICE_NAMESPACE.
    Ctn Engine Config Delete Value In Hosts    ${poller}    ${host}    _${name}${SPACE}
    Ctn Engine Config Set Value In Hosts    ${poller}    ${host}    _${name}    ${value}

Ctn Check OTLP Identity
    [Documentation]    When a passive result with a new probe value is sent through the poller
    ...    Then its datapoint carries the expected service identity and the check identity
    [Arguments]    ${name}    ${namespace}    ${poller}=0    ${host}=host_1    ${service}=service_1
    ${probe}    Ctn Otlp Next Probe
    Ctn Process Service Check Result    ${host}    ${service}    0
    ...    OTLP Robot probe | robot_probe=${probe}    config=config${poller}
    ${point}    Ctn Wait For Otlp Point    ${host}    centreon.robot_probe    ${probe}
    Ctn Assert OTLP Resource Identity    ${point}    ${name}    ${namespace}
    Dictionary Should Contain Item    ${point}[resource]    host.name    ${host}
    Dictionary Should Contain Item    ${point}[attributes]    centreon.service.description    ${service}
    Dictionary Should Not Contain Key    ${point}[resource]    centreon.service.description
    RETURN    ${point}

Ctn Assert OTLP Resource Identity
    [Arguments]    ${point}    ${name}    ${namespace}
    Dictionary Should Contain Item    ${point}[resource]    service.name    ${name}
    Dictionary Should Contain Item    ${point}[resource]    service.namespace    ${namespace}
    Dictionary Should Not Contain Key    ${point}[resource]    SECRET
    ${values}    Get Dictionary Values    ${point}[resource]
    Should Not Contain    ${values}    do-not-export
    IF    $name == 'centreon-broker'
        Dictionary Should Contain Key    ${point}[resource]    service.version
        Should Not Be Empty    ${point}[resource][service.version]
    ELSE
        Dictionary Should Not Contain Key    ${point}[resource]    service.version
    END

Ctn Start OTLP Event Stream
    [Documentation]    Given a BBDO 3.1 test peer replaces the pollers on Broker's input
    ...    And Engine is not started
    Ctn Config Broker Bbdo Input    central    bbdo_server    5669    grpc
    Ctn Broker Config Add Item    central    bbdo_version    3.1.0
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Start Broker    only_central=${True}
    Ctn Connect Otlp Bbdo Peer    127.0.0.1:5669

Ctn Send OTLP Host
    [Arguments]    ${poller}
    Ctn Send Otlp Bbdo Event    Host
    ...    {"host_id":101,"instance_id":${poller},"name":"robot-host","enabled":true}

Ctn Send OTLP Macro
    [Arguments]    ${event}    ${name}    ${value}    ${poller}    ${enabled}=true
    IF    $event == 'CustomVariable'
        Ctn Send Otlp Bbdo Event    ${event}
        ...    {"host_id":101,"instance_id":${poller},"name":"${name}","value":"${value}","enabled":${enabled}}
    ELSE
        Ctn Send Otlp Bbdo Event    ${event}
        ...    {"host_id":101,"instance_id":${poller},"name":"${name}","value":"${value}"}
    END

Ctn Check BBDO OTLP Identity
    [Arguments]    ${name}    ${namespace}
    ${point}    Ctn Otlp Bbdo Probe
    Ctn Assert OTLP Resource Identity    ${point}    ${name}    ${namespace}
    Dictionary Should Contain Item    ${point}[resource]    centreon.host.id    ${101}
    RETURN    ${point}

Ctn Check Agent Host Metadata
    [Arguments]    ${exclude_link_local}
    IF    ${exclude_link_local}
        Ctn Broker Config Output Set    central    robot-otlp    host_ip_exclude_link_local    true
    END
    Ctn Start OTLP Event Stream
    Ctn Send OTLP Host    10
    ${before}    Ctn Check BBDO OTLP Identity    centreon-broker    centreon
    Dictionary Should Not Contain Key    ${before}[resource]    host.id
    Ctn Send Otlp Bbdo Event    AgentHostInfo
    ...    {"host_id":101,"poller_id":10,"host_name":"robot-host","observed_at":100,"machine_id":"robot-machine","arch":"amd64","os_type":"linux","os_name":"Robot Linux","os_version":"1.0","ips":["192.0.2.10","169.254.1.2","fe80::1","2001:db8::1"]}
    ${point}    Ctn Check BBDO OTLP Identity    centreon-broker    centreon
    Dictionary Should Contain Item    ${point}[resource]    host.id    robot-machine
    Dictionary Should Contain Item    ${point}[resource]    host.arch    amd64
    Dictionary Should Contain Item    ${point}[resource]    os.type    linux
    Dictionary Should Contain Item    ${point}[resource]    os.name    Robot Linux
    Dictionary Should Contain Item    ${point}[resource]    os.version    1.0
    IF    ${exclude_link_local}
        ${ips}    Create List    192.0.2.10    2001:db8::1
    ELSE
        ${ips}    Create List    192.0.2.10    169.254.1.2    fe80::1    2001:db8::1
    END
    Lists Should Be Equal    ${point}[resource][host.ip]    ${ips}    ignore_order=${True}
    # When a later observation is empty, then no field of the previous one is kept
    Ctn Send Otlp Bbdo Event    AgentHostInfo
    ...    {"host_id":101,"poller_id":10,"host_name":"robot-host","observed_at":101}
    ${empty}    Ctn Check BBDO OTLP Identity    centreon-broker    centreon
    FOR    ${key}    IN    host.id    host.arch    host.ip    os.type    os.name    os.version
        Dictionary Should Not Contain Key    ${empty}[resource]    ${key}
    END

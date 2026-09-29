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

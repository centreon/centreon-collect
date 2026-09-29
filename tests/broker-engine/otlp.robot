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

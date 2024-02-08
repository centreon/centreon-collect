*** Settings ***
Documentation       Engine/Broker tests on opentelemetry engine server

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Stop Engine Broker And Save Logs


*** Test Cases ***
BEOTEL1
    [Documentation]    store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
    [Tags]    broker    engine    opentelemetry
    Config Engine    ${1}
    Add Otl ServerModule    0    {"server":{"host": "0.0.0.0","port": 4317}}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Broker Config Source Log    central    True
    Broker Config Add Lua Output    central    dump-otl-event    ${SCRIPTS}dump-otl-event.lua

    Config BBDO3    1
    Config Broker Sql Output    central    unified_sql
    Clear Retention

    Remove File    /tmp/lua.log

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the otel server start
    ${content}    Create List    unencrypted server listening on 0.0.0.0:4317
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4317" should be available.

    Sleep    1s

    ${all_attributes}    Create Empty List
    ${resources_list}    Create Empty List
    ${json_resources_list}    Create Empty List
    FOR    ${resource_index}    IN RANGE    2
        ${scope_metrics_list}    Create Empty List
        FOR    ${scope_metric_index}    IN RANGE    2
            ${metrics_list}    Create Empty List
            FOR    ${metric_index}    IN RANGE    2
                ${metric_attrib}    Create Random Dictionary    5
                Append To List    ${all_attributes}    ${metric_attrib}
                ${metric}    Create Otl Metric    metric_${metric_index}    5    ${metric_attrib}
                Append To List    ${metrics_list}    ${metric}
            END
            ${scope_attrib}    Create Random Dictionary    5
            Append To List    ${all_attributes}    ${scope_attrib}
            ${scope_metric}    Create Otl Scope Metrics    ${scope_attrib}    ${metrics_list}
            Append To List    ${scope_metrics_list}    ${scope_metric}
        END
        ${resource_attrib}    Create Random Dictionary    5
        Append To List    ${all_attributes}    ${resource_attrib}
        ${resource_metrics}    Create Otl Resource Metrics    ${resource_attrib}    ${scope_metrics_list}
        Append To List    ${resources_list}    ${resource_metrics}
        ${json_resource_metrics}    Protobuf To Json    ${resource_metrics}
        Append To List    ${json_resources_list}    ${json_resource_metrics}
    END

    Log To Console    export metrics
#    ${request_content}    Protobuf To Json    ${}
    Send Otl To Engine    4317    ${resources_list}

    Sleep    5

    ${event}    Extract Event From Lua Log    /tmp/lua.log    resource_metrics

    ${test_ret}    Is List In Other List    ${event}    ${json_resources_list}

    Should Be True    ${test_ret}    protobuf object sent to engine mus be in lua.log

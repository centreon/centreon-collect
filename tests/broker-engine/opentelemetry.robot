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
    [Tags]    broker    engine    opentelemetry    mon-34074
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

    ${all_attributes}    Create List
    ${resources_list}    Create List
    ${json_resources_list}    Create List
    FOR    ${resource_index}    IN RANGE    2
        ${scope_metrics_list}    Create List
        FOR    ${scope_metric_index}    IN RANGE    2
            ${metrics_list}    Create List
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
    Send Otl To Engine    4317    ${resources_list}

    Sleep    5

    ${event}    Extract Event From Lua Log    /tmp/lua.log    resource_metrics

    ${test_ret}    Is List In Other List    ${event}    ${json_resources_list}

    Should Be True    ${test_ret}    protobuf object sent to engine mus be in lua.log

BEOTEL_TELEGRAF_CHECK_HOST
    [Documentation]    we send nagios telegraf formated datas and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    mon-34004
    Config Engine    ${1}
    Add Otl ServerModule    0    {"server":{"host": "0.0.0.0","port": 4317}}
    Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    open_telemetry attributes --host_attribute=data_point --host_key=host --service_attribute=data_point --service_key=service
    Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp
    ...    nagios_telegraf
    ...    OTEL connector

    Config Broker    central
    Config Broker    module
    Config Broker    rrd

    Config BBDO3    1
    Config Broker Sql Output    central    unified_sql
    Clear Retention

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the otel server start
    ${content}    Create List    unencrypted server listening on 0.0.0.0:4317
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4317" should be available.
    Sleep    1

    ${state_attrib}    Create Dictionary    host=host_1
    ${rta_attrib}    Create Dictionary    host=host_1    perfdata=rta    unit=ms
    ${rtmax_attrib}    Create Dictionary    host=host_1    perfdata=rtmax    unit=ms
    ${pl_attrib}    Create Dictionary    host=host_1    perfdata=pl    unit=%

    # ok state
    ${state_metric}    Create Otl Metric    check_icmp_state    1    ${state_attrib}    ${0}
    # value
    ${value_metric}    Create Otl Metric    check_icmp_value    1    ${rta_attrib}    ${0.022}
    Add Data Point To Metric    ${value_metric}    ${rtmax_attrib}    ${0.071}
    Add Data Point To Metric    ${value_metric}    ${pl_attrib}    ${0.001}

    ${critical_gt_metric}    Create Otl Metric    check_icmp_critical_gt    1    ${rta_attrib}    ${500}
    Add Data Point To Metric    ${critical_gt_metric}    ${pl_attrib}    ${80}
    ${critical_lt_metric}    Create Otl Metric    check_icmp_critical_lt    1    ${rta_attrib}    ${1}
    Add Data Point To Metric    ${critical_gt_metric}    ${pl_attrib}    ${0.00001}

    ${metrics_list}    Create List
    ...    ${state_metric}
    ...    ${value_metric}
    ...    ${critical_gt_metric}
    ...    ${critical_lt_metric}

    ${scope_attrib}    Create Dictionary
    ${scope_metric}    Create Otl Scope Metrics    ${scope_attrib}    ${metrics_list}

    ${scope_metrics_list}    Create List    ${scope_metric}

    ${resource_attrib}    Create Dictionary
    ${resource_metrics}    Create Otl Resource Metrics    ${resource_attrib}    ${scope_metrics_list}
    ${resources_list}    Create List    ${resource_metrics}

    Log To Console    export metrics
    Send Otl To Engine    4317    ${resources_list}

    Sleep    5

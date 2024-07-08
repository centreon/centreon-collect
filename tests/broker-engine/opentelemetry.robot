*** Settings ***
Documentation       Engine/Broker tests on opentelemetry engine server

Resource            ../resources/import.resource
Library             ../resources/Agent.py

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
# to enable when engine will be able to send unknown metrics to victoria metrics
# BEOTEL1
#    [Documentation]    store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
#    [Tags]    broker    engine    opentelemetry    mon-34074
#    Ctn Config Engine    ${1}
#    Add Otl ServerModule    0    {"server":{"host": "0.0.0.0","port": 4317}}
#    Config Broker    central
#    Config Broker    module
#    Config Broker    rrd
#    Broker Config Source Log    central    True
#    Broker Config Add Lua Output    central    dump-otl-event    ${SCRIPTS}dump-otl-event.lua

#    Ctn ConfigBBDO3    1
#    Config Broker Sql Output    central    unified_sql
#    Ctn Clear Retention

#    Remove File    /tmp/lua.log

#    ${start}    Get Current Date
#    Ctn Start Broker
#    Ctn Start Engine

#    # Let's wait for the otel server start
#    ${content}    Create List    unencrypted server listening on 0.0.0.0:4317
#    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
#    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4317" should be available.

#    Sleep    1s

#    ${all_attributes}    Create List
#    ${resources_list}    Create List
#    ${json_resources_list}    Create List
#    FOR    ${resource_index}    IN RANGE    2
#    ${scope_metrics_list}    Create List
#    FOR    ${scope_metric_index}    IN RANGE    2
#    ${metrics_list}    Create List
#    FOR    ${metric_index}    IN RANGE    2
#    ${metric_attrib}    Create Random Dictionary    5
#    Append To List    ${all_attributes}    ${metric_attrib}
#    ${metric}    Create Otl Metric    metric_${metric_index}    5    ${metric_attrib}
#    Append To List    ${metrics_list}    ${metric}
#    END
#    ${scope_attrib}    Create Random Dictionary    5
#    Append To List    ${all_attributes}    ${scope_attrib}
#    ${scope_metric}    Create Otl Scope Metrics    ${scope_attrib}    ${metrics_list}
#    Append To List    ${scope_metrics_list}    ${scope_metric}
#    END
#    ${resource_attrib}    Create Random Dictionary    5
#    Append To List    ${all_attributes}    ${resource_attrib}
#    ${resource_metrics}    Create Otl Resource Metrics    ${resource_attrib}    ${scope_metrics_list}
#    Append To List    ${resources_list}    ${resource_metrics}
#    ${json_resource_metrics}    Protobuf To Json    ${resource_metrics}
#    Append To List    ${json_resources_list}    ${json_resource_metrics}
#    END

#    Log To Console    export metrics
#    Send Otl To Engine    4317    ${resources_list}

#    Sleep    5

#    ${event}    Extract Event From Lua Log    /tmp/lua.log    resource_metrics

#    ${test_ret}    Is List In Other List    ${event}    ${json_resources_list}

#    Should Be True    ${test_ret}    protobuf object sent to engine mus be in lua.log

BEOTEL_TELEGRAF_CHECK_HOST
    [Documentation]    we send nagios telegraf formated datas and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    mon-34004
    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=nagios_telegraf --extractor=attributes --host_path=resource_metrics.scope_metrics.data.data_points.attributes.host --service_path=resource_metrics.scope_metrics.data.data_points.attributes.service
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.1
    ...    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    sql    trace

    Ctn ConfigBBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the otel server start
    ${content}    Create List    unencrypted server listening on 0.0.0.0:4317
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4317" should be available.
    Sleep    1

    ${resources_list}    Ctn Create Otl Request    ${0}    host_1

    # check without feed
    ${start}    Ctn Get Round Current Date
    Ctn Schedule Forced Host Check    host_1
    ${result}    Ctn Check Host Output Resource Status With Timeout
    ...    host_1
    ...    35
    ...    ${start}
    ...    0
    ...    HARD
    ...    (No output returned from host check)
    Should Be True    ${result}    hosts table not updated


    Log To Console    export metrics
    Ctn Send Otl To Engine    4317    ${resources_list}

    Sleep    5


    # feed and check
    ${start}    Ctn Get Round Current Date
    Ctn Schedule Forced Host Check    host_1

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    30    ${start}    0  HARD  OK
    Should Be True    ${result}    hosts table not updated


    Log To Console    export metrics
    Ctn Send Otl To Engine    4317    ${resources_list}

    Sleep    5


    # feed and check
    ${start}    Ctn Get Round Current Date
    Ctn Schedule Forced Host Check    host_1

    ${result}    Ctn Check Host Check Status With Timeout    host_1    30    ${start}    0    OK
    Should Be True    ${result}    hosts table not updated

    # check then feed, three times to modify hard state
    ${start}    Ctn Get Round Current Date
    Ctn Schedule Forced Host Check    host_1
    Sleep    2
    ${resources_list}    Ctn Create Otl Request    ${2}    host_1
    Ctn Send Otl To Engine    4317    ${resources_list}
    Ctn Schedule Forced Host Check    host_1
    Sleep    2
    ${resources_list}    Ctn Create Otl Request    ${2}    host_1
    Ctn Send Otl To Engine    4317    ${resources_list}
    Ctn Schedule Forced Host Check    host_1
    Sleep    2
    ${resources_list}    Ctn Create Otl Request    ${2}    host_1
    Ctn Send Otl To Engine    4317    ${resources_list}
    ${result}    Ctn Check Host Check Status With Timeout    host_1    30    ${start}    1    CRITICAL

    Should Be True    ${result}    hosts table not updated

BEOTEL_TELEGRAF_CHECK_SERVICE
    [Documentation]    we send nagios telegraf formated datas and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    mon-34004
    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule    0    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=nagios_telegraf --extractor=attributes --host_path=resource_metrics.scope_metrics.data.data_points.attributes.host --service_path=resource_metrics.scope_metrics.data.data_points.attributes.service
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check_icmp
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.1
    ...    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd

    Ctn ConfigBBDO3    1
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the otel server start
    ${content}    Create List    unencrypted server listening on 0.0.0.0:4317
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4317" should be available.
    Sleep    1

    ${resources_list}    Ctn Create Otl Request    ${0}    host_1    service_1

    # check without feed

    ${start}    Ctn Get Round Current Date
    Ctn Schedule Forced Svc Check    host_1    service_1
    ${result}    Ctn Check Service Output Resource Status With Timeout
    ...    host_1
    ...    service_1
    ...    35
    ...    ${start}
    ...    0
    ...    HARD
    ...    (No output returned from plugin)
    Should Be True    ${result}    services table not updated

    Log To Console    export metrics
    Ctn Send Otl To Engine    4317    ${resources_list}

    Sleep    5

    # feed and check
    ${start}    Ctn Get Round Current Date
    Ctn Schedule Forced Svc Check    host_1    service_1

    ${result}    Ctn Check Service Output Resource Status With Timeout    host_1    service_1    30    ${start}    0  HARD   OK
    Should Be True    ${result}    services table not updated

    Log To Console    export metrics
    Ctn Send Otl To Engine    4317    ${resources_list}

    Sleep    5

    # feed and check
    ${start}    Ctn Get Round Current Date
    Ctn Schedule Forced Svc Check    host_1    service_1

    ${result}    Ctn Check Service Check Status With Timeout    host_1    service_1    30    ${start}    0    OK
    Should Be True    ${result}    services table not updated

    # check then feed, three times to modify hard state
    ${start}    Ctn Get Round Current Date
    ${resources_list}    Ctn Create Otl Request    ${2}    host_1    service_1
    Ctn Send Otl To Engine    4317    ${resources_list}
    Sleep    2
    Ctn Schedule Forced Svc Check    host_1    service_1
    ${resources_list}    Ctn Create Otl Request    ${2}    host_1    service_1
    Ctn Send Otl To Engine    4317    ${resources_list}
    Sleep    2
    Ctn Schedule Forced Svc Check    host_1    service_1
    ${resources_list}    Ctn Create Otl Request    ${2}    host_1    service_1
    Ctn Send Otl To Engine    4317    ${resources_list}
    Sleep    2
    Ctn Schedule Forced Svc Check    host_1    service_1
    ${result}    Ctn Check Service Output Resource Status With Timeout    host_1    service_1    30    ${start}    2  HARD  CRITICAL

    Should Be True    ${result}    services table not updated

BEOTEL_SERVE_TELEGRAF_CONFIGURATION_CRYPTED
    [Documentation]    we configure engine with a telegraf conf server and we check telegraf conf file
    [Tags]    broker    engine    opentelemetry    mon-35539
    Ctn Create Key And Certificate    localhost    /tmp/otel/server.key    /tmp/otel/server.crt
    Ctn Config Engine    ${1}    ${3}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0, "telegraf_conf_server": {"http_server":{"port": 1443, "encryption": true, "certificate_path": "/tmp/otel/server.crt", "key_path": "/tmp/otel/server.key"}, "cehck_interval":60, "engine_otel_endpoint": "127.0.0.1:4317"}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=nagios_telegraf --extractor=attributes --host_path=resource_metrics.scope_metrics.data.data_points.attributes.host --service_path=resource_metrics.scope_metrics.data.data_points.attributes.service
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check_icmp_serv_1
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp_serv_1
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.1
    ...    OTEL connector

    Ctn Engine Config Replace Value In Services    ${0}    service_2    check_command    otel_check_icmp_serv_2
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp_serv_2
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.2
    ...    OTEL connector

    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp_host_1
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp_host_1
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.10
    ...    OTEL connector

    Ctn Engine Config Replace Value In Hosts    ${0}    host_2    check_command    otel_check_icmp_host_2
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp_host_2
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.20
    ...    OTEL connector

    Ctn Engine Config Replace Value In Services    ${0}    service_5    check_command    otel_check_icmp_serv_5
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp_serv_5
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.5
    ...    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    module

    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Engine

    # Let's wait for the otel server start
    ${content}    Create List    server listen on 0.0.0.0:1443
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "server listen on 0.0.0.0:1443" should be available.
    Sleep    1
    ${telegraf_conf_response}    GET
    ...    verify=${False}
    ...    url=https://localhost:1443/engine?host=host_1

    Should Be Equal As Strings    ${telegraf_conf_response.reason}    OK    no response received or error response
    ${content_compare_result}    Ctn Compare String With File
    ...    ${telegraf_conf_response.text}
    ...    resources/opentelemetry/telegraf.conf

    Should Be True
    ...    ${content_compare_result}
    ...    unexpected telegraf server response: ${telegraf_conf_response.text}

BEOTEL_SERVE_TELEGRAF_CONFIGURATION_NO_CRYPTED
    [Documentation]    we configure engine with a telegraf conf server and we check telegraf conf file
    [Tags]    broker    engine    opentelemetry    mon-35539
    Ctn Create Key And Certificate    localhost    /tmp/otel/server.key    /tmp/otel/server.crt
    Ctn Config Engine    ${1}    ${3}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0, "telegraf_conf_server": {"http_server": {"port": 1443, "encryption": false}, "engine_otel_endpoint": "127.0.0.1:4317"}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=nagios_telegraf --extractor=attributes --host_path=resource_metrics.scope_metrics.data.data_points.attributes.host --service_path=resource_metrics.scope_metrics.data.data_points.attributes.service
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check_icmp_serv_1
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp_serv_1
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.1
    ...    OTEL connector

    Ctn Engine Config Replace Value In Services    ${0}    service_2    check_command    otel_check_icmp_serv_2
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp_serv_2
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.2
    ...    OTEL connector

    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp_host_1
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp_host_1
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.10
    ...    OTEL connector

    Ctn Engine Config Replace Value In Hosts    ${0}    host_2    check_command    otel_check_icmp_host_2
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp_host_2
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.20
    ...    OTEL connector

    Ctn Engine Config Replace Value In Services    ${0}    service_5    check_command    otel_check_icmp_serv_5
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp_serv_5
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.5
    ...    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    module

    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Engine

    # Let's wait for the otel server start
    ${content}    Create List    server listen on 0.0.0.0:1443
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "server listen on 0.0.0.0:1443" should be available.
    Sleep    1
    ${telegraf_conf_response}    GET
    ...    url=http://localhost:1443/engine?host=host_1

    Should Be Equal As Strings    ${telegraf_conf_response.reason}    OK    no response received or error response

    Should Be Equal As Strings    ${telegraf_conf_response.reason}    OK    no response received or error response
    ${content_compare_result}    Ctn Compare String With File
    ...    ${telegraf_conf_response.text}
    ...    resources/opentelemetry/telegraf.conf

    Should Be True
    ...    ${content_compare_result}
    ...    unexpected telegraf server response: ${telegraf_conf_response.text}


BEOTEL_CENTREON_AGENT_CHECK_HOST
    [Documentation]    agent check host and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-63843
    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0, "centreon_agent":{"check_interval":10, "export_period":10}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp
    ...    /bin/echo "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    ...    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent
    Ctn Broker Config Log    central    sql    trace

    Ctn ConfigBBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    ${content}    Create List    unencrypted server listening on 0.0.0.0:4317
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4317" should be available.
    Sleep    1

    ${start}    Ctn Get Round Current Date
    Ctn Schedule Forced Host Check    host_1

    ${result}    Ctn Check Host Check Status With Timeout    host_1    30    ${start}    0    OK - 127.0.0.1
    Should Be True    ${result}    hosts table not updated

    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp_2
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp_2
    ...    /bin/echo "OK check2 - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    ...    OTEL connector

    #update conf engine, it must be taken into account by agent
    Log To Console    modify engine conf and reload engine
    Ctn Reload Engine

    #wait for new data from agent
    ${start}    Ctn Get Round Current Date
    ${content}    Create List    description: \"OK check2
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    22
    Should Be True    ${result}    "description: "OK check2" should be available.

    ${start}    Ctn Get Round Current Date
    Ctn Schedule Forced Host Check    host_1

    ${result}    Ctn Check Host Check Status With Timeout    host_1    30    ${start}    0    OK check2 - 127.0.0.1: rta 0,010ms, lost 0%
    Should Be True    ${result}    hosts table not updated


BEOTEL_CENTREON_AGENT_CHECK_SERVICE
    [Documentation]    agent check service and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-63843
    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"check_interval":10, "export_period":15}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check
    ...    /tmp/var/lib/centreon-engine/check.pl --id 456
    ...    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    #service_1 check fail CRITICAL
    Ctn Set Command Status    456    ${2}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent
    Ctn Broker Config Log    central    sql    trace

    Ctn ConfigBBDO3    1
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    ${content}    Create List    unencrypted server listening on 0.0.0.0:4317
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4317" should be available.
    
    ${content}    Create List    fifos:{"host_1,service_1"
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    fifos not found in logs
    
    Ctn Schedule Forced Svc Check    host_1    service_1

    ${result}    Ctn Check Service Check Status With Timeout    host_1  service_1  60  ${start}  2  Test check 456
    Should Be True    ${result}    services table not updated

    ${start}    Ctn Get Round Current Date
    #service_1 check ok
    Ctn Set Command Status    456    ${0}

    ${content}    Create List    as_int: 0
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    status 0 not found in logs
    
    Ctn Schedule Forced Svc Check    host_1    service_1

    ${result}    Ctn Check Service Check Status With Timeout    host_1  service_1  60  ${start}  0  Test check 456
    Should Be True    ${result}    services table not updated


BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST
    [Documentation]    agent check host with reversed connection and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-63843
    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"max_length_grpc_log":0,"centreon_agent":{"check_interval":10, "export_period":15, "reverse_connections":[{"host": "127.0.0.1","port": 4317}]}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp
    ...    /bin/echo "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    ...    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Reverse Centreon Agent
    Ctn Broker Config Log    central    sql    trace

    Ctn ConfigBBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for engine to connect to agent
    ${content}    Create List    init from [.\\s]*127.0.0.1:4317
    ${result}    Ctn Find Regex In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "init from localhost:4317" not found in log
    Sleep    1

    ${start}    Ctn Get Round Current Date
    Ctn Schedule Forced Host Check    host_1

    ${result}    Ctn Check Host Check Status With Timeout    host_1    30    ${start}    0    OK - 127.0.0.1
    Should Be True    ${result}    hosts table not updated

    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp_2
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp_2
    ...    /bin/echo "OK check2 - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    ...    OTEL connector

    #update conf engine, it must be taken into account by agent
    Log To Console    modify engine conf and reload engine
    Ctn Reload Engine

    #wait for new data from agent
    ${start}    Ctn Get Round Current Date
    ${content}    Create List    description: \"OK check2
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    "description: "OK check2" should be available.

    ${start}    Ctn Get Round Current Date
    Ctn Schedule Forced Host Check    host_1

    ${result}    Ctn Check Host Check Status With Timeout    host_1    30    ${start}    0    OK check2 - 127.0.0.1: rta 0,010ms, lost 0%
    Should Be True    ${result}    hosts table not updated


BEOTEL_REVERSE_CENTREON_AGENT_CHECK_SERVICE
    [Documentation]    agent check service with reversed connection and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-63843
    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"max_length_grpc_log":0,"centreon_agent":{"check_interval":10, "export_period":15, "reverse_connections":[{"host": "127.0.0.1","port": 4317}]}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check
    ...    /tmp/var/lib/centreon-engine/check.pl --id 456
    ...    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    #service_1 check fail CRITICAL
    Ctn Set Command Status    456    ${2}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Reverse Centreon Agent
    Ctn Broker Config Log    central    sql    trace

    Ctn ConfigBBDO3    1
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for engine to connect to agent
    ${content}    Create List    init from [.\\s]*127.0.0.1:4317
    ${result}    Ctn Find Regex In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "init from 127.0.0.1:4317" not found in log

    
    ${content}    Create List    fifos:{"host_1,service_1"
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    fifos not found in logs
    
    Ctn Schedule Forced Svc Check    host_1    service_1

    ${result}    Ctn Check Service Check Status With Timeout    host_1  service_1  60  ${start}  2  Test check 456
    Should Be True    ${result}    services table not updated

    ${start}    Ctn Get Round Current Date
    #service_1 check ok
    Ctn Set Command Status    456    ${0}

    ${content}    Create List    as_int: 0
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    status 0 not found in logs
    
    Ctn Schedule Forced Svc Check    host_1    service_1

    ${result}    Ctn Check Service Check Status With Timeout    host_1  service_1  60  ${start}  0  Test check 456
    Should Be True    ${result}    services table not updated

BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED
    [Documentation]    agent check host with encrypted connection and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-63843
    Ctn Config Engine    ${1}    ${2}    ${2}
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317, "encryption": true, "public_cert": "/tmp/server_1234.crt", "private_key": "/tmp/server_1234.key", "ca_certificate": "/tmp/ca_1234.crt"},"max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp
    ...    /bin/echo "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    ...    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent  ${None}  ${None}  /tmp/ca_1234.crt
    Ctn Broker Config Log    central    sql    trace

    Ctn ConfigBBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    ${content}    Create List    encrypted server listening on 0.0.0.0:4317
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4317" should be available.
    Sleep    1

    ${start}    Ctn Get Round Current Date
    Ctn Schedule Forced Host Check    host_1

    ${result}    Ctn Check Host Check Status With Timeout    host_1    30    ${start}    0    OK - 127.0.0.1
    Should Be True    ${result}    hosts table not updated



BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST_CRYPTED
    [Documentation]    agent check host with encrypted reversed connection and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-63843
    Ctn Config Engine    ${1}    ${2}    ${2}
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/

    Ctn Add Otl ServerModule
    ...    0
    ...    {"max_length_grpc_log":0,"centreon_agent":{"check_interval":10, "export_period":15, "reverse_connections":[{"host": "localhost","port": 4317, "encryption": true, "ca_certificate": "/tmp/ca_1234.crt"}]}}

    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp
    ...    /bin/echo "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    ...    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Reverse Centreon Agent  /tmp/server_1234.key  /tmp/server_1234.crt  /tmp/ca_1234.crt
    Ctn Broker Config Log    central    sql    trace

    Ctn ConfigBBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for engine to connect to agent
    ${content}    Create List    init from localhost:4317
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "init from localhost:4317" not found in log
    Sleep    1

    ${start}    Ctn Get Round Current Date
    Ctn Schedule Forced Host Check    host_1

    ${result}    Ctn Check Host Check Status With Timeout    host_1    30    ${start}    0    OK - 127.0.0.1
    Should Be True    ${result}    hosts table not updated




*** Keywords ***
Ctn Create Otl Request
    [Documentation]    create an otl request with nagios telegraf style
    [Arguments]    ${state}    ${host}    ${service}=
    ${state_attrib}    Create Dictionary    host=${host}    service=${service}
    ${rta_attrib}    Create Dictionary    host=${host}    service=${service}    perfdata=rta    unit=ms
    ${rtmax_attrib}    Create Dictionary    host=${host}    service=${service}    perfdata=rtmax    unit=ms
    ${pl_attrib}    Create Dictionary    host=${host}    service=${service}    perfdata=pl    unit=%

    # state
    ${state_metric}    Ctn Create Otl Metric    check_icmp_state    1    ${state_attrib}    ${state}
    # value
    ${value_metric}    Ctn Create Otl Metric    check_icmp_value    1    ${rta_attrib}    ${0.022}
    Ctn Add Data Point To Metric    ${value_metric}    ${rtmax_attrib}    ${0.071}
    Ctn Add Data Point To Metric    ${value_metric}    ${pl_attrib}    ${0.001}

    ${critical_gt_metric}    Ctn Create Otl Metric    check_icmp_critical_gt    1    ${rta_attrib}    ${500}
    Ctn Add Data Point To Metric    ${critical_gt_metric}    ${pl_attrib}    ${80}
    ${critical_lt_metric}    Ctn Create Otl Metric    check_icmp_critical_lt    1    ${rta_attrib}    ${1}
    Ctn Add Data Point To Metric    ${critical_gt_metric}    ${pl_attrib}    ${0.00001}

    ${metrics_list}    Create List
    ...    ${state_metric}
    ...    ${value_metric}
    ...    ${critical_gt_metric}
    ...    ${critical_lt_metric}

    ${scope_attrib}    Create Dictionary
    ${scope_metric}    Ctn Create Otl Scope Metrics    ${scope_attrib}    ${metrics_list}

    ${scope_metrics_list}    Create List    ${scope_metric}

    ${resource_attrib}    Create Dictionary
    ${resource_metrics}    Ctn Create Otl Resource Metrics    ${resource_attrib}    ${scope_metrics_list}
    ${resources_list}    Create List    ${resource_metrics}

    RETURN    ${resources_list}

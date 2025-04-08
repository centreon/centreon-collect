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

#    Ctn Config BBDO3    1
#    Config Broker Sql Output    central    unified_sql
#    Ctn Clear Retention

#    Remove File    /tmp/lua.log

#    ${start}    Get Current Date
#    Ctn Start Broker
#    Ctn Start Engine

#    # Let's wait for the otel server start
#    Ctn Wait For Otel Server To Be Ready    ${start}

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
    [Documentation]    we send nagios telegraf formatted datas and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-34004
    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=nagios_telegraf --extractor=attributes --host_path=resource_metrics.scope_metrics.data.data_points.attributes.host --service_path=resource_metrics.scope_metrics.data.data_points.attributes.service
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1 
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

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the otel server start
    Ctn Wait For Otel Server To Be Ready    ${start}
    Sleep    1


    Log To Console    export metrics
    # feed and check
    ${start}    Ctn Get Round Current Date
    ${resources_list}    Ctn Create Otl Request    ${0}    host_1
    Ctn Send Otl To Engine    4317    ${resources_list}

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    30    ${start}    0  HARD  OK
    Should Be True    ${result}    hosts table not updated


    Log To Console    export metrics
    Ctn Send Otl To Engine    4317    ${resources_list}

    Sleep    5

    # check then feed, three times to modify hard state
    ${start}    Ctn Get Round Current Date
    Sleep    2
    ${resources_list}    Ctn Create Otl Request    ${2}    host_1
    Ctn Send Otl To Engine    4317    ${resources_list}

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    30    ${start}    1   SOFT  CRITICAL
    Should Be True    ${result}    hosts table not updated

    ${resources_list}    Ctn Create Otl Request    ${2}    host_1
    Ctn Send Otl To Engine    4317    ${resources_list}


    Sleep    2
    ${resources_list}    Ctn Create Otl Request    ${2}    host_1
    Ctn Send Otl To Engine    4317    ${resources_list}
    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    30    ${start}    1   HARD  CRITICAL
    Should Be True    ${result}    hosts table not updated

BEOTEL_TELEGRAF_CHECK_SERVICE
    [Documentation]    we send nagios telegraf formatted datas and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    mon-34004
    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule    0    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=nagios_telegraf --extractor=attributes --host_path=resource_metrics.scope_metrics.data.data_points.attributes.host --service_path=resource_metrics.scope_metrics.data.data_points.attributes.service
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check_icmp
    Ctn Set Services Passive       0    service_1
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.1
    ...    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd

    Ctn Config BBDO3    1
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the otel server start
    Ctn Wait For Otel Server To Be Ready    ${start}
    Sleep    1


    # feed and check
    ${start}    Ctn Get Round Current Date
    ${resources_list}    Ctn Create Otl Request    ${0}    host_1    service_1
    Log To Console    export metrics
    Ctn Send Otl To Engine    4317    ${resources_list}

    ${result}    Ctn Check Service Output Resource Status With Timeout    host_1    service_1    30    ${start}    0  HARD   OK
    Should Be True    ${result}    services table not updated

    # check then feed, three times to modify hard state
    ${start}    Ctn Get Round Current Date
    ${resources_list}    Ctn Create Otl Request    ${2}    host_1    service_1
    Ctn Send Otl To Engine    4317    ${resources_list}

    ${result}    Ctn Check Service Output Resource Status With Timeout    host_1    service_1    30    ${start}    2  SOFT  CRITICAL
    Should Be True    ${result}    services table not updated

    ${resources_list}    Ctn Create Otl Request    ${2}    host_1    service_1
    Ctn Send Otl To Engine    4317    ${resources_list}

    Sleep    2
    ${resources_list}    Ctn Create Otl Request    ${2}    host_1    service_1
    Ctn Send Otl To Engine    4317    ${resources_list}
    ${result}    Ctn Check Service Output Resource Status With Timeout    host_1    service_1    30    ${start}    2  HARD  CRITICAL
    Should Be True    ${result}    services table not updated

BEOTEL_SERVE_TELEGRAF_CONFIGURATION_CRYPTED
    [Documentation]    we configure engine with a telegraf conf server and we check telegraf conf file
    [Tags]    broker    engine    opentelemetry    mon-35539
    Ctn Create Key And Certificate    localhost    /tmp/otel/server.key    /tmp/otel/server.crt
    Ctn Config Engine    ${1}    ${3}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0, "telegraf_conf_server": {"http_server":{"port": 1443, "encryption": true, "public_cert": "/tmp/otel/server.crt", "private_key": "/tmp/otel/server.key"}, "check_interval":60, "engine_otel_endpoint": "127.0.0.1:4317"}}
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

    Create Directory    /etc/centreon-engine-whitelist
    Empty Directory    /etc/centreon-engine-whitelist
    ${whitelist_content}    Catenate   {"whitelist":{"wildcard":["/usr/lib/nagios/plugins/check_icmp *"]}}
    Create File    /etc/centreon-engine-whitelist/test    ${whitelist_content}

    Ctn Config Engine    ${1}    ${3}    ${3}
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

    Ctn Engine Config Replace Value In Services    ${0}    service_3    check_command    otel_check_icmp_serv_3
    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp_serv_3
    ...    rejected_by_whitelist
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

    ${content}    Create List    service_3: this command cannot be executed because of security restrictions on the poller. A whitelist has been defined, and it does not include this command.
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "service 3 blacklisted unavailable."




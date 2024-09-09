*** Settings ***
Documentation       Engine/Broker tests on centreon monitoring agent

Library             Collections
Library             DatabaseLibrary
Library             DateTime
Library             String

Resource            ../resources/resources.resource

Library             ../resources/Agent.py
Library             ../resources/Broker.py
Library             ../resources/Common.py
Library             ../resources/Engine.py

Suite Setup         Ctn Create Cert And Init
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs

*** Test Cases ***

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
    Ctn Set Hosts Passive  ${0}  host_1 

    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"

    Ctn Engine Config Add Command    ${0}  otel_check_icmp   ${echo_command}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    ${content}    Create List    unencrypted server listening on 0.0.0.0:4317
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4317" should be available.
    Sleep    1

    ${result}    Ctn Check Host Check Status With Timeout    host_1    30    ${start_int}    0    OK - 127.0.0.1
    Should Be True    ${result}    hosts table not updated

    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp_2
    
    ${echo_command}   Ctn Echo Command  "OK check2 - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command  ${0}    otel_check_icmp_2  ${echo_command}    OTEL connector

    #update conf engine, it must be taken into account by agent
    Log To Console    modify engine conf and reload engine
    Ctn Reload Engine

    #wait for new data from agent
    ${start}    Ctn Get Round Current Date
    ${content}    Create List    description: \"OK check2
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    22
    Should Be True    ${result}    "description: "OK check2" should be available.

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
    Ctn Set Services Passive       0    service_1

    ${check_cmd}  Ctn Check Pl Command   --id 456

    Ctn Engine Config Add Command    ${0}    otel_check   ${check_cmd}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    #service_1 check fail CRITICAL
    Ctn Set Command Status    456    ${2}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    ${content}    Create List    unencrypted server listening on 0.0.0.0:4317
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4317" should be available.
    
    ${result}    Ctn Check Service Check Status With Timeout    host_1  service_1  60  ${start_int}  2  Test check 456
    Should Be True    ${result}    services table not updated

    ${start}    Ctn Get Round Current Date
    #service_1 check ok
    Ctn Set Command Status    456    ${0}

    ${result}    Ctn Check Service Check Status With Timeout    host_1  service_1  60  ${start}  0  Test check 456
    Should Be True    ${result}    services table not updated


BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST
    [Documentation]    agent check host with reversed connection and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-63843
    Ctn Config Engine    ${1}    ${2}    ${2}

    ${host_host_name}      Ctn Host Hostname
    ${config_content}    Catenate  {"max_length_grpc_log":0,"centreon_agent":{"check_interval":10, "export_period":15, "reverse_connections":[{"host": "${host_host_name}","port": 4320}]}} 
    Ctn Add Otl ServerModule   0    ${config_content}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1 

    ${echo_command}   Ctn Echo Command  "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp   ${echo_command}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Reverse Centreon Agent
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for engine to connect to agent
    ${content}    Create List    init from ${host_host_name}:4320
    ${result}    Ctn Find Regex In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "init from ${host_host_name}:4320" not found in log
    Sleep    1

    ${result}    Ctn Check Host Check Status With Timeout    host_1    30    ${start_int}    0    OK - 127.0.0.1
    Should Be True    ${result}    hosts table not updated

    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp_2

    ${echo_command}   Ctn Echo Command   "OK check2 - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp_2   ${echo_command}    OTEL connector

    #update conf engine, it must be taken into account by agent
    Log To Console    modify engine conf and reload engine
    Ctn Reload Engine

    #wait for new data from agent
    ${start}    Ctn Get Round Current Date
    ${content}    Create List    description: \"OK check2
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    "description: "OK check2" should be available.

    ${result}    Ctn Check Host Check Status With Timeout    host_1    30    ${start}    0    OK check2 - 127.0.0.1: rta 0,010ms, lost 0%
    Should Be True    ${result}    hosts table not updated


BEOTEL_REVERSE_CENTREON_AGENT_CHECK_SERVICE
    [Documentation]    agent check service with reversed connection and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-63843
    Ctn Config Engine    ${1}    ${2}    ${2}

    ${host_host_name}      Ctn Host Hostname
    ${config_content}    Catenate  {"max_length_grpc_log":0,"centreon_agent":{"check_interval":10, "export_period":15, "reverse_connections":[{"host": "${host_host_name}","port":4320}]}} 
    Ctn Add Otl ServerModule   0    ${config_content}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check
    Ctn Set Services Passive       0    service_1

    ${check_cmd}  Ctn Check Pl Command   --id 456
    Ctn Engine Config Add Command    ${0}    otel_check   ${check_cmd}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    #service_1 check fail CRITICAL
    Ctn Set Command Status    456    ${2}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Reverse Centreon Agent
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for engine to connect to agent
    ${content}    Create List    init from ${host_host_name}:4320
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "init from ${host_host_name}:4320" not found in log

    
    ${result}    Ctn Check Service Check Status With Timeout    host_1  service_1  60  ${start_int}  2  Test check 456
    Should Be True    ${result}    services table not updated

    ${start}    Ctn Get Round Current Date
    #service_1 check ok
    Ctn Set Command Status    456    ${0}

    ${result}    Ctn Check Service Check Status With Timeout    host_1  service_1  60  ${start}  0  Test check 456
    Should Be True    ${result}    services table not updated

BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST_CRYPTED
    [Documentation]    agent check host with encrypted reversed connection and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-63843
    Ctn Config Engine    ${1}    ${2}    ${2}

    ${host_host_name}      Ctn Host Hostname
    Ctn Add Otl ServerModule
    ...    0
    ...    {"max_length_grpc_log":0,"centreon_agent":{"check_interval":10, "export_period":15, "reverse_connections":[{"host": "${host_host_name}","port": 4321, "encryption": true, "ca_certificate": "/tmp/server_grpc.crt"}]}}

    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1 

    ${echo_command}   Ctn Echo Command  "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp   ${echo_command}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Reverse Centreon Agent   /tmp/server_grpc.key  /tmp/server_grpc.crt
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for engine to connect to agent
    ${content}    Create List    init from ${host_host_name}:4321
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "init from ${host_host_name}:4321" not found in log
    Sleep    1

    ${result}    Ctn Check Host Check Status With Timeout    host_1    30    ${start_int}    0    OK - 127.0.0.1
    Should Be True    ${result}    hosts table not updated



BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED
    [Documentation]    agent check host with encrypted connection and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-63843
    Ctn Config Engine    ${1}    ${2}    ${2}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": true, "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    
    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp  ${echo_command}   OTEL connector
    Ctn Set Hosts Passive  ${0}  host_1 

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent  ${None}  ${None}  /tmp/server_grpc.crt
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    ${content}    Create List    encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    Sleep    1

    ${result}    Ctn Check Host Check Status With Timeout    host_1    30    ${start_int}    0    OK - 127.0.0.1
    Should Be True    ${result}    hosts table not updated

*** Keywords ***
Ctn Create Cert And Init
    [Documentation]  create key and certificates used by agent and engine on linux side
    ${host_name}  Ctn Get Hostname
    Ctn Create Key And Certificate  ${host_name}  /tmp/server_grpc.key   /tmp/server_grpc.crt

    Ctn Clean Before Suite
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
    Ctn Wait For Otel Server To Be Ready    ${start}
    Sleep    1s

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    60    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated

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

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    60    ${start_int}    0  HARD  OK check2 - 127.0.0.1: rta 0,010ms, lost 0%
    Should Be True    ${result}    resources table not updated


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
    Ctn Wait For Otel Server To Be Ready    ${start}
    
    ${result}    Ctn Check Service Output Resource Status With Timeout    host_1    service_1    60    ${start_int}    2  HARD  Test check 456
    Should Be True    ${result}    resources table not updated

    ${start}    Ctn Get Round Current Date
    #service_1 check ok
    Ctn Set Command Status    456    ${0}

    ${result}    Ctn Check Service Output Resource Status With Timeout    host_1    service_1    60    ${start_int}    0  HARD  Test check 456
    Should Be True    ${result}    resources table not updated


BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST
    [Documentation]    agent check host with reversed connection and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-63843
    Ctn Config Engine    ${1}    ${2}    ${2}

    ${host_host_name}      Ctn Host Hostname
    ${config_content}    Catenate    {"max_length_grpc_log":0,"centreon_agent":{"check_interval":10, "export_period":15, "reverse_connections":[{"host": "${host_host_name}","port": 4320}]}} 
    Ctn Add Otl ServerModule   0    ${config_content}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1 

    ${echo_command}    Ctn Echo Command    "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
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
    Sleep    1s

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    60    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated

    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp_2

    ${echo_command}    Ctn Echo Command    "OK check2 - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp_2   ${echo_command}    OTEL connector

    #update conf engine, it must be taken into account by agent
    Log To Console    modify engine conf and reload engine
    Ctn Reload Engine

    #wait for new data from agent
    ${start}    Ctn Get Round Current Date
    ${content}    Create List    description: \"OK check2
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    "description: "OK check2" should be available.

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    60    ${start_int}    0  HARD  OK check2 - 127.0.0.1: rta 0,010ms, lost 0%
    Should Be True    ${result}    resources table not updated


BEOTEL_REVERSE_CENTREON_AGENT_CHECK_SERVICE
    [Documentation]    agent check service with reversed connection and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-63843
    Ctn Config Engine    ${1}    ${2}    ${2}

    ${host_host_name}      Ctn Host Hostname
    ${config_content}    Catenate    {"max_length_grpc_log":0,"centreon_agent":{"check_interval":10, "export_period":15, "reverse_connections":[{"host": "${host_host_name}","port":4320}]}} 
    Ctn Add Otl ServerModule   0    ${config_content}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check
    Ctn Set Services Passive    0    service_1

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

    ${result}    Ctn Check Service Output Resource Status With Timeout    host_1    service_1    60    ${start_int}    0  HARD  Test check 456
    Should Be True    ${result}    resources table not updated

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
    Ctn Set Hosts Passive    ${0}    host_1 

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
    Sleep    1s

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    30    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated



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
    Ctn Engine Config Add Command    ${0}    otel_check_icmp    ${echo_command}    OTEL connector
    Ctn Set Hosts Passive    ${0}    host_1 

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt
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
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    Sleep    1

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    120    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated

BEOTEL_CENTREON_AGENT_CHECK_NATIVE_CPU
    [Documentation]    agent check service with native check cpu and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-149536
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

    Ctn Engine Config Add Command    ${0}    otel_check   {"check": "cpu_percentage"}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Clear Metrics

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    Ctn Wait For Otel Server To Be Ready    ${start}
    
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    0    120    HARD
    Should Be True    ${result}    resources table not updated

    ${metrics_list}    Create List   cpu.utilization.percentage    0#core.cpu.utilization.percentage
    ${result}    Ctn Compare Metrics Of Service    1    ${metrics_list}    60
    Should Be True    ${result}    metrics not updated


    #a small threshold to make service_1 warning
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check2

    Ctn Engine Config Add Command    ${0}    otel_check2   {"check": "cpu_percentage", "args": {"warning-average" : "0.01"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    1    60    ANY
    Should Be True    ${result}    resources table not updated

    #a small threshold to make service_1 critical
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check3

    Ctn Engine Config Add Command    ${0}    otel_check3   {"check": "cpu_percentage", "args": {"critical-average" : "0.02", "warning-average" : "0.01"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    2    60    ANY
    Should Be True    ${result}    resources table not updated


BEOTEL_CENTREON_AGENT_CHECK_NATIVE_STORAGE
    [Documentation]    agent check service with native check storage and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-147936

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" != "WSL"    "This test is only for WSL"

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

    Ctn Engine Config Add Command    ${0}    otel_check   {"check": "storage", "args": { "free": true, "unit": "%"}}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Clear Metrics

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    Ctn Wait For Otel Server To Be Ready    ${start}
    
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    0    120    HARD
    Should Be True    ${result}    resources table not updated

    ${expected_perfdata}    Ctn Get Drive Statistics    free_{}:\\
    ${result}    Ctn Check Service Perfdata    host_1    service_1    60    1    ${expected_perfdata}
    Should be True    ${result}    data_bin not updated


    #a small threshold to make service_1 warning
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check2

    Ctn Engine Config Add Command    ${0}    otel_check2   {"check": "storage", "args": {"warning" : "10", "unit": "B"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    1    60    ANY
    Should Be True    ${result}    resources table not updated

    #a small threshold to make service_1 critical
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check3

    Ctn Engine Config Add Command    ${0}    otel_check3   {"check": "storage", "args": {"critical" : "10", "unit": "B"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    2    60    ANY
    Should Be True    ${result}    resources table not updated



BEOTEL_CENTREON_AGENT_CHECK_NATIVE_UPTIME
    [Documentation]    agent check service with native check uptime and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-147919

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" != "WSL"    "This test is only for WSL"

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

    Ctn Engine Config Add Command    ${0}    otel_check   {"check": "uptime"}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Clear Metrics

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    Ctn Wait For Otel Server To Be Ready    ${start}
    
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    0    120    HARD
    Should Be True    ${result}    resources table not updated

    ${expected_perfdata}    Ctn Get Uptime
    ${result}    Ctn Check Service Perfdata    host_1    service_1    60    600    ${expected_perfdata}
    Should be True    ${result}    data_bin not updated


    #a small threshold to make service_1 warning
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check2

    Ctn Engine Config Add Command    ${0}    otel_check2   {"check": "uptime", "args": {"warning-uptime" : "1000000000"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    1    60    ANY
    Should Be True    ${result}    resources table not updated

    #a small threshold to make service_1 critical
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check3

    Ctn Engine Config Add Command    ${0}    otel_check3   {"check": "uptime", "args": {"critical-uptime" : "1000000000"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    2    60    ANY
    Should Be True    ${result}    resources table not updated

BEOTEL_CENTREON_AGENT_CHECK_NATIVE_MEMORY
    [Documentation]    agent check service with native check memory and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-147916

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" != "WSL"    "This test is only for WSL"

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

    Ctn Engine Config Add Command    ${0}    otel_check   {"check": "memory"}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Clear Metrics

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    Ctn Wait For Otel Server To Be Ready    ${start}
    
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    0    120    HARD
    Should Be True    ${result}    resources table not updated

    ${expected_perfdata}    Ctn Get Memory
    #many process (cbd centengine under wsl) had consumed a lot of memory since tests began so we have to use a huge interval (800 Mo)
    ${result}    Ctn Check Service Perfdata    host_1    service_1    60    800000000    ${expected_perfdata}
    Should be True    ${result}    data_bin not updated


    #a small threshold to make service_1 warning
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check2

    Ctn Engine Config Add Command    ${0}    otel_check2   {"check": "memory", "args": {"warning-usage-prct" : "1"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    1    60    ANY
    Should Be True    ${result}    resources table not updated

    #a small threshold to make service_1 critical
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check3

    Ctn Engine Config Add Command    ${0}    otel_check3   {"check": "memory", "args": {"critical-usage-prct" : "1"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    2    60    ANY
    Should Be True    ${result}    resources table not updated

BEOTEL_CENTREON_AGENT_CHECK_NATIVE_SERVICE
    [Documentation]    agent check service with native check service and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-147933

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" != "WSL"    "This test is only for WSL"

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

    Ctn Engine Config Add Command    ${0}    otel_check   {"check": "service"}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Clear Metrics

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    Ctn Wait For Otel Server To Be Ready    ${start}
    
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    0    120    HARD
    Should Be True    ${result}    resources table not updated

    # as centagent retrieve much more services than powershell (on my computer 660 versus 263), we can't compare perfdatas
    # ${expected_perfdata}    Ctn Get Service
    # ${result}    Ctn Check Service Perfdata    host_1    service_1    60    2    ${expected_perfdata}
    # Should be True    ${result}    data_bin not updated


    #a small threshold to make service_1 warning
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check2

    Ctn Engine Config Add Command    ${0}    otel_check2   {"check": "service", "args": {"warning-total-running" : "1000"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    1    60    ANY
    Should Be True    ${result}    resources table not updated

    #a small threshold to make service_1 critical
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check3

    Ctn Engine Config Add Command    ${0}    otel_check3   {"check": "service", "args": {"critical-total-running" : "1000"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    2    60    ANY
    Should Be True    ${result}    resources table not updated


BEOTEL_CENTREON_AGENT_CHECK_HEALTH
    [Documentation]    agent check health and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-147934
    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"check_interval":10, "export_period":15}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    cpu_check
    Ctn Engine Config Replace Value In Services    ${0}    service_2    check_command    health_check
    Ctn Set Services Passive       0    service_[1-2]


    Ctn Engine Config Add Command    ${0}    cpu_check   {"check": "cpu_percentage"}    OTEL connector
    Ctn Engine Config Add Command    ${0}    health_check   {"check": "health"}    OTEL connector
    Ctn Engine Config Add Command    ${0}    health_check_warning   {"check": "health", "args":{"warning-interval": "5"} }    OTEL connector
    Ctn Engine Config Add Command    ${0}    health_check_critical   {"check": "health", "args":{"warning-interval": "5", "critical-interval": "6"} }    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Clear Metrics

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    Ctn Wait For Otel Server To Be Ready    ${start}
    
    Log To Console    service_1 and service_2 must be ok
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    0    120    HARD
    Should Be True    ${result}    resources table not updated for service_1

    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_2    0    60    HARD
    Should Be True    ${result}    resources table not updated for service_2

    ${metrics_list}    Create List   cpu.utilization.percentage    0#core.cpu.utilization.percentage
    ${result}    Ctn Compare Metrics Of Service    1    ${metrics_list}    30
    Should Be True    ${result}    cpu metrics not updated

    ${metrics_list}    Create List   runtime    interval
    ${result}    Ctn Compare Metrics Of Service    2    ${metrics_list}    30
    Should Be True    ${result}    health metrics not updated

    Log To Console    service_2 must be warning
    Ctn Engine Config Replace Value In Services    ${0}    service_2    check_command    health_check_warning
    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_2    1    60    ANY
    Should Be True    ${result}    resources table not updated for service_2

    Log To Console    service_2 must be critical
    Ctn Engine Config Replace Value In Services    ${0}    service_2    check_command    health_check_critical
    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_2    2    60    ANY
    Should Be True    ${result}    resources table not updated for service_2


BEOTEL_CENTREON_AGENT_CEIP
   [Documentation]    we connect an agent to engine and we expect a row in agent_information table
   [Tags]    broker    engine    opentelemetry    MON-145030
   Ctn Config Engine    ${1}    ${2}    ${2}
   Ctn Add Otl ServerModule
   ...    0
   ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"check_interval":10, "export_period":15}}
   Ctn Config Add Otl Connector
   ...    0
   ...    OTEL connector
   ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
   Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    cpu_check
   Ctn Engine Config Replace Value In Services    ${0}    service_2    check_command    health_check
   Ctn Set Services Passive       0    service_[1-2]


   Ctn Engine Config Add Command    ${0}    cpu_check   {"check": "cpu_percentage"}    OTEL connector
   Ctn Engine Config Add Command    ${0}    health_check   {"check": "health"}    OTEL connector

   Ctn Engine Config Set Value    0    log_level_checks    trace

   Ctn Clear Db    metrics

   Ctn Config Broker    central
   Ctn Config Broker    module
   Ctn Config Broker    rrd
   Ctn Config BBDO3    1
   Ctn Config Centreon Agent
   Ctn Broker Config Log    central    sql    trace
   Ctn Broker Config Output Set    central    central-broker-unified-sql    instance_timeout    10

   Ctn Clear Retention

   ${start}    Ctn Get Round Current Date
   Ctn Start Broker
   Ctn Start Engine
   Ctn Start Agent

   # Let's wait for the otel server start
   Ctn Wait For Otel Server To Be Ready    ${start}

   ${result}    Ctn Check Agent Information    1    1    120
   Should Be True    ${result}    agent_information table not updated as expected

   Log To Console    "stop engine"
   Ctn Stop Engine
   ${result}    Ctn Check Agent Information    0    0    120
   Should Be True    ${result}    agent_information table not updated as expected


BEOTEL_CENTREON_AGENT_LINUX_NO_DEFUNCT_PROCESS
    [Documentation]    agent check host and we expect to get it in check result
    [Tags]    broker    engine    opentelemetry    MON-156455

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" == "WSL"    "This test is only for linux agent version"

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

    Ctn Engine Config Add Command    ${0}  otel_check_icmp   turlututu    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    Ctn Wait For Otel Server To Be Ready    ${start}
    Sleep    30s

    ${nb_agent_process}    Ctn Get Nb Process    centagent
    Should Be True    ${nb_agent_process} >= 1 and ${nb_agent_process} <= 2    "There should be no centagent defunct process"

    Log To Console    Stop agent
    Ctn Kindly Stop Agent

    FOR   ${i}    IN RANGE    1    10
        ${nb_agent_process}    Ctn Get Nb Process    centagent
        IF    ${nb_agent_process} == 0
            Exit For Loop
        ELSE
            Sleep    2s
        END
    END

    Should Be True    ${nb_agent_process} == 0    "There should be no centagent process"


*** Keywords ***
Ctn Create Cert And Init
    [Documentation]  create key and certificates used by agent and engine on linux side
    ${host_name}  Ctn Get Hostname
    ${run_env}       Ctn Run Env
    IF    "${run_env}" == "WSL"
        Copy File    ../server_grpc.key    /tmp/server_grpc.key
        Copy File    ../server_grpc.crt    /tmp/server_grpc.crt
    ELSE
        Ctn Create Key And Certificate  ${host_name}  /tmp/server_grpc.key   /tmp/server_grpc.crt
    END

    Ctn Clean Before Suite

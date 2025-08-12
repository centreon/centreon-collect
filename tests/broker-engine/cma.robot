*** Settings ***
Documentation       Engine/Broker tests on centreon monitoring agent

Library             Collections
Library             DatabaseLibrary
Library             DateTime
Library             String


Resource            ../resources/import.resource

Library             ../resources/Agent.py


Suite Setup         Ctn Create Cert And Init
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs

*** Test Cases ***

BEOTEL_CENTREON_AGENT_CHECK_HOST
    [Documentation]    Given an agent host checked by centagent, we set a first output to check command, 
    ...    modify it, reload engine and expect the new output in resource table
    [Tags]    broker    engine    opentelemetry    MON-63843
    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0, "centreon_agent":{"export_period":10}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_interval    1

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
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"export_period":5}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check
    Ctn Set Services Passive       0    service_1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    1

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
    ${config_content}    Catenate    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port": 4320}]}} 
    Ctn Add Otl ServerModule   0    ${config_content}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1 
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_interval    1

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
    ${result}    Ctn Find Regex In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
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
    ${config_content}    Catenate    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port":4320}]}} 
    Ctn Add Otl ServerModule   0    ${config_content}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check
    Ctn Set Services Passive    0    service_1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    1

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
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
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
    ...    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port": 4321, "encryption": true, "ca_certificate": "/tmp/reverse_server_grpc.crt"}]}}

    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive    ${0}    host_1 
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_interval    1

    ${echo_command}   Ctn Echo Command  "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp   ${echo_command}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    ${token1}    Ctn Create Jwt Token    ${3600}

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' == 'None'
        Ctn Add Token Agent Otl Server   0    0    ${token1}
    END


    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Reverse Centreon Agent    /tmp/reverse_server_grpc.key    /tmp/reverse_server_grpc.crt    ${None}    ${token1}
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
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
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
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": true, "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"}, "centreon_agent":{"export_period":5}, "max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    
    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp    ${echo_command}    OTEL connector
    Ctn Set Hosts Passive    ${0}    host_1 
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_interval    1

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    ${token}    Ctn Create Jwt Token    ${60}
    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt    ${token}
    Ctn Add Token Otl Server Module    0    ${token}   
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
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
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
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"export_period":5}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check
    Ctn Set Services Passive       0    service_1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    1

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
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"export_period":5}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check
    Ctn Set Services Passive       0    service_1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    1

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
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"export_period":5}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check
    Ctn Set Services Passive       0    service_1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    1

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
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"export_period":5}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check
    Ctn Set Services Passive       0    service_1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    1

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
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"export_period":5}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check
    Ctn Set Services Passive       0    service_1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    1

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
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"export_period":5}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    cpu_check
    Ctn Engine Config Replace Value In Services    ${0}    service_2    check_command    health_check
    Ctn Set Services Passive       0    service_[1-2]
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    1
    Ctn Engine Config Replace Value In Services    ${0}    service_2    check_interval    1

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


BEOTEL_CENTREON_AGENT_CHECK_DIFFERENT_INTERVAL
    [Documentation]    Given and agent who has to execute checks with different intervals, we expect to find these intervals in data_bin
    [Tags]    broker    engine    opentelemetry    MON-164494
    Ctn Config Engine    ${1}    ${2}    ${3}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{ "export_period":5}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    health_check
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    1
    Ctn Engine Config Replace Value In Services    ${0}    service_2    check_command    health_check
    Ctn Engine Config Replace Value In Services    ${0}    service_2    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_3    check_command    health_check
    Ctn Engine Config Replace Value In Services    ${0}    service_3    check_interval    3
    Ctn Set Services Passive       0    service_[1-3]


    Ctn Engine Config Add Command    ${0}    health_check   {"check": "health"}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace
    Ctn Engine Config Set Value    0    interval_length    10
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

    ${result}    Ctn Check Service Check Interval   host_1    service_1    80    10    5
    Should Be True    ${result}    check_interval is not respected for service_1
    ${result}    Ctn Check Service Check Interval   host_1    service_2    80    20    5
    Should Be True    ${result}    check_interval is not respected for service_2
    ${result}    Ctn Check Service Check Interval   host_1    service_3    80    30    5
    Should Be True    ${result}    check_interval is not respected for service_3


BEOTEL_CENTREON_AGENT_CHECK_EVENTLOG
    [Documentation]    Given an agent with eventlog check, we expect status, output and metrics
    [Tags]    broker    engine    opentelemetry    MON-155395

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" != "WSL"    "This test is only for WSL"

    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"export_period":5}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    eventlog_check
    Ctn Set Services Passive       0    service_1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    1


    Ctn Engine Config Add Command    ${0}    eventlog_check   {"check":"eventlog_nscp", "args":{ "file": "Application", "filter-event": "written > -1s and level in ('error', 'warning', critical)", "empty-state": "No event as expected"} }    OTEL connector
    Ctn Engine Config Add Command    ${0}    eventlog_check_warning    {"check":"eventlog_nscp", "args":{ "file": "Application", "filter-event": "written > -2w", "warning-status": "level in ('info')", "output-syntax": "{status}: {count} '{problem-list}'", "critical-status": "written > -1s && level == 'critical'"} }     OTEL connector
    Ctn Engine Config Add Command    ${0}    eventlog_check_critical   {"check":"eventlog_nscp", "args":{ "file": "Application", "filter-event": "written > -2w", "warning-status": "level in ('info')", "output-syntax": "{status}: {count} '{problem-list}'", "critical-status": "level == 'info'", "verbose": "0"} }    OTEL connector

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
    
    Log To Console    service_1 must be ok
    ${result}     Ctn Check Service Output Resource Status With Timeout    host_1    service_1    120    ${start}    0    HARD    No event as expected
    Should Be True    ${result}    resources table not updated for service_1

    ${metrics_list}    Create List   critical-count    warning-count
    ${result}    Ctn Compare Metrics Of Service    1    ${metrics_list}    30
    Should Be True    ${result}    eventlog metrics not updated

    Log To Console    service_1 must be warning
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    eventlog_check_warning
    Ctn Reload Engine
    ${result}     Ctn Check Service Status With Timeout Rt    host_1    service_1    1    60    ANY
    Should Be True    ${result[0]}    resources table not updated for service_1
    ${nb_lines}    Get Line Count    ${result[1]}
    Should Be True    ${nb_lines} > 1    output is not multiline

    Log To Console    service_1 must be critical
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    eventlog_check_critical
    Ctn Reload Engine
    ${result}     Ctn Check Service Status With Timeout Rt    host_1    service_1    2    60    ANY
    Should Be True    ${result[0]}    resources table not updated for service_1
    ${nb_lines}    Get Line Count    ${result[1]}
    Should Be True    ${nb_lines} == 1    output must not be multiline


BEOTEL_CENTREON_AGENT_CEIP
    [Documentation]    we connect an agent to engine and we expect a row in agent_information table
    [Tags]    broker    engine    opentelemetry    MON-145030
    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"export_period":5}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    cpu_check
    Ctn Engine Config Replace Value In Services    ${0}    service_2    check_command    health_check
    Ctn Set Services Passive       0    service_[1-2]
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    1


    Ctn Engine Config Add Command    ${0}    cpu_check   {"check": "cpu_percentage"}    OTEL connector
    Ctn Engine Config Add Command    ${0}    health_check   {"check": "health"}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Clear Db    metrics
    Ctn Clear Db    agent_information

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
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0, "centreon_agent":{"export_period":10}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1 
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_interval    1

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


 NON_TLS_CONNECTION_WARNING
    [Documentation]    Given an agent starts a non-TLS connection,
    ...    we expect to get a warning message.
    [Tags]    agent    engine    opentelemetry    MON-159308    
    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0, "centreon_agent":{"export_period":10}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --server=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
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
    Ctn Broker Config Log    module0    grpc    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    Ctn Wait For Otel Server To Be Ready    ${start}

    ${content}    Create List    NON TLS CONNECTION ARE ALLOWED FOR Agents // THIS IS NOT ALLOWED IN PRODUCTION
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    22
    Should Be True    ${result}    "A warning message should appear : NON TLS CONNECTION ARE ALLOWED FOR Agents // THIS IS NOT ALLOWED IN PRODUCTION.
    
    # check if the agent is in windows or not, to get the right log path
    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' == 'None'
        # not windows 
            ${content}    Create List    NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION
            ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    22    agent_format=True
            Should Be True    ${result}    "A warning message should appear : NON TLS CONNECTION ARE ALLOWED // THIS IS NOT ALLOWED IN PRODUCTION.
    ELSE
        # in windows ,Ctn Start Agent doesn't create the agent
        #  the agent are start in a different time, so we cant use find in the log
        ${log_path}    Set Variable    ../reports/centagent.log
        ${result}    Grep File    ${log_path}    NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION
        Should Not Be Empty    ${result}    "A warning message should appear : NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION.
    END


NON_TLS_CONNECTION_WARNING_REVERSED
    [Documentation]    Given an agent starts a non-TLS connection reversed,
    ...    we expect to get a warning message.
    [Tags]    agent    engine    opentelemetry    MON-159308 
    Ctn Config Engine    ${1}    ${2}    ${2}

    ${host_host_name}      Ctn Host Hostname
    ${config_content}    Catenate    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port": 4320}]}} 
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
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for engine to connect to agent
    ${content}    Create List    init from ${host_host_name}:4320
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
    Should Be True    ${result}    "init from ${host_host_name}:4320" not found in log

    ${content}    Create List    NON TLS CONNECTION ARE ALLOWED FOR Agents(${host_host_name}:4320) // THIS IS NOT ALLOWED IN PRODUCTION
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    22
    Should Be True    ${result}    "A warning message should appear : NON TLS CONNECTION ARE ALLOWED FOR Agents // THIS IS NOT ALLOWED IN PRODUCTION.
    
    # check if the agent is in windows or not, to get the right log path
    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' == 'None'
        # not windows 
            ${content}    Create List    NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION
            ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    22    agent_format=True
            Should Be True    ${result}    "A warning message should appear : NON TLS CONNECTION ARE ALLOWED // THIS IS NOT ALLOWED IN PRODUCTION.
    ELSE
        # in windows ,Ctn Start Agent doesn't create the agent
        #  the agent are start in a different time, so we cant use find in the log
        ${log_path}    Set Variable    ../reports/reverse_centagent.log
        ${result}    Grep File    ${log_path}    NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION
        Should Not Be Empty    ${result}    "A warning message should appear : NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION.
    END


NON_TLS_CONNECTION_WARNING_REVERSED_ENCRYPTED
    [Documentation]    Given agent with encrypted reversed connection, we expect no warning message.
    [Tags]    agent    engine    opentelemetry    MON-159308    
    Ctn Config Engine    ${1}    ${2}    ${2}
    ${host_host_name}      Ctn Host Hostname
    Ctn Add Otl ServerModule
    ...    0
    ...    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port": 4321, "encryption": true, "ca_certificate": "/tmp/reverse_server_grpc.crt"}]}}

    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive    ${0}    host_1 

    ${echo_command}   Ctn Echo Command  "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp   ${echo_command}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace


    ${token1}    Ctn Create Jwt Token    ${3600}

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' == 'None'
        Ctn Add Token Agent Otl Server   0    0    ${token1}
    END
    


    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Reverse Centreon Agent    /tmp/reverse_server_grpc.key    /tmp/reverse_server_grpc.crt    ${None}    ${token1}
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # for win : 
    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' == 'None'
            ${log_path}    Set Variable    ${agentlog}
    ELSE
            ${log_path}    Set Variable    ../reports/encrypted_reverse_centagent.log
    END

    # Let's wait for engine to connect to agent
    ${content}    Create List    init from ${host_host_name}:4321
    ${result}    Ctn Find In Log With Timeout   ${engineLog0}    ${start}    ${content}    20
    Should Be True    ${result}    "init from ${host_host_name}:4321" not found in log"

    ${content}    Create List    NON TLS CONNECTION ARE ALLOWED FOR Agents(${host_host_name}:4320) // THIS IS NOT ALLOWED IN PRODUCTION
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
    Should Not Be True   ${result}   "This warrning message shouldn't appear : NON TLS CONNECTION ARE ALLOWED FOR Agents // THIS IS NOT ALLOWED IN PRODUCTION."
    
    # check if the agent is in windows or not, to get the right log path
    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' == 'None'
        # not windows 
            ${content}    Create List    NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION
            ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    22    agent_format=True
            Should Not Be True    ${result}    "This warrning message shouldn't appear : NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION."
    ELSE
        # in windows ,Ctn Start Agent doesn't create the agent
        #  the agent are start in a different time, so we cant use find in the log
        ${log_path}    Set Variable    ../reports/encrypted_reverse_centagent.log
        ${result}    Grep File    ${log_path}    NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION
        Should Be Empty    ${result}    "This warrning message shouldn't appear : NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION."
    END


NON_TLS_CONNECTION_WARNING_ENCRYPTED
    [Documentation]    Given agent with encrypted connection, we expect no warning message.
    [Tags]    agent    engine    opentelemetry    MON-159308 
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
    ${token}    Ctn Create Jwt Token    ${3600}
    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt    ${token}
    Ctn Add Token Otl Server Module    0    ${token}
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
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.

    ${content}    Create List    NON TLS CONNECTION ARE ALLOWED FOR Agents // THIS IS NOT ALLOWED IN PRODUCTION
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
    Should Not Be True   ${result}    "This warrning message shouldn't appear : NON TLS CONNECTION ARE ALLOWED FOR Agents // THIS IS NOT ALLOWED IN PRODUCTION.
    
    # check if the agent is in windows or not, to get the right log path
    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' == 'None'
        # not windows 
            ${content}    Create List    NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION
            ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    22    agent_format=True
            Should Not Be True    ${result}    "This warrning message shouldn't appear : NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION.
    ELSE
        # in windows ,Ctn Start Agent doesn't create the agent
        #  the agent are start in a different time, so we cant use find in the log
        ${log_path}    Set Variable    ../reports/encrypted_centagent.log
        ${result}    Grep File    ${log_path}    NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION
        Should Be Empty    ${result}    "This warrning message shouldn't appear : NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION.
    END


NON_TLS_CONNECTION_WARNING_FULL
    [Documentation]    Given an agent starts a non-TLS connection,
    ...    we expect to get a warning message.
    ...    After 1 hour, we expect to get a warning message about the connection time expired
    ...    and the connection killed.
    [Tags]    agent    engine    opentelemetry    MON-159308    unstable    Only_linux
    # this test should not be running in CI
    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0, "centreon_agent":{"export_period":10}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --server=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
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
    Ctn Broker Config Log    module0    grpc    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    Ctn Wait For Otel Server To Be Ready    ${start}
    Sleep    1s

    ${content}    Create List    NON TLS CONNECTION ARE ALLOWED FOR Agents // THIS IS NOT ALLOWED IN PRODUCTION
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    22
    Should Be True    ${result}    "A warning message should appear : NON TLS CONNECTION ARE ALLOWED FOR Agents // THIS IS NOT ALLOWED IN PRODUCTION.
    

    ${content}    Create List    NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    22    agent_format=True
    Should Be True    ${result}    "A warning message should appear : NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION.
    Sleep    3580s
    ${start}    Get Current Date

    ${content}    Create List    NON TLS CONNECTION TIME EXPIRED // THIS IS NOT ALLOWED IN PRODUCTION
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
    Should Be True    ${result}    "A warning message should appear : NON TLS CONNECTION TIME EXPIRED // THIS IS NOT ALLOWED IN PRODUCTION.

    ${content}    Create List    CONNECTION KILLED, AGENT NEED TO BE RESTART
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
    Should Be True    ${result}    "A warning message should appear : CONNECTION KILLED, AGENT NEED TO BE RESTART.


NON_TLS_CONNECTION_WARNING_FULL_REVERSED
    [Documentation]    Given an agent starts a non-TLS connection reverse,
    ...    we expect to get a warning message.
    ...    After 1 hour, we expect to get a warning message about the connection time expired
    ...    and the connection killed.
    [Tags]    agent    engine    opentelemetry    MON-159308    unstable    Only_linux
    # this test should not be running in CI
    Ctn Config Engine    ${1}    ${2}    ${2}

    ${host_host_name}      Ctn Host Hostname
    ${config_content}    Catenate    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port": 4320}]}} 
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
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for engine to connect to agent
    ${content}    Create List    init from ${host_host_name}:4320
    ${result}    Ctn Find Regex In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "init from ${host_host_name}:4320" not found in log
    Sleep    1s

    # Let's wait for engine to connect to agent
    ${content}    Create List    init from ${host_host_name}:4320
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
    Should Be True    ${result}    "init from ${host_host_name}:4320" not found in log

    ${content}    Create List    NON TLS CONNECTION ARE ALLOWED FOR Agents(${host_host_name}:4320) // THIS IS NOT ALLOWED IN PRODUCTION
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    22
    Should Be True    ${result}    "A warning message should appear : NON TLS CONNECTION ARE ALLOWED FOR Agents // THIS IS NOT ALLOWED IN PRODUCTION.
    
    ${content}    Create List    NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    22    agent_format=True
    Should Be True    ${result}    "A warning message should appear : NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION.
    Sleep    3580s
    ${start}    Get Current Date

    ${content}    Create List    NON TLS CONNECTION TIME EXPIRED // THIS IS NOT ALLOWED IN PRODUCTION
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
    Should Be True    ${result}    "A warning message should appear : NON TLS CONNECTION TIME EXPIRED // THIS IS NOT ALLOWED IN PRODUCTION.

    ${content}    Create List    CONNECTION KILLED, AGENT NEED TO BE RESTART
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
    Should Be True    ${result}    "A warning message should appear : CONNECTION KILLED, AGENT NEED TO BE RESTART.


BEOTEL_INVALID_CHECK_COMMANDS_AND_ARGUMENTS
    [Documentation]    Given the agent is configured with native checks for services
    ...    And the OpenTelemetry server module is added
    ...    And services are configured with incorrect check commands and arguments
    ...    When the broker, engine, and agent are started
    ...    Then the resources table should be updated with the correct status
    ...    And appropriate error messages should be generated for invalid checks
    [Tags]    broker    engine    agent    opentelemetry    MON-158969
    Ctn Config Engine    ${1}    ${2}    ${5}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"export_period":5}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_3    check_command    cpu_check
    Ctn Engine Config Replace Value In Services    ${0}    service_4    check_command    health_check
    Ctn Set Services Passive       0    service_[3-4]
    Ctn Clear Db    resources
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_3    check_interval    1
    Ctn Engine Config Replace Value In Services    ${0}    service_4    check_interval    1

    # wrong check command for service_3
    Ctn Engine Config Add Command    ${0}    cpu_check   {"check": "error"}    OTEL connector
    # wrong args value for service_4
    Ctn Engine Config Add Command    ${0}    health_check   {"check": "health","args":{"warning-interval": "A", "critical-interval": "6"} }    OTEL connector
    
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
    
    ${result}    ${content}     Ctn Check Service Resource Status With Timeout Rt    host_1    service_3    2    120    ANY
    Should Be True    ${result}    resources table not updated for service_3
    Should Be Equal As Strings    ${content}    unable to execute native check {"check": "error"} , output error : command cpu_check, unknown native check:{"check": "error"}
    ...    "Error the output for invalid check command is not correct"
 
    ${result}    ${content}     Ctn Check Service Resource Status With Timeout RT    host_1    service_4    2    120    ANY
    Should Be True    ${result}    resources table not updated for service_4
    Should Be Equal As Strings    ${content}    unable to execute native check {"check": "health","args":{"warning-interval": "A", "critical-interval": "6"} } , output error : field warning-interval is not a unsigned int string
    ...    "Error the output for invalid check args is not correct"


BEOTEL_CENTREON_AGENT_CHECK_PROCESS
    [Documentation]    Given an agent with eventlog check, we expect to get the correct status for thr centagent process running on windows host
    [Tags]    broker    engine    opentelemetry    MON-155836

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" != "WSL"    "This test is only for WSL"

    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"export_period":5}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    agent_process_check
    Ctn Set Services Passive       0    service_1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    1


    Ctn Engine Config Add Command    ${0}    agent_process_check
    ...    {"check":"process_nscp", "args":{ "filter-process": "exe = 'centagent.exe'", "ok-syntax": "{status}: all is ok"} }
    ...    OTEL connector
    
    Ctn Engine Config Add Command    ${0}    agent_process_warning
    ...    {"check":"process_nscp", "args":{ "filter-process": "exe = 'centagent.exe'", "warning-process": "virtual > 1k", "warning-rules": "warn_count > 0", "output-syntax": "{status} '{problem_list}'", "process-detail-syntax": "{exe} {pid} {virtual}"} }
    ...    OTEL connector

    Ctn Engine Config Add Command    ${0}    agent_process_critical
    ...    {"check":"process_nscp", "args":{ "filter-process": "exe = 'centagent.exe'", "warning-process": "virtual > 1k", "warning-rules": "warn_count > 0", "critical-process": "virtual > 2k", "critical-rules": "crit_count > 0", "output-syntax": "{status} '{problem_list}'", "process-detail-syntax": "{exe} {pid} {virtual}", "verbose": false} }
    ...    OTEL connector

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
    
    Log To Console    service_1 must be ok
    ${result}     Ctn Check Service Output Resource Status With Timeout    host_1    service_1    120    ${start}    0    HARD    OK: all is ok
    Should Be True    ${result}    resources table not updated for service_1

    ${metrics_list}    Create List   process.count
    ${result}    Ctn Compare Metrics Of Service    1    ${metrics_list}    30
    Should Be True    ${result}    process metrics not updated

    Log To Console    service_1 must be warning
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    agent_process_warning
    Ctn Reload Engine
    ${result}     Ctn Check Service Status With Timeout Rt    host_1    service_1    1    60    ANY
    Should Be True    ${result[0]}    resources table not updated for service_1
    ${nb_lines}    Get Line Count    ${result[1]}
    Should Be True    ${nb_lines} > 1    output is not multiline

    Log To Console    service_1 must be critical
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    agent_process_critical
    Ctn Reload Engine
    ${result}     Ctn Check Service Status With Timeout Rt    host_1    service_1    2    60    ANY
    Should Be True    ${result[0]}    resources table not updated for service_1
    ${nb_lines}    Get Line Count    ${result[1]}
    Should Be True    ${nb_lines} == 1    no verbose output must not be multiline

BEOTEL_CENTREON_AGENT_CHECK_COUNTER
    [Documentation]    Given an agent with counter check, we expect to get the correct status for the centagent process running on windows host
    [Tags]    broker    engine    opentelemetry    MON-155836

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
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    agent_process_check
    Ctn Set Services Passive       0    service_1

    Ctn Engine Config Add Command    ${0}    agent_process_check
    ...    {"check":"counter", "args":{ "counter": "\\\\System\\\\Processes","use_english":true} }
    ...    OTEL connector
    
    Ctn Engine Config Add Command    ${0}    agent_process_warning
    ...    {"check":"counter", "args":{ "counter": "\\\\System\\\\Processes", "warning-status":"value >=0","use_english":true} }
    ...    OTEL connector

    Ctn Engine Config Add Command    ${0}    agent_process_critical
    ...    {"check":"counter", "args":{ "counter": "\\\\System\\\\Processes", "critical-status":"value >=0","use_english":true} }
    ...    OTEL connector

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
    
    Log To Console    service_1 must be ok
    ${result}     Ctn Check Service Status With Timeout Rt    host_1    service_1    0    60    ANY
    Should Be True    ${result}    resources table not updated for service_1

    Log To Console    service_1 must be warning
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    agent_process_warning
    Ctn Reload Engine
    ${result}     Ctn Check Service Status With Timeout Rt    host_1    service_1    1    60    ANY
    Should Be True    ${result[0]}    resources table not updated for service_1


    Log To Console    service_1 must be critical
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    agent_process_critical
    Ctn Reload Engine

    ${result}     Ctn Check Service Status With Timeout Rt    host_1    service_1    2    60    ANY
    Should Be True    ${result[0]}    resources table not updated for service_1


BEOTEL_CENTREON_AGENT_CHECK_TASKSCHEDULER
    [Documentation]    Given an agent with task scheduler check, we expect to get the correct status for the centagent process running on windows host
    [Tags]    broker    engine    opentelemetry    MON-158584

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" != "WSL"    "This test is only for WSL"

    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"export_period":15}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    agent_tasksched_check
    Ctn Set Services Passive       0    service_1

    Ctn Engine Config Add Command    ${0}    agent_tasksched_check
    ...    {"check":"tasksched", "args":{ "filter-tasks": "name == 'TaskExit0'"} }
    ...    OTEL connector
    
    Ctn Engine Config Add Command    ${0}    agent_tasksched_warning
    ...    {"check":"tasksched", "args":{ "filter-tasks": "name == 'TaskExit1'","warning-status": "exit_code != 0"} }
    ...    OTEL connector

    Ctn Engine Config Add Command    ${0}    agent_tasksched_critical
    ...    {"check":"tasksched", "args":{ "filter-tasks": "name == 'TaskExit2'","critical-status": "exit_code != 0"} }
    ...    OTEL connector

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
    
    Log To Console    service_1 must be ok
    ${result}     Ctn Check Service Status With Timeout Rt    host_1    service_1    0    60    ANY
    Should Be True    ${result}    resources table not updated for service_1


    Log To Console    service_1 must be warning
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    agent_tasksched_warning
    Ctn Reload Engine
    ${result}     Ctn Check Service Status With Timeout Rt    host_1    service_1    1    60    ANY
    Should Be True    ${result[0]}    resources table not updated for service_1


    Log To Console    service_1 must be critical
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    agent_tasksched_critical
    Ctn Reload Engine

    ${result}     Ctn Check Service Status With Timeout Rt    host_1    service_1    2    60    ANY
    Should Be True    ${result[0]}    resources table not updated for service_1


BEOTEL_CENTREON_AGENT_CHECK_FILES
    [Documentation]    Given an agent with file check, we expect to get the correct status for files under monitoring on the Windows host
    [Tags]    broker    engine    opentelemetry    MON-155401

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" != "WSL"    "This test is only for WSL"

    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"export_period":15}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    agent_files_check_ok
    Ctn Set Services Passive       0    service_1

    Ctn Engine Config Add Command    ${0}    agent_files_check_ok
    ...    {"check":"files", "args":{ "path": "C:\\\\Windows","pattern": "*.dll","max-depth": 0} }
    ...    OTEL connector
    
    Ctn Engine Config Add Command    ${0}    agent_files_check_warning
    ...    {"check":"files", "args":{ "path": "C:\\\\Windows","pattern": "*.dll","max-depth": 0,"warning-status": "size > 1k"} }
    ...    OTEL connector

    Ctn Engine Config Add Command    ${0}    agent_files_check_critical
    ...    {"check":"files", "args":{ "path": "C:\\\\Windows","pattern": "*.dll","max-depth": 0,"critical-status": "size > 1k"} }
    ...    OTEL connector

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
    
    Log To Console    service_1 must be ok
    ${result}     Ctn Check Service Status With Timeout Rt    host_1    service_1    0    60    ANY
    Should Be True    ${result}    resources table not updated for service_1


    Log To Console    service_1 must be warning
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    agent_files_check_warning
    Ctn Reload Engine
    ${result}     Ctn Check Service Status With Timeout Rt    host_1    service_1    1    60    ANY
    Should Be True    ${result[0]}    resources table not updated for service_1


    Log To Console    service_1 must be critical
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    agent_files_check_critical
    Ctn Reload Engine

    ${result}     Ctn Check Service Status With Timeout Rt    host_1    service_1    2    60    ANY
    Should Be True    ${result[0]}    resources table not updated for service_1


BEOTEL_CENTREON_AGENT_TOKEN
    [Documentation]    Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled
    ...    When the Centreon Agent attempts to connect using an valid JWT token
    ...    Then the connection should be accepted
    ...    And the log should confirm that the token is valid
    [Tags]    broker    engine    opentelemetry    MON-160084

    Ctn Config Engine    ${1}    ${2}    ${2}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": true, "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    
    Ctn Engine Config Set Value    0    log_level_checks    trace

    ${token1}    Ctn Create Jwt Token    ${60}

    Ctn Add Token Otl Server Module    0    ${token1}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt    ${token1}
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent
    
    # Let's wait for the otel server start
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    
    #if the message apear mean that the connection is accepted
    ${content}    Create List    Token is valid
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "Token is valid" should appear.

BEOTEL_CENTREON_AGENT_NO_TRUSTED_TOKEN
    [Documentation]    Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled with no trusted_token
    ...    When the Centreon Agent attempts to connect with tls
    ...    Then the connection should be accepted
    [Tags]    broker    engine    opentelemetry    MON-170625

    Ctn Config Engine    ${1}    ${2}    ${5}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": true, "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0,"centreon_agent":{"export_period":10}}
    ...    False
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    
    # create a host with otel_check_icmp command
    Ctn Engine Config Replace Value In Services    ${0}    service_5    check_command    otel_check_icmp
    Ctn Set Services Passive    0    service_5
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_5    check_interval    1
    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}  otel_check_icmp   ${echo_command}    OTEL connector

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt
    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent
    
    # Let's wait for the otel server start
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    
    ${result}    ${content}     Ctn Check Service Resource Status With Timeout Rt    host_1    service_5    0    120    HARD
    Should Be True    ${result}    resources table not updated
    Should Contain    ${content}    OK - 127.0.0.1:

BEOTEL_CENTREON_AGENT_TOKEN_MISSING_HEADER
    [Documentation]    Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled
    ...    When the Centreon Agent attempts to connect without a JWT token
    ...    Then the connection should be refused
    ...    And the log should contain the message "UNAUTHENTICATED: No authorization header"
    [Tags]    broker    engine    opentelemetry    MON-160435
    
    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' != 'None'
        Pass Execution    Test passes, skipping on Windows
    END
    
    Ctn Config Engine    ${1}    ${2}    ${2}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": true, "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_interval    1

    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"

    Ctn Engine Config Add Command    ${0}  otel_check_icmp   ${echo_command}    OTEL connector

    ${token1}    Ctn Create Jwt Token    ${60}

    Ctn Add Token Otl Server Module    0    ${token1}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt
    Ctn Engine Config Set Value    0    log_level_checks    error
    Ctn Engine Config Set Value    0    log_level_functions    error
    Ctn Engine Config Set Value    0    log_level_config    error
    Ctn Engine Config Set Value    0    log_level_events    error

    Ctn Broker Config Log    module0    core    warning
    Ctn Broker Config Log    module0    processing    warning
    Ctn Broker Config Log    module0    neb    warning

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent
    
    # Let's wait for the otel server start
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    
    #if the message apear mean that the connection is accepted
    ${content}    Create List    UNAUTHENTICATED: No authorization header
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "UNAUTHENTICATED: No authorization header" should appear.

BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED
    [Documentation]    Given the OpenTelemetry server is configured with encryption enabled
...   And the server uses a public certificate and private key for secure communication
...   When the Centreon Agent attempts to connect using an invalid JWT token
...   Then the connection should be refused
...   And the log should contain the message "Token is not trusted"
    [Tags]    broker    engine    opentelemetry    MON-160084

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' != 'None'
        Pass Execution    Test passes, skipping on Windows
    END

    Ctn Config Engine    ${1}    ${2}    ${2}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": true, "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    
    Ctn Engine Config Set Value    0    log_level_checks    trace

    ${token1}    Ctn Create Jwt Token    ${60}
    ${token2}    Ctn Create Jwt Token    ${3600}

    Ctn Add Token Otl Server Module    0    ${token2}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt    ${token1}
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    
    #if the message apear mean that the connection is refused
    ${content}    Create List    Token is not trusted
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "Token is not trusted" should appear.


BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED
    [Documentation]    Given the OpenTelemetry server is configured with encryption enabled
...   And the server uses a public certificate and private key for secure communication
...   When the Centreon Agent attempts to connect using an expired JWT token
...   Then the connection should be refused
...   And the log should contain the message "Token is expired"
    [Tags]    broker    engine    opentelemetry    MON-160084

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' != 'None'
        Pass Execution    Test passes, skipping on Windows
    END

    Ctn Config Engine    ${1}    ${2}    ${2}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": true, "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    
    Ctn Engine Config Set Value    0    log_level_checks    trace

    ${token1}    Ctn Create Jwt Token    ${10}

    Ctn Add Token Otl Server Module    0    ${token1}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt    ${token1}
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Sleep    10s
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    
    ${content}    Create List    UNAUTHENTICATED : Token expired
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "UNAUTHENTICATED : Token expired" should appear.


BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING
    [Documentation]    Given the OpenTelemetry server is configured with encryption enabled
...   And the server uses a public certificate and private key for secure communication
...   When the Centreon Agent attempts to connect using an JWT token valid
...   Then the connection should be accepted
...   When the token expires
...   Then the connection should be refused
...   And the log should contain the message "Token is expired"
    [Tags]    broker    engine    opentelemetry    MON-160084

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' != 'None'
        Pass Execution    Test passes, skipping on Windows
    END

    Ctn Config Engine    ${1}    ${2}    ${2}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": true, "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0,"centreon_agent": {"check_interval": 10}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    
    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp    ${echo_command}    OTEL connector
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Set Hosts Passive    ${0}    host_1 

    Ctn Engine Config Set Value    0    log_level_checks    trace
    Ctn Engine Config Set Value    0    log_level_functions    warning
    

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd

    ${token}    Ctn Create Jwt Token    ${20}

    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt    ${token}
    Ctn Add Token Otl Server Module    0    ${token}   


    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    module0    core    warning
    Ctn Broker Config Log    module0    processing    warning
    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    
    # if message apear the connection is accepted
    ${content}    Create List    Token is valid
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "Token is valid" should appear.

    Sleep   30s

    ${content}    Create List    UNAUTHENTICATED : Token expired
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "UNAUTHENTICATED : Token expired" should appear.


BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH
    [Documentation]    Given an OpenTelemetry server is configured with token-based connection
...    And the Centreon Agent is configured with a valid token
...    When the agent attempts to connect to the server
...    Then the connection should be successful
...    And the log should confirm that the token is valid
...    And Telegraf should connect and send data to the engine
    [Tags]    broker    engine    opentelemetry    MON-160084

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' != 'None'
        Pass Execution    Test passes, skipping on Windows
    END

    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": true, "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0,"centreon_agent": {"check_interval": 10},"telegraf_conf_server": {"http_server":{"port": 1443, "encryption": true, "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"}, "check_interval":60, "engine_otel_endpoint": "127.0.0.1:4317"}}
    Ctn Config Add Otl Connector
    ...    0
    ...    CMA connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Config Add Otl Connector
    ...    0
    ...    TEL connector
    ...    opentelemetry --processor=nagios_telegraf --extractor=attributes --host_path=resource_metrics.scope_metrics.data.data_points.attributes.host --service_path=resource_metrics.scope_metrics.data.data_points.attributes.service

    
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    cma_check_icmp
    
    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    cma_check_icmp    ${echo_command}    CMA connector
    Ctn Set Hosts Passive    ${0}    host_1

    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp2
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.1
    ...    TEL connector
    Ctn Engine Config Replace Value In Hosts    ${0}    host_2    check_command    otel_check_icmp2
    Ctn Set Hosts Passive  ${0}  host_2

    Ctn Engine Config Set Value    0    log_level_checks    trace
    Ctn Engine Config Set Value    0    log_level_functions    warning
    

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd

    ${token}    Ctn Create Jwt Token    ${60}

    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt    ${token}
    Ctn Add Token Otl Server Module    0    ${token}   


    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    module0    core    warning
    Ctn Broker Config Log    module0    processing    warning
    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.

    # if message apear the agent is accepted
    ${content}    Create List    Token is valid
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "Token is valid" should appear.

    ${start}    Ctn Get Round Current Date
    ${resources_list}    Ctn Create Otl Request    ${0}    host_2

    ${host_name}  Ctn Get Hostname
    Ctn Send Otl To Engine Secure    ${host_name}:4318    ${resources_list}    /tmp/server_grpc.crt

    #if pass telegraph can connect to engine without token
    ${content}    Create List    receive:resource_metrics { scope_metrics { metrics { name: "check_icmp_state"
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "if message don't apper in log it mean that the message is not send to the engine"

BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH_2
    [Documentation]    Given an OpenTelemetry server is configured with token-based connection
...    And the Centreon Agent is configured with a valid token that will expire 
...    When the agent attempts to connect to the server
...    Then the connection should be successful
...    And the log should confirm that the token is valid
...    And Telegraf should connect and send data to the engine
    [Tags]    broker    engine    opentelemetry    MON-160084

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' != 'None'
        Pass Execution    Test passes, skipping on Windows
    END

    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": true, "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0,"telegraf_conf_server": {"http_server":{"port": 1443, "encryption": true, "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"}, "check_interval":60, "engine_otel_endpoint": "127.0.0.1:4317"}}
    Ctn Config Add Otl Connector
    ...    0
    ...    CMA connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Config Add Otl Connector
    ...    0
    ...    TEL connector
    ...    opentelemetry --processor=nagios_telegraf --extractor=attributes --host_path=resource_metrics.scope_metrics.data.data_points.attributes.host --service_path=resource_metrics.scope_metrics.data.data_points.attributes.service

    
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    cma_check_icmp
    
    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    cma_check_icmp    ${echo_command}    CMA connector
    Ctn Set Hosts Passive    ${0}    host_1
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    1
    Ctn Engine Config Set Value    0    interval_length    10

    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp2
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.1
    ...    TEL connector
    Ctn Engine Config Replace Value In Hosts    ${0}    host_2    check_command    otel_check_icmp2
    Ctn Set Hosts Passive  ${0}  host_2

    Ctn Engine Config Set Value    0    log_level_checks    trace
    Ctn Engine Config Set Value    0    log_level_functions    warning
    

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd

    ${token}    Ctn Create Jwt Token    ${15}

    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt    ${token}
    Ctn Add Token Otl Server Module    0    ${token}   


    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    module0    core    warning
    Ctn Broker Config Log    module0    processing    warning
    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.

    # if message apear the agent is accepted
    ${content}    Create List    Token is valid
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "Token is valid" should appear.

    ${start}    Ctn Get Round Current Date
    ${resources_list}    Ctn Create Otl Request    ${0}    host_2

    ${host_name}  Ctn Get Hostname
    Ctn Send Otl To Engine Secure    ${host_name}:4318    ${resources_list}    /tmp/server_grpc.crt

    #if pass telegraph can connect to engine without token
    ${content}    Create List    receive:resource_metrics { scope_metrics { metrics { name: "check_icmp_state"
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "if message don't apper in log it mean that the message is not send to the engine"
    
    sleep  10s
    ${start}    Get Current Date

    ${content}    Create List    UNAUTHENTICATED : Token expired
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "UNAUTHENTICATED : Token expired" should appear.

    Ctn Send Otl To Engine Secure    ${host_name}:4318    ${resources_list}    /tmp/server_grpc.crt

    #if pass telegraph can connect to engine without token
    ${content}    Create List    receive:resource_metrics { scope_metrics { metrics { name: "check_icmp_state"
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "if message don't apper in log it mean that the message is not send to the engine"


BEOTEL_CENTREON_AGENT_TOKEN_REVERSE
    [Documentation]    Given the Centreon Engine is configured as client with token and the agent as server with encryption enables
    ...    When the Centreon engine attempts to connect using an valid JWT token
    ...    Then the connection should be accepted
    ...    And the log should confirm that the token is valid
    [Tags]    broker    engine    opentelemetry    MON-160435

    Ctn Config Engine    ${1}    ${2}    ${2}

    ${host_host_name}      Ctn Host Hostname
    ${config_content}    Catenate    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port": 4321,"encryption": true, "ca_certificate": "/tmp/reverse_server_grpc.crt"}]}} 
    Ctn Add Otl ServerModule   0    ${config_content}
    
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
 
    Ctn Engine Config Set Value    0    log_level_checks    error
    Ctn Engine Config Set Value    0    log_level_functions    error
    Ctn Engine Config Set Value    0    log_level_config    error
    Ctn Engine Config Set Value    0    log_level_events    error

    ${token1}    Ctn Create Jwt Token    ${60}

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' == 'None'
        Ctn Add Token Agent Otl Server   0    0    ${token1}
    END


    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Reverse Centreon Agent   /tmp/reverse_server_grpc.key  /tmp/reverse_server_grpc.crt   ${None}    ${token1}

    Ctn Broker Config Log    module0    core    warning
    Ctn Broker Config Log    module0    processing    warning
    Ctn Broker Config Log    module0    neb    warning

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent
    
    ${content}    Create List    init from ${host_host_name}:4321
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "if message don't apper in log it mean that the connection is not accepted"

BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED_REVERSE
    [Documentation]    Given the Centreon Engine is configured as client with token and the agent as server with encryption enables
    ...    When the Centreon engine attempts to connect using an invalid JWT token
    ...    Then the connection should be refused
    ...    And the log should confirm that the token is not trusted
    [Tags]    broker    engine    opentelemetry    MON-160435

    Ctn Config Engine    ${1}    ${2}    ${2}

    ${host_host_name}      Ctn Host Hostname
    ${config_content}    Catenate    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port": 4321,"encryption": true, "ca_certificate": "/tmp/reverse_server_grpc.crt"}]}} 
    Ctn Add Otl ServerModule   0    ${config_content}
    
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
 
    Ctn Engine Config Set Value    0    log_level_checks    error
    Ctn Engine Config Set Value    0    log_level_functions    error
    Ctn Engine Config Set Value    0    log_level_config    error
    Ctn Engine Config Set Value    0    log_level_events    error

    ${token1}    Ctn Create Jwt Token    ${60}
    ${token2}    Ctn Create Jwt Token    ${60}

    Ctn Add Token Agent Otl Server    0    0    ${token1}


    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Reverse Centreon Agent   /tmp/reverse_server_grpc.key  /tmp/reverse_server_grpc.crt   ${None}    ${token2}

    Ctn Broker Config Log    module0    core    warning
    Ctn Broker Config Log    module0    processing    warning
    Ctn Broker Config Log    module0    neb    warning

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent
    
    ${content}    Create List    client::OnDone(Token not trusted)
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "ithe message should appear to confirm that the connection is refused"
    
BEOTEL_CENTREON_AGENT_TOKEN_EXPIRE_REVERSE
    [Documentation]    Given the Centreon Engine is configured as client with token and the agent as server with encryption enables
    ...    When the Centreon engine attempts to connect using an valid JWT token but expired
    ...    Then the connection should be refused
    ...    And the log should confirm that the token is expired
    [Tags]    broker    engine    opentelemetry    MON-160435

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' != 'None'
        Pass Execution    Test passes, skipping on Windows
    END

    Ctn Config Engine    ${1}    ${2}    ${2}

    ${host_host_name}      Ctn Host Hostname
    ${config_content}    Catenate    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port": 4321,"encryption": true, "ca_certificate": "/tmp/reverse_server_grpc.crt"}]}} 
    Ctn Add Otl ServerModule   0    ${config_content}
    
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
 
    Ctn Engine Config Set Value    0    log_level_checks    error
    Ctn Engine Config Set Value    0    log_level_functions    error
    Ctn Engine Config Set Value    0    log_level_config    error
    Ctn Engine Config Set Value    0    log_level_events    error

    ${token1}    Ctn Create Jwt Token    ${2}
    # make sure the token is expired
    Sleep    3s

    Ctn Add Token Agent Otl Server   0    0    ${token1}


    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Reverse Centreon Agent   /tmp/reverse_server_grpc.key  /tmp/reverse_server_grpc.crt   ${None}    ${token1}
    Ctn Broker Config Log    module0    core    warning
    Ctn Broker Config Log    module0    processing    warning
    Ctn Broker Config Log    module0    neb    warning

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent
    
    ${content}    Create List    client::OnDone(Token expired)
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "if message don't apper in log it mean that the connection is not accepted"


BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING_REVERSE
    [Documentation]    Given the Centreon Engine is configured as client with token and the agent as server with encryption enables
...   When the Centreon engine attempts to connect using an valid JWT token 
...   Then the connection should be accepted
...   When the token expires
...   Then the connection should be refused
...   And the log should contain the message "Token is expired"
    [Tags]    broker    engine    opentelemetry    MON-160435

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' != 'None'
        Pass Execution    Test passes, skipping on Windows
    END

    Ctn Config Engine    ${1}    ${2}    ${2}

    ${host_host_name}      Ctn Host Hostname
    ${config_content}    Catenate    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port": 4321,"encryption": true, "ca_certificate": "/tmp/reverse_server_grpc.crt"}]}} 
    Ctn Add Otl ServerModule   0    ${config_content}
    
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
 
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1 
    Ctn Engine Config Set Value    0    interval_length    2
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_interval    1

    ${echo_command}    Ctn Echo Command    "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp   ${echo_command}    OTEL connector

    ${token1}    Ctn Create Jwt Token    ${30}

    Ctn Add Token Agent Otl Server   0    0    ${token1}


    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Reverse Centreon Agent   /tmp/reverse_server_grpc.key  /tmp/reverse_server_grpc.crt   ${None}    ${token1}

    Ctn Broker Config Log    module0    core    warning
    Ctn Broker Config Log    module0    processing    warning
    Ctn Broker Config Log    module0    neb    warning

    Ctn Engine Config Set Value    0    log_level_checks    error
    Ctn Engine Config Set Value    0    log_level_functions    error
    Ctn Engine Config Set Value    0    log_level_config    error
    Ctn Engine Config Set Value    0    log_level_events    error

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent
    
    ${content}    Create List    init from ${host_host_name}:4321
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "if message don't apper in log it mean that the connection is not accepted"

    ${content}    Create List    client::OnDone(Token expired)
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "this message client::OnDone(Token expired) should apper in log"

BEOTEL_CENTREON_AGENT_WHITE_LIST
    [Documentation]    Scenario: Enforcing command whitelist for agent checks
    ...    Given a whitelist file is created with allowed commands for host_1
    ...    And the engine, broker, and agent are configured and started
    ...    When a check command matching the whitelist is executed for host_1
    ...    Then the check result is accepted and stored in the resources table
    ...    When a check command not matching the whitelist is configured for host_1 and engine is reloaded
    ...    Then the command is rejected and a "command not allowed by whitelist" message appears in the log
    [Tags]    broker    engine    opentelemetry    MON-173914

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' != 'None'
        Pass Execution    Test passes, skipping on Windows
    END
    Create Directory    /etc/centreon-engine-whitelist
    Empty Directory    /etc/centreon-engine-whitelist
    ${whitelist_content}    Catenate    {"cma-whitelist": {"default": {"wildcard": ["*"]}, "hosts": [{"hostname": "host_1", "wildcard": ["/bin/echo \\"OK - *"]}]}}
    Create File    /etc/centreon-engine-whitelist/test    ${whitelist_content}
    
    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0, "centreon_agent":{"export_period":10}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_interval    1

    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}  otel_check_icmp    ${echo_command}    OTEL connector

        ${echo_command}   Ctn Echo Command  "OK check2 - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command  ${0}    rejected_by_whitelist    ${echo_command}    OTEL connector



    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent

    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    module0    core    warning
    Ctn Broker Config Log    module0    processing    warning
    Ctn Broker Config Log    module0    neb    warning

    Ctn Engine Config Set Value    0    log_level_checks    trace
    Ctn Engine Config Set Value    0    log_level_functions    error
    Ctn Engine Config Set Value    0    log_level_config    error
    Ctn Engine Config Set Value    0    log_level_events    error

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

    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    rejected_by_whitelist
    


    #update conf engine, it must be taken into account by agent
    Log To Console    modify engine conf and reload engine
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    #wait for new data from agent
    ${content}    Create List    host_1: command not allowed by whitelist /bin/echo "OK check2 - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    command not allowed by whitelist.


*** Keywords ***
Ctn Create Cert And Init
    [Documentation]  create key and certificates used by agent and engine on linux side
    ${host_name}  Ctn Host Hostname
    ${run_env}       Ctn Run Env
    IF    "${run_env}" == "WSL"
        Copy File    ../server_grpc.key    /tmp/server_grpc.key
        Copy File    ../server_grpc.crt    /tmp/server_grpc.crt
        Copy File    ../reverse_server_grpc.crt    /tmp/reverse_server_grpc.crt
    ELSE
        Ctn Create Key And Certificate  ${host_name}  /tmp/server_grpc.key   /tmp/server_grpc.crt
        Copy File    /tmp/server_grpc.crt    /tmp/reverse_server_grpc.crt
        Copy File    /tmp/server_grpc.key    /tmp/reverse_server_grpc.key
    END

    Ctn Clean Before Suite

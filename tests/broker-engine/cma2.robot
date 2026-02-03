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
    # we use 3 services with different intervals and we expect to find these intervals in data_bin
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    health_check
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    1
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1
    Ctn Engine Config Replace Value In Services    ${0}    service_2    check_command    health_check
    Ctn Engine Config Replace Value In Services    ${0}    service_2    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_2    retry_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_3    check_command    health_check
    Ctn Engine Config Replace Value In Services    ${0}    service_3    check_interval    3
    Ctn Engine Config Replace Value In Services    ${0}    service_3    retry_interval    3
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
    Ctn Engine Config Set Value    0    interval_length    5
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1


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


BEOTEL_CENTREON_AGENT_CHECK_NATIVE_CUSTOM
    [Documentation]    Scenario: Native custom check echoes provided arguments
    ...    Given Centreon Engine is configured with an OpenTelemetry server module and OTEL connector
    ...    And service_1 is set as a passive service with defined check and retry intervals
    ...    And the check command "otel_check" runs the native custom check "check_echo" with ARG1 "user" and ARG2 "hello"
    ...    When Broker, Engine and Agent are started and the OTEL server becomes ready
    ...    Then service_1 must reach HARD OK state within 120 seconds
    ...    And the service output must be "hello user from custom check"
    [Tags]    broker    engine    opentelemetry    MON-190240

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
    Ctn Engine Config Set Value    0    interval_length    5
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1

    Ctn Engine Config Add Command    ${0}    otel_check   {"check": "custom", "args": { "name": "check_echo", "ARG1": "user", "ARG2": "hello"}}    OTEL connector

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

    ${result}     Ctn Check Service Resource Status With Timeout rt   host_1    service_1    0    120    HARD
    Should Be True    ${result}    resources table not updated
    Should Be Equal    ${result[1]}    hello user from custom check
    

BEOTEL_CENTREON_AGENT_CEIP
    [Documentation]    Scenario: Agent and "centreon_storage.agent_information" Statistics
    ...    Given Engine connected to Broker
    ...    When an agent connects to Engine
    ...	  Then a message is sent to Broker that results in a new row in the "centreon_storage.agent_information" table.

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
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0, "centreon_agent":{"export_period":10}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1 
    Ctn Engine Config Set Value    0    interval_length    5
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    2
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    retry_interval    1

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
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
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
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "A warning message should appear : NON TLS CONNECTION ARE ALLOWED FOR Agents // THIS IS NOT ALLOWED IN PRODUCTION.
    
    # check if the agent is in windows or not, to get the right log path
    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' == 'None'
        # not windows 
            ${content}    Create List    NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION
            ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
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
    ...    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port": 4321, "encryption": "full", "ca_certificate": "/tmp/reverse_server_grpc.crt"}]}}

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
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0}
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
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "A warning message should appear : NON TLS CONNECTION ARE ALLOWED FOR Agents // THIS IS NOT ALLOWED IN PRODUCTION.
    

    ${content}    Create List    NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
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
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "A warning message should appear : NON TLS CONNECTION ARE ALLOWED FOR Agents // THIS IS NOT ALLOWED IN PRODUCTION.
    
    ${content}    Create List    NON TLS CONNECTION CONFIGURED // THIS IS NOT ALLOWED IN PRODUCTION
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
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

    Ctn Broker Config Log    module0    core    warning
    Ctn Broker Config Log    module0    processing    warning
    Ctn Broker Config Log    module0    neb    warning

    Ctn Engine Config Set Value    0    log_level_checks    error
    Ctn Engine Config Set Value    0    log_level_functions    error
    Ctn Engine Config Set Value    0    log_level_config    error
    Ctn Engine Config Set Value    0    log_level_events    error

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
 
    ${result}    ${content}     Ctn Check Service Resource Status With Timeout RT    host_1    service_4    2    60    ANY
    Should Be True    ${result}    resources table not updated for service_4
    Should Be Equal As Strings    ${content}    unable to execute native check {"check": "health","args":{"warning-interval": "A", "critical-interval": "6"} } , output error : invalid warning/critical interval/runtime range


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
    Ctn Engine Config Set Value    0    interval_length    5
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1


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
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"export_period":15}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    agent_process_check
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1
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
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    agent_tasksched_check
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1
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
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    agent_files_check_ok
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    3
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    2
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
        [Documentation]    Scenario: Validate agent connection with valid JWT token
        ...    Given the OpenTelemetry server is configured with encryption enabled and the server trusts a JWT token
        ...    And the Centreon Agent is configured to use that valid token and the server certificate
        ...    When the broker, engine and agent are started
        ...    Then the engine should expose an encrypted OTEL listener
        ...    And the engine log should report that the token is valid
        ...    When the agent is reconfigured to use an unencrypted connection and the engine is reloaded
        ...    And the agent is restarted
        ...    Then the engine should expose an unencrypted OTEL listener
        ...    And the engine log should still report that the token is valid
    [Tags]    broker    engine    opentelemetry    MON-160084    MON-186322

    Ctn Config Engine    ${1}    ${2}    ${2}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    
    Ctn Engine Config Set Value    0    log_level_checks    trace

    ${token1}    Ctn Create Jwt Token    ${-1}

    Ctn Add Token Otl Server Module    0    ${token1}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt    ${token1}
    
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

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" == "WSL"    "the test ends here for WSL"

    Ctn Kindly Stop Agent

    ${otl_path}    Ctn Get Engine Conf Path   0
    ${agent_path}    Ctn Get Agent Conf Path

    Update Json Field    ${agent_path}    encryption    no
    Update Json Field    ${otl_path}/otl_server.json   otel_server.encryption    no

    Ctn Reload Engine
    Ctn Start Agent
    ${start}    Get Current Date

    # Let's wait for the otel server start
    ${content}    Create List    ] unencrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4318" should be available.

    #if the message apear mean that the connection is accepted
    ${content}    Create List    Token is valid
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "Token is valid" should appear.


BEOTEL_CENTREON_AGENT_TOKEN_MISSING_HEADER
    [Documentation]    Scenario: Missing JWT header over encrypted OTEL connection
    ...    Given Centreon Engine is configured with an encrypted OTEL server (port 4318) and a trusted token list
    ...    And Centreon Agent is configured to connect without a JWT token (no Authorization header)
    ...    When Broker, Engine and Agent are started
    ...    Then the encrypted OTEL server is listening
    ...    And the agent connection is rejected
    ...    And the engine log contains "UNAUTHENTICATED: No authorization header"
    ...    Scenario: Missing JWT header over unencrypted OTEL connection
    ...    Given Engine and Agent are reconfigured to use an unencrypted OTEL connection still without a JWT token
    ...    When the Engine is reloaded and the Agent restarted
    ...    Then the unencrypted OTEL server is listening
    ...    And the agent connection is rejected
    ...    And the engine log contains "UNAUTHENTICATED: No authorization header"
    [Tags]    broker    engine    opentelemetry    MON-160435    MON-186322
    
    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' != 'None'
        Pass Execution    Test passes, skipping on Windows
    END

    Ctn Config Engine    ${1}    ${2}    ${2}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1
    Ctn Engine Config Set Value    0    interval_length    10

    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"

    Ctn Engine Config Add Command    ${0}  otel_check_icmp   ${echo_command}    OTEL connector

    ${token1}    Ctn Create Jwt Token    ${60}

    Ctn Add Token Otl Server Module    0    ${token1}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt    ${None}    ${None}    full    ${False}
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
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent
    
    # Let's wait for the otel server start
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    
    #if the message apear mean that the connection is refused
    ${content}    Create List    UNAUTHENTICATED: No authorization header
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "UNAUTHENTICATED: No authorization header" should appear.

    Ctn Kindly Stop Agent

    ${otl_path}    Ctn Get Engine Conf Path   0
    ${agent_path}    Ctn Get Agent Conf Path

    Update Json Field    ${agent_path}    encryption    no
    Update Json Field    ${otl_path}/otl_server.json   otel_server.encryption    no

    Ctn Reload Engine
    Ctn Start Agent
    ${start}    Get Current Date

    # Let's wait for the otel server start
    ${content}    Create List    ] unencrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4318" should be available.

    #if the message apear mean that the connection is refused
    ${content}    Create List    UNAUTHENTICATED: No authorization header
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "UNAUTHENTICATED: No authorization header" should appear.
    
BEOTEL_CENTREON_AGENT_NO_TRUSTED_TOKEN
    [Documentation]    Scenario: Encrypted connection rejected with untrusted token
    ...    Given Centreon Engine is configured with an encrypted OpenTelemetry server on port 4318 and an empty trusted token list
    ...    And host_1 is configured with a passive check using otel_check_icmp
    ...    And Centreon Agent is configured with a JWT token and the server certificate
    ...    When Broker, Engine and Agent are started
    ...    Then the encrypted OTEL server starts listening
    ...    And the agent connection is refused
    ...    And host_1 resource status is not updated in the resources table
    ...    And the engine log contains 'UNAUTHENTICATED : Token is not trusted'
    ...    Scenario: Unencrypted connection rejected with untrusted token
    ...    When the Engine is reloaded and the Agent restarted
    ...    Then the unencrypted OTEL server starts listening
    ...    And the agent connection is refused
    ...    And host_1 resource status is not updated in the resources table
    ...    And the engine log contains 'UNAUTHENTICATED : Token is not trusted'
    [Tags]    broker    engine    opentelemetry    MON-170625    MON-186322

    Ctn Config Engine    ${1}    ${2}    ${2}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0,"centreon_agent":{"export_period":10}}
    ...    False
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    
    # create a host with otel_check_icmp command
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    2
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    retry_interval    1
    Ctn Engine Config Set Value    0    interval_length    5
    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}  otel_check_icmp   ${echo_command}    OTEL connector

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd

    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    module0    core    warning
    Ctn Broker Config Log    module0    processing    warning
    Ctn Broker Config Log    module0    neb    warning

    Ctn Engine Config Set Value    0    log_level_checks    trace
    Ctn Engine Config Set Value    0    log_level_functions    error
    Ctn Engine Config Set Value    0    log_level_config    error
    Ctn Engine Config Set Value    0    log_level_events    error

    ${token1}    Ctn Create Jwt Token    ${-1}

    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt    ${token1}
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
    
    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    15    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Not Be True    ${result}    resources table should not be updated for host_1

    ${content}    Create List    UNAUTHENTICATED : Token is not trusted
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "UNAUTHENTICATED : Token is not trusted" should appear.

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" == "WSL"    "the test ends here for WSL"

    Ctn Kindly Stop Agent

    ${otl_path}    Ctn Get Engine Conf Path   0
    ${agent_path}    Ctn Get Agent Conf Path

    Update Json Field    ${agent_path}    encryption    no
    Update Json Field    ${otl_path}/otl_server.json   otel_server.encryption    no

    Ctn Reload Engine
    Ctn Start Agent
    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date

    # Let's wait for the otel server start
    ${content}    Create List    ] unencrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4318" should be available.

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    15    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Not Be True    ${result}    resources table should not be updated for host_1

    ${content}    Create List    UNAUTHENTICATED : Token is not trusted
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "UNAUTHENTICATED : Token is not trusted" should appear.


BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED
    [Documentation]    Scenario: Encrypted connection with untrusted token
    ...    Given Centreon Engine is configured with an encrypted OpenTelemetry server
    ...    And the server trusts a JWT token different from the one configured on the Centreon Agent
    ...    When Broker, Engine and Agent are started
    ...    Then the encrypted OTEL server starts listening
    ...    And the agent connection is refused
    ...    And the engine log contains "Token is not trusted"
    ...    Scenario: Unencrypted connection with untrusted token
    ...    Given Engine and Agent are reconfigured to use an unencrypted OTEL connection with the same untrusted agent token
    ...    When the Engine is reloaded and the Agent restarted
    ...    Then the unencrypted OTEL server starts listening
    ...    And the agent connection is refused
    ...    And the engine log contains "Token is not trusted"
    [Tags]    broker    engine    opentelemetry    MON-160084    MON-186322

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' != 'None'
        Pass Execution    Test passes, skipping on Windows
    END

    Ctn Config Engine    ${1}    ${2}    ${2}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0}
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

    Ctn Kindly Stop Agent

    ${otl_path}    Ctn Get Engine Conf Path   0
    ${agent_path}    Ctn Get Agent Conf Path

    Update Json Field    ${agent_path}    encryption    no
    Update Json Field    ${otl_path}/otl_server.json   otel_server.encryption    no

    Ctn Reload Engine
    Ctn Start Agent
    ${start}    Get Current Date

    # Let's wait for the otel server start
    ${content}    Create List    ] unencrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4318" should be available.

    #if the message apear mean that the connection is refused
    ${content}    Create List    Token is not trusted
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "Token is not trusted" should appear.


BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED
    [Documentation]    Scenario: Encrypted connection with expired token
...    Given Centreon Engine is configured with an encrypted OpenTelemetry server on port 4318 
...    And a JWT token with 1 second validity is created and added to the trusted token list
...    And Centreon Agent is configured to use that token
...    When the test waits long enough for the token to expire before starting Broker, Engine and Agent
...    Then the encrypted OTEL server starts listening
...    And the agent connection is refused
...    And the engine log contains "UNAUTHENTICATED : Token expired"
...    Scenario: Unencrypted connection with expired token
...    Given Engine and Agent are reconfigured to use an unencrypted OTEL server still with the expired token
...    When the Engine is reloaded and the Agent restarted
...    Then the unencrypted OTEL server starts listening
...    And the agent connection is refused
...    And the engine log contains "UNAUTHENTICATED : Token expired"
    [Tags]    broker    engine    opentelemetry    MON-160084    MON-186322

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' != 'None'
        Pass Execution    Test passes, skipping on Windows
    END

    Ctn Config Engine    ${1}    ${2}    ${2}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    
    Ctn Engine Config Set Value    0    log_level_checks    trace

    ${token1}    Ctn Create Jwt Token    ${1}

    Ctn Add Token Otl Server Module    0    ${token1}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt    ${token1}
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    Sleep    2s
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

    Ctn Kindly Stop Agent

    ${otl_path}    Ctn Get Engine Conf Path   0
    ${agent_path}    Ctn Get Agent Conf Path

    Update Json Field    ${agent_path}    encryption    no
    Update Json Field    ${otl_path}/otl_server.json   otel_server.encryption    no

    Ctn Reload Engine
    Ctn Start Agent
    ${start}    Get Current Date

    # Let's wait for the otel server start
    ${content}    Create List    ] unencrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4318" should be available.

    ${content}    Create List    UNAUTHENTICATED : Token expired
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "UNAUTHENTICATED : Token expired" should appear.

BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING
    [Documentation]    Scenario: Token expires during encrypted connection
    ...    Given Centreon Engine is configured with an encrypted OTEL server on port 4318
    ...    And Centreon Agent is configured with a valid JWT token (20s lifetime) and the server certificate
    ...    When Broker, Engine and Agent are started
    ...    Then the engine log reports the encrypted server is listening
    ...    And the engine log reports "Token is valid"
    ...    When the token lifetime elapses
    ...    Then the engine log reports "UNAUTHENTICATED : Token expired"
    ...    Scenario: Token expires during unencrypted connection after reconfiguration
    ...    Given a new valid JWT token (20s lifetime) is created and added to the OTEL server
    ...    And Engine and Agent are reconfigured to use an unencrypted OTEL server with the new token
    ...    When the Engine is reloaded and the Agent restarted
    ...    Then the engine log reports the unencrypted server is listening
    ...    And the engine log reports "Token is valid"
    ...    When the token lifetime elapses
    ...    Then the engine log reports "UNAUTHENTICATED : Token expired"
    [Tags]    broker    engine    opentelemetry    MON-160084    MON-186322

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' != 'None'
        Pass Execution    Test passes, skipping on Windows
    END

    Ctn Config Engine    ${1}    ${2}    ${2}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0,"centreon_agent": {"check_interval": 10}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    
    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp    ${echo_command}    OTEL connector
    Ctn Set Hosts Passive    ${0}    host_1 
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    2
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    retry_interval    1
    Ctn Engine Config Set Value    0    interval_length    5

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

    ${content}    Create List    UNAUTHENTICATED : Token expired
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "UNAUTHENTICATED : Token expired" should appear.

    Ctn Kindly Stop Agent
    
    ${token1}    Ctn Create Jwt Token    ${20}
    Ctn Add Token Otl Server Module    0    ${token1} 

    ${otl_path}    Ctn Get Engine Conf Path   0
    ${agent_path}    Ctn Get Agent Conf Path

    Update Json Field    ${agent_path}    encryption    no
    Update Json Field    ${agent_path}    token    ${token1} 
    Update Json Field    ${otl_path}/otl_server.json   otel_server.encryption    no

    Ctn Reload Engine
    Ctn Start Agent
    ${start}    Get Current Date

    # Let's wait for the otel server start
    ${content}    Create List    ] unencrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4318" should be available.

    # if message apear the connection is accepted
    ${content}    Create List    Token is valid
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "Token is valid" should appear.

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
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0,"centreon_agent": {"check_interval": 10},"telegraf_conf_server": {"http_server":{"port": 1443, "encryption": true, "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"}, "check_interval":60, "engine_otel_endpoint": "127.0.0.1:4317"}}
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
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    2
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    retry_interval    1
    Ctn Engine Config Set Value    0    interval_length    5

    Ctn Engine Config Add Command
    ...    ${0}
    ...    otel_check_icmp2
    ...    /usr/lib/nagios/plugins/check_icmp 127.0.0.1
    ...    TEL connector
    Ctn Engine Config Replace Value In Hosts    ${0}    host_2    check_command    otel_check_icmp2
    Ctn Set Hosts Passive  ${0}  host_2
    Ctn Engine Config Set Value In Hosts    ${0}    host_2    check_interval    2
    Ctn Engine Config Set Value In Hosts    ${0}    host_2    retry_interval    1

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
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"},"max_length_grpc_log":0,"telegraf_conf_server": {"http_server":{"port": 1443, "encryption": true, "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"}, "check_interval":60, "engine_otel_endpoint": "127.0.0.1:4317"}}
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
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    2
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    retry_interval    1
    Ctn Engine Config Set Value    0    interval_length    5

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
    [Documentation]    Scenario: Encrypted reversed connection with valid token
    ...    Given Centreon Engine is configured as a client with a reverse encrypted connection to the Agent server on port 4321 and the Agent trusts a valid JWT token
    ...    When Broker, Engine and Agent are started
    ...    Then the Agent starts an encrypted reversed OTEL server
    ...    And the Engine connects to the Agent (init log appears)
    ...    And the Agent log reports "Token is valid"
    ...    Scenario: Unencrypted reversed connection with valid token after reconfiguration
    ...    Given encryption is disabled for the reverse connection while keeping the same trusted JWT token
    ...    When the Agent is stopped, the Engine configuration is reloaded, and the Agent is restarted
    ...    Then the Agent starts an unencrypted reversed OTEL server
    ...    And the Engine reconnects to the Agent (init log appears)
    ...    And the Agent log reports "Token is valid"
    [Tags]    broker    engine    opentelemetry    MON-160435    MON-186322

    Ctn Config Engine    ${1}    ${2}    ${2}

    ${host_host_name}      Ctn Host Hostname
    ${config_content}    Catenate    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port": 4321,"encryption": "full", "ca_certificate": "/tmp/reverse_server_grpc.crt"}]}} 
    Ctn Add Otl ServerModule   0    ${config_content}
    
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
 
    Ctn Engine Config Set Value    0    log_level_checks    error
    Ctn Engine Config Set Value    0    log_level_functions    error
    Ctn Engine Config Set Value    0    log_level_config    error
    Ctn Engine Config Set Value    0    log_level_events    error

    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    1

    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"

    Ctn Engine Config Add Command    ${0}  otel_check_icmp   ${echo_command}    OTEL connector

    ${token1}    Ctn Create Jwt Token    ${-1}

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

    # if the resource status is OK means the connection is accepted
    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    60    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" == "WSL"    "the test ends here for WSL"

    # if message apear the connection is accepted
    ${content}    Create List    Token is valid
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
    Should Be True    ${result}    "Token is valid" should appear.

    Ctn Kindly Stop Agent

    ${otl_path}    Ctn Get Engine Conf Path   0
    ${agent_path}    Ctn Get Agent Conf Path

    Update Json Field    ${agent_path}    encryption    no
    Update Json Field    ${otl_path}/otl_server.json   centreon_agent.reverse_connections.0.encryption    no

    Ctn Reload Engine
    Ctn Start Agent
    ${start}    Get Current Date

    # Let's wait for the otel server start
    ${content}    Create List    init from ${host_host_name}:4321
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "if message don't apper in log it mean that the connection is not accepted"

    # if message apear the connection is accepted
    ${content}    Create List    ] unencrypted server listening
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
    Should Be True    ${result}    "] unencrypted server listening " should appear.

    # if message apear the connection is accepted
    ${content}    Create List    Token is valid
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
    Should Be True    ${result}    "Token is valid" should appear.

BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED_REVERSE
    [Documentation]    Scenario: Encrypted reversed connection refused with untrusted token
    ...    Given Centreon Engine is configured as a client using a reverse encrypted OTEL connection to the Agent on port 4321
    ...    And the Agent reverse OTEL server trusts token1
    ...    And the Engine is configured to connect with a different (untrusted) token2
    ...    When Broker, Engine and Agent are started
    ...    Then the Agent starts its encrypted reversed server (listening is logged)
    ...    And the Engine connection is refused
    ...    And the engine log contains "client::OnDone(Token not trusted)"
    ...    Scenario: Unencrypted reversed connection refused with untrusted token after reconfiguration
    ...    Given encryption is disabled for the reverse connection while the Agent still trusts token1 and the Engine still uses token2
    ...    When the Agent is stopped, the Engine configuration reloaded, and the Agent restarted
    ...    Then the Agent starts its unencrypted reversed server (listening is logged)
    ...    And the Engine connection is refused
    ...    And the engine log contains "client::OnDone(Token not trusted)"
    [Tags]    broker    engine    opentelemetry    MON-160435    MON-186322

    Ctn Config Engine    ${1}    ${2}    ${2}

    ${host_host_name}      Ctn Host Hostname
    ${config_content}    Catenate    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port": 4321,"encryption": "full", "ca_certificate": "/tmp/reverse_server_grpc.crt"}]}} 
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

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" == "WSL"    "the test ends here for WSL"

    Ctn Kindly Stop Agent

    ${otl_path}    Ctn Get Engine Conf Path   0
    ${agent_path}    Ctn Get Agent Conf Path

    Update Json Field    ${agent_path}    encryption    no
    Update Json Field    ${otl_path}/otl_server.json   centreon_agent.reverse_connections.0.encryption    no

    Ctn Reload Engine
    Ctn Start Agent
    ${start}    Get Current Date

    # if message apear the connection is accepted
    ${content}    Create List    ] unencrypted server listening
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
    Should Be True    ${result}    "] unencrypted server listening " should appear.

    ${content}    Create List    client::OnDone(Token not trusted)
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "ithe message should appear to confirm that the connection is refused"

    
BEOTEL_CENTREON_AGENT_TOKEN_EXPIRE_REVERSE
    [Documentation]    Scenario: Encrypted reversed connection with expired token
    ...    Given Centreon Engine is configured as a client using a reversed encrypted OTEL connection to the Agent on port 4321
    ...    And the Agent reverse OTEL server trusts a JWT token that is already expired
    ...    When Broker, Engine and Agent are started
    ...    Then the Agent starts its encrypted reversed OTEL server (server listening appears in the agent log)
    ...    And the Engine connection is refused because the token is expired client::OnDone(Token expired appears in engine log)
    ...    Scenario: Unencrypted reversed connection with expired token
    ...    Given encryption is disabled for the reverse connection while still using the same expired token
    ...    When the Agent is stopped, the Engine configuration is reloaded, and the Agent is restarted
    ...    Then the Agent starts its unencrypted reversed OTEL server (server listening appears in the agent log)
    ...    And the Engine connection is refused because the token is expired client::OnDone(Token expired appears in engine log)
    [Tags]    broker    engine    opentelemetry    MON-160435    MON-186322

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' != 'None'
        Pass Execution    Test passes, skipping on Windows
    END

    Ctn Config Engine    ${1}    ${2}    ${2}

    ${host_host_name}      Ctn Host Hostname
    ${config_content}    Catenate    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port": 4321,"encryption": "full", "ca_certificate": "/tmp/reverse_server_grpc.crt"}]}} 
    Ctn Add Otl ServerModule   0    ${config_content}
    
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
 
    Ctn Engine Config Set Value    0    log_level_checks    error
    Ctn Engine Config Set Value    0    log_level_functions    error
    Ctn Engine Config Set Value    0    log_level_config    error
    Ctn Engine Config Set Value    0    log_level_events    error

    ${token1}    Ctn Create Jwt Token    ${1}
    # make sure the token is expired
    Sleep    2s

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

    # if message apear the connection is accepted
    ${content}    Create List    ] encrypted server listening
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
    Should Be True    ${result}    "] encrypted server listening" should appear.
    
    ${content}    Create List    client::OnDone(Token expired)
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "if message don't appear in log it mean that the connection is not accepted"

    Ctn Kindly Stop Agent

    ${otl_path}    Ctn Get Engine Conf Path   0
    ${agent_path}    Ctn Get Agent Conf Path

    Update Json Field    ${agent_path}    encryption    no
    Update Json Field    ${otl_path}/otl_server.json   centreon_agent.reverse_connections.0.encryption    no

    Ctn Reload Engine
    Ctn Start Agent
    ${start}    Get Current Date

    # if message apear the connection is accepted
    ${content}    Create List    ] unencrypted server listening
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
    Should Be True    ${result}    "] unencrypted server listening " should appear.

    ${content}    Create List    client::OnDone(Token expired)
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "if message don't appear in log it mean that the connection is not accepted"


BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING_REVERSE
    [Documentation]    Scenario: Token expires during encrypted reverse connection
    ...    Given Centreon Engine is configured to initiate a reverse encrypted OTEL connection to the Agent using a valid JWT token (10s lifetime)
    ...    When Broker, Engine and Agent are started
    ...    Then the Agent starts its encrypted reverse OTEL server and the Engine connects (init log appears)
    ...    And the Agent log reports "Token is valid"
    ...    When the token lifetime elapses
    ...    Then the Engine log reports "client::OnDone(Token expired)"
    ...    Scenario: Token expires during unencrypted reverse connection after reconfiguration
    ...    Given the Engine and Agent are reconfigured to use an unencrypted reverse OTEL connection with a new valid JWT token (10s lifetime)
    ...    When the Engine configuration is reloaded and the Agent restarted
    ...    Then the Agent starts its unencrypted reverse OTEL server and the Engine connects (init log appears)
    ...    And the Agent log reports "Token is valid"
    ...    When the new token lifetime elapses
    ...    Then the Engine log reports "client::OnDone(Token expired)"
    [Tags]    broker    engine    opentelemetry    MON-160435    MON-186322

    ${cur_dir}    Ctn Workspace Win
    IF    '${cur_dir}' != 'None'
        Pass Execution    Test passes, skipping on Windows
    END

    Ctn Config Engine    ${1}    ${2}    ${2}

    ${host_host_name}      Ctn Host Hostname
    ${config_content}    Catenate    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port": 4321,"encryption": "full", "ca_certificate": "/tmp/reverse_server_grpc.crt"}]}} 
    Ctn Add Otl ServerModule   0    ${config_content}
    
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
 
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1 
    Ctn Engine Config Set Value    0    interval_length    2
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    1

    ${echo_command}    Ctn Echo Command    "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp   ${echo_command}    OTEL connector

    ${token1}    Ctn Create Jwt Token    ${10}

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
    
    # if message apear the connection is accepted
    ${content}    Create List    ] encrypted server listening
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
    Should Be True    ${result}    "] encrypted server listening" should appear.

    ${content}    Create List    init from ${host_host_name}:4321
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "if message don't apper in log it mean that the connection is not accepted"

    # if message apear the connection is accepted
    ${content}    Create List    Token is valid
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
    Should Be True    ${result}    "Token is valid" should appear.

    ${content}    Create List    client::OnDone(Token expired)
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "this message client::OnDone(Token expired) should apper in log"

    Ctn Kindly Stop Agent

    ${token2}    Ctn Create Jwt Token    ${10}

    ${otl_path}    Ctn Get Engine Conf Path   0
    ${agent_path}    Ctn Get Agent Conf Path

    Update Json Field    ${agent_path}    encryption    no
    Update Json Field    ${agent_path}    token    ${token2}
    Update Json Field    ${otl_path}/otl_server.json   centreon_agent.reverse_connections.0.encryption    no
    Update Json Field    ${otl_path}/otl_server.json   centreon_agent.reverse_connections.0.token    ${token2}

    Ctn Reload Engine
    Ctn Start Agent
    ${start}    Get Current Date

    # if message apear the connection is accepted
    ${content}    Create List    ] unencrypted server listening
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
    Should Be True    ${result}    "] unencrypted server listening " should appear.

    ${content}    Create List    init from ${host_host_name}:4321
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "if message don't apper in log it mean that the connection is not accepted"

    # if message apear the connection is accepted
    ${content}    Create List    Token is valid
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=True
    Should Be True    ${result}    "Token is valid" should appear.

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
    Ctn Engine Config Set Value    0    interval_length    5
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    2
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    retry_interval    1

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

    # Force delete directory and all its contents
    Remove Directory    /etc/centreon-engine-whitelist    recursive=True

BEOTEL_CENTREON_AGENT_FORCE_CHECK
    [Documentation]    Given an agent host with 20 services checked by centagent, we force check to 10nth service and then to the host and 
    ...    we expect correct status in bdd
    [Tags]    broker    engine    opentelemetry    MON-176090
    Ctn Config Engine    ${1}    ${1}    ${20}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0, "centreon_agent":{"export_period":10}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1

    FOR   ${i}    IN RANGE    1    21
        Ctn Engine Config Replace Value In Services    ${0}    service_${i}    check_command    otel_check_icmp
        Ctn Set Services Passive    ${0}    service_${i}
    END


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
    Ctn Clear Db    resources

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    Ctn Wait For Otel Server To Be Ready    ${start}
    Sleep    1s

    ${result}    Ctn Check Service Output Resource Status With Timeout    host_1    service_20    120    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated

    ${start}    Get Current Date
    Ctn Schedule Forced Service Check    host_1    service_10

    ${result}    Ctn Check Service Output Resource Status With Timeout    host_1    service_10    30    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    Resources table was not updated for host_1/service_10 after forced service check

    ${start}    Get Current Date
    Ctn Schedule Forced Host Check    host_1

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    30    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated


BEOTEL_CENTREON_AGENT_TLS_BAD_CERT
    [Documentation]    Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled 
    ...    When the Centreon Agent connects using a certificate with a mismatched hostname and the full option enabled
    ...    Then the connection is refused
    ...    And the engine log confirms the certificate hostname mismatch
    [Tags]    broker    engine    opentelemetry    MON-159813    Only_linux
    
    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" == "WSL"    "This test is only for linux agent version"

    Ctn Config Engine    ${1}    ${2}    ${2}
    ${host_host_name}      Ctn Host Hostname
    Ctn Create Key And Certificate  "server.local"  /tmp/server_grpc1.key   /tmp/server_grpc1.crt

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc1.crt", "private_key": "/tmp/server_grpc1.key"},"max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    
    Ctn Engine Config Set Value    0    log_level_checks    trace

    ${token1}    Ctn Create Jwt Token    ${-1}

    Ctn Add Token Otl Server Module    0    ${token1}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc1.crt    ${token1}    ${None}
    
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

    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.

    #connection is not accepted because the hostname used by agent is not in certificate
    ${content}    Create List    Peer name ${host_host_name} is not in peer certificate
    ${result}    Ctn Find Regex In Log With Timeout    ${agentlog}    ${start}    ${content}    20    True
    Should Be True    ${result}     this message should appear : Peer name ${host_host_name} is not in peer certificate

BEOTEL_CENTREON_AGENT_INSECURE
    [Documentation]    Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled 
    ...    When the Centreon Agent connects using a certificate with a mismatched hostname and the insecure option enabled
    ...    Then the connection succeeds
    ...    And the engine log confirms the token was accepted
    [Tags]    broker    engine    opentelemetry    MON-159813    Only_linux
    
    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" == "WSL"    "This test is only for linux agent version"

    Ctn Config Engine    ${1}    ${2}    ${2}
    Ctn Create Key And Certificate  server.local  /tmp/server_grpc1.key   /tmp/server_grpc1.crt

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc1.crt", "private_key": "/tmp/server_grpc1.key"},"max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    
    Ctn Engine Config Set Value    0    log_level_checks    trace

    ${token1}    Ctn Create Jwt Token    ${-1}

    Ctn Add Token Otl Server Module    0    ${token1}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc1.crt    ${token1}    server.local    insecure
    
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

BEOTEL_CENTREON_AGENT_CMD_DATABASE
    [Documentation]    Given an Engine configured 
...    And service_1 is configured as passive and set to use the check command "otel_check"
...    And the "otel_check" command payload is (corresponding to check id 456)
...    When the Engine, Broker and Agent are started and the OTEL server is ready
...    Then the command line stored in the database for host_1/service_1 must exactly match
    [Tags]    broker    engine    opentelemetry    MON-183988
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
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1
    Ctn Engine Config Replace Value In Services    ${0}    service_1    max_check_attempts    4

    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive  ${0}  host_1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    1

    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"

    Ctn Engine Config Add Command    ${0}  otel_check_icmp   ${echo_command}    OTEL connector

    ${check_cmd}  Ctn Check Pl Command   --id 456

    Ctn Engine Config Add Command    ${0}    otel_check   ${check_cmd}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    #service_1 check fail CRITICAL
    Ctn Set Command Status    456    ${0}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Db    resources
    Ctn Clear Retention

    Ctn Broker Config Log    module0    core    warning
    Ctn Broker Config Log    module0    processing    warning
    Ctn Broker Config Log    module0    neb    warning
    Ctn Engine Config Set Value    0    log_level_checks    error
    Ctn Engine Config Set Value    0    log_level_functions    error
    Ctn Engine Config Set Value    0    log_level_config    error
    Ctn Engine Config Set Value    0    log_level_events    error

    ${start}    Ctn Get Round Current Date
    ${start_int}    Ctn Get Round Current Date

    Ctn Start Engine
    Ctn Start Broker
    Ctn Start Agent

    # Let's wait for the otel server start
    Ctn Wait For Otel Server To Be Ready    ${start}

    # check in the db that the command line is stored correctly
    ${result}    Ctn Check Commandline Service With Timeout Rt    host_1    service_1    120   ${check_cmd}
    Should Be True    ${result}    command line not found in db

    ${result}    Ctn Check Commandline Host With Timeout Rt    host_1    120   ${echo_command}
    Should Be True    ${result}    command line not found in db



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

*** Variables ***
${Salt}        U2FsdA==
${AppSecret}   SGVsbG8gd29ybGQsIGRvZywgY2F0LCBwdXBwaWVzLgo=

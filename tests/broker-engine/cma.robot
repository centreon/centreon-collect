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
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    1

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
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    #wait for new data from agent
    ${content}    Create List    \"output\":\"OK check2
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "output":"OK check2 should be available.

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    60    ${start_int}    0  HARD  OK check2 - 127.0.0.1: rta 0,010ms, lost 0%
    Should Be True    ${result}    resources table not updated


BEOTEL_CENTREON_AGENT_CHECK_HOST_NO_ENCRYPTED_CREDENTIALS
    [Documentation]    Given an agent host checked by centagent over a non encrypted connection,
    ...    Engine use credentials encryption, but send no encrypted commands
    ...    we set a first output to check command, 
    ...    modify it, reload engine and expect the new output in resource table
    [Tags]    broker    engine    opentelemetry    MON-158789
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

    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    1
    Ctn Engine Config Add Value    0    credentials_encryption    1

    
    Create File    /etc/centreon-engine/engine-context.json   {"app_secret":"${AppSecret}","salt":"${Salt}"}

    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"

    Ctn Engine Config Add Command    ${0}  otel_check_icmp   ${echo_command}    OTEL connector

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent
    Ctn Broker Config Log    central    sql    trace

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

    Sleep    1s
    Remove File    /etc/centreon-engine/engine-context.json

    #lets wait engine not send keys to CMA
    ${content}    Create List    As connection is not encrypted, Engine will send no encrypted credentials to agent
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "As connection is not encrypted, Engine will send no encrypted credentials to agent" should be available.

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    60    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated

    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp_2
    
    ${echo_command}   Ctn Echo Command  "OK check2 - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command  ${0}    otel_check_icmp_2  ${echo_command}    OTEL connector

    #update conf engine, it must be taken into account by agent
    Log To Console    modify engine conf and reload engine
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    #wait for new data from agent
    ${content}    Create List    \"output\":\"OK check2
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "output":"OK check2 should be available.

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    60    ${start_int}    0  HARD  OK check2 - 127.0.0.1: rta 0,010ms, lost 0%
    Should Be True    ${result}    resources table not updated

BEOTEL_CENTREON_AGENT_CHECK_SERVICE
    [Documentation]    Given an Engine configured with an OpenTelemetry server module 
...    And the check command  return CRITICAL
...    When the broker, engine and agent are started and repeated/forced checks are scheduled for service_1
...    Then the service must enter SOFT states on successive retry attempts
...    And there must be exactly three distinct SOFT attempts (three different last_check timestamps, each strictly increasing)
...    Then after retries are exhausted the service must transition to a HARD CRITICAL state with output "Test check 456"
...    When the check command 456 is changed to return OK and a forced check is scheduled
...    Then the service must transition to a HARD OK state with output "Test check 456"
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
    Ctn Engine Config Set Value    0    interval_length    60
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1
    Ctn Engine Config Replace Value In Services    ${0}    service_1    max_check_attempts    4

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
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    Ctn Wait For Otel Server To Be Ready    ${start}
    
    ${i}    Set Variable    1
    ${prev_last_check}    Set Variable    None
    ${tries}    Set Variable    0
    ${last_checks}    Create List

    # Let's wait for service to be in SOFT state 3 times with different last_check timestamp 
    WHILE    ${i} < 4
        ${res}    ${content}    Ctn Check Service Output Resource Status With Timeout RT    host_1    service_1    60    ${start_int}    2    SOFT    Test check 456
        Should Be True    ${res}    soft state not reached

        sleep    1s
        ${start_int}    Ctn Get Round Current Date
        Append To List    ${last_checks}    ${content}[last_check]
        Log To Console    Soft #${i} last_check=${content}[last_check]
        Ctn Schedule Forced Service Check    host_1    service_1
        ${i}    Evaluate    ${i} + 1
 
    END
    Log To Console    last_checks=${last_checks}
    # 1er soft , 2eme soft , 3eme soft ,4eme hard
    ${len_last_checks}    Get Length    ${last_checks}
    Should Be Equal As Integers    ${len_last_checks}    3    There should be exactly three distinct SOFT attempts

    ${result}    Ctn Check Service Output Resource Status With Timeout    host_1    service_1    60    ${start_int}    2  HARD  Test check 456
    Should Be True    ${result}    resources table not updated


    ${start}    Ctn Get Round Current Date
    #service_1 check ok
    Ctn Set Command Status    456    ${0}
    
    Ctn Schedule Forced Service Check    host_1    service_1

    ${result}    Ctn Check Service Output Resource Status With Timeout    host_1    service_1    60    ${start_int}    0  HARD  Test check 456
    Should Be True    ${result}    resources table not updated

BEOTEL_CENTREON_AGENT_CHECK_SERVICE_FRESHNESS
    [Documentation]    Given Centreon Engine is configured with an OpenTelemetry server module
...    And broker, engine and agent are started and the OTEL server is ready
...    When the agent executes checks under normal operation
...    Then service_1 MUST be recorded as HARD OK (status 0) with output "Test check 456"
...    When the agent is stopped and no new check results arrive for a period exceeding the freshness_threshold (30 seconds)
...    Then service_1 MUST transition to HARD UNKNOWN (status 3) with output "(Execute command failed)"
    [Tags]    broker    engine    opentelemetry    MON-162182

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" == "WSL"    "This test is only for linux agent version"

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
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1
    Ctn Engine Config Replace Value In Services    ${0}    service_1    max_check_attempts    3
    Ctn Engine Config Set Value In Services    ${0}    service_1    check_freshness    1
    Ctn Engine Config Set Value In Services    ${0}    service_1    freshness_threshold    30

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

    ${start}    Ctn Get Round Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    Ctn Wait For Otel Server To Be Ready    ${start}
    
    ${i}    Set Variable    1
    ${prev_last_check}    Set Variable    None
    ${tries}    Set Variable    0
    ${last_checks}    Create List

    ${result}    Ctn Check Service Output Resource Status With Timeout    host_1    service_1    80    ${start_int}    0  HARD  Test check 456
    Should Be True    ${result}    resources table not updated

    Ctn Kindly Stop Agent
    Sleep    35s

    ${result}    Ctn Check Service Output Resource Status With Timeout    host_1    service_1    60    ${start_int}    3  HARD  (Execute command failed) 
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
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    1

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
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    #wait for new data from agent
    ${content}    Create List    \"output\":\"OK check2
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "output":"OK check2 should be available.

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
    Ctn Engine Config Set Value    0    interval_length    5
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1

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
    ...    {"max_length_grpc_log":0,"centreon_agent":{"export_period":5, "reverse_connections":[{"host": "${host_host_name}","port": 4321, "encryption": "full", "ca_certificate": "/tmp/reverse_server_grpc.crt"}]}}

    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    Ctn Set Hosts Passive    ${0}    host_1 
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    1

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
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"}, "centreon_agent":{"export_period":5}, "max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    
    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp    ${echo_command}    OTEL connector
    Ctn Set Hosts Passive    ${0}    host_1 
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    1

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    ${token}    Ctn Create Jwt Token    ${600}
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
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    Sleep    1

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    120    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated

BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_WITHOUT_CERT
    [Documentation]    Given Engine is configured with a full TLS gRPC server (port 4318) exposing its CA certificate,
    ...    And the CMA has only a fingerprint (no CA certificate file) and a JWT token,
    ...    When the CMA starts and attempts a full TLS connection to Engine (which fails — no CA yet),
    ...    Then the CMA falls back to an insecure TLS connection to retrieve the CA certificate from Engine,
    ...    And Engine logs a security entry confirming the CA certificate was delivered,
    ...    And the CMA validates the retrieved CA certificate against the configured fingerprint,
    ...    And the CMA logs a security entry confirming the CA bootstrap succeeded,
    ...    And the CMA stores the CA certificate and reconnects using full TLS with the JWT token,
    ...    Then the host check result is reported successfully in the resources table.
    [Tags]    broker    engine    opentelemetry    MON-192054
    
    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" == "WSL"    "This test is only for linux"

    Ctn Config Engine    ${1}    ${2}    ${2}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"}, "centreon_agent":{"export_period":5}, "max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    
    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp    ${echo_command}    OTEL connector
    Ctn Set Hosts Passive    ${0}    host_1 
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    1

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    ${token}    Ctn Create Jwt Token    ${600}
    ${fingerprint}    Ctn Get Cert Fingerprint    /tmp/server_grpc.crt
    Ctn Config Centreon Agent    ${None}    ${None}    ${None}    ${token}    fingerprint=${fingerprint}
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
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.

    # Verify engine security log: CA certificate was delivered to the agent
    ${content}    Create List    [SECURITY] CA bootstrap certificate delivered to peer
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Engine security log "[SECURITY] CA bootstrap certificate delivered to peer" not found

    # Verify agent security log: CA was successfully bootstrapped from fingerprint
    ${content}    Create List    [SECURITY] CA bootstrap succeeded from fingerprint for endpoint
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    30    agent_format=${True}
    Should Be True    ${result}    Agent security log "[SECURITY] CA bootstrap succeeded from fingerprint for endpoint" not found

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    120    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated


BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_ENCRYPTED_CREDENTIALS
    [Documentation]    Given an agent host checked by centagent over an encrypted connection,
    ...    Engine use credentials encryption and send encrypted commands
    ...    we set a first output to check command, 
    ...    modify it, reload engine and expect the new output in resource table
    [Tags]    broker    engine    opentelemetry    MON-158789
    Ctn Config Engine    ${1}    ${2}    ${2}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"}, "centreon_agent":{"export_period":5}, "max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    
    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp    ${echo_command}    OTEL connector
    Ctn Set Hosts Passive    ${0}    host_1 
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    1

    Ctn Engine Config Set Value    0    log_level_checks    trace
    
    Create File    /etc/centreon-engine/engine-context.json   {"app_secret":"${AppSecret}","salt":"${Salt}"}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    ${token}    Ctn Create Jwt Token    ${600}
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

    Sleep    1s
    Remove File    /etc/centreon-engine/engine-context.json

    #lets wait engine send keys to CMA
    ${content}    Create List    Engine will send encrypted credentials to agent
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    120
    Should Be True    ${result}    "Engine will send encrypted credentials to agent" should be available.

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
    Ctn Engine Config Set Value    0    interval_length    5
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1

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
    ${result}    Ctn Check Commandline Service With Timeout Rt    host_1    service_1    30   {"check": "cpu_percentage"}
    Should Be True    ${result}    command line not found in db


    #a small threshold to make service_1 warning
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check2

    Ctn Engine Config Add Command    ${0}    otel_check2   {"check": "cpu_percentage", "args": {"warning-average" : "0.01"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    1    60    ANY
    Should Be True    ${result}    resources table not updated
    ${result}    Ctn Check Commandline Service With Timeout Rt    host_1    service_1    30   {"check": "cpu_percentage", "args": {"warning-average" : "0.01"}}
    Should Be True    ${result}    command line not found in db


    #a small threshold to make service_1 critical
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check3

    Ctn Engine Config Add Command    ${0}    otel_check3   {"check": "cpu_percentage", "args": {"critical-average" : "0.02", "warning-average" : "0.01"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    2    60    ANY
    Should Be True    ${result}    resources table not updated
    ${result}    Ctn Check Commandline Service With Timeout Rt    host_1    service_1    30   {"check": "cpu_percentage", "args": {"critical-average" : "0.02", "warning-average" : "0.01"}}
    Should Be True    ${result}    command line not found in db


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
    Ctn Engine Config Set Value    0    interval_length    5
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1

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
    Ctn Engine Config Set Value    0    interval_length    5
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1

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

    Ctn Engine Config Add Command    ${0}    otel_check2   {"check": "uptime", "args": {"warning-uptime" : "1000000000:"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    1    60    ANY
    Should Be True    ${result}    resources table not updated

    #a small threshold to make service_1 critical
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check3

    Ctn Engine Config Add Command    ${0}    otel_check3   {"check": "uptime", "args": {"critical-uptime" : "1000000000:"}}    OTEL connector

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
    Ctn Engine Config Set Value    0    interval_length    5
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1

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
    ${result}    Ctn Check Service Perfdata    host_1    service_1    60    2000000000    ${expected_perfdata}
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
    Ctn Engine Config Set Value    0    interval_length    5
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1

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

    Ctn Engine Config Add Command    ${0}    otel_check2   {"check": "service", "args": {"warning-total-running" : "1000:"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    1    60    ANY
    Should Be True    ${result}    resources table not updated

    #a small threshold to make service_1 critical
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check3

    Ctn Engine Config Add Command    ${0}    otel_check3   {"check": "service", "args": {"critical-total-running" : "1000:"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    2    60    ANY
    Should Be True    ${result}    resources table not updated


BEOTEL_CENTREON_AGENT_CHECK_NATIVE_SERVICE_FILTER_STATE
    [Documentation]    Given a windows agent with native service check, 
    ...    we filter on CentreonMonitoringAgent service and use warning-state and critical-state to trigger status changes
    [Tags]    broker    engine    opentelemetry    MON-192525

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
    Ctn Engine Config Set Value    0    interval_length    5
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1

    # filter on CentreonMonitoringAgent service which is running => OK
    Ctn Engine Config Add Command    ${0}    otel_check   {"check": "service", "args": {"filter-name": "centreonmonitoringagent1"}}    OTEL connector

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

    # agent is running => warning-state: running triggers WARNING
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check2

    Ctn Engine Config Add Command    ${0}    otel_check2   {"check": "service", "args": {"filter-name": "centreonmonitoringagent1", "warning-state": "running"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    1    60    ANY
    Should Be True    ${result}    resources table not updated

    # agent is running => critical-state: running triggers CRITICAL
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check3

    Ctn Engine Config Add Command    ${0}    otel_check3   {"check": "service", "args": {"filter-name": "centreonmonitoringagent1", "critical-state": "running"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    2    60    ANY
    Should Be True    ${result}    resources table not updated


BEOTEL_CENTREON_AGENT_CHECK_NATIVE_SERVICE_AUTO
    [Documentation]    Given a window agent with native service check, we check cma service with different configurations
    [Tags]    broker    engine    opentelemetry    MON-191652

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
    Ctn Engine Config Set Value    0    interval_length    5
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1

    Ctn Engine Config Add Command    ${0}    otel_check   {"check": "service", "args": { "start-auto": "true", "filter-name": "CentreonMonitoringAgent1", "critical-total-running": 1}}    OTEL connector

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

    #we filter on no start auto services => we should find 0 service cma
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check2

    Ctn Engine Config Add Command    ${0}    otel_check2   {"check": "service", "args": { "start-auto": false, "filter-name": "CentreonMonitoringAgent1", "critical-total-running": 1}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    2    60    HARD
    Should Be True    ${result}    resources table not updated

    #no filter => OK service
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    otel_check3

    Ctn Engine Config Add Command    ${0}    otel_check3   {"check": "service", "args": { "filter-name": "CentreonMonitoringAgent1", "critical-total-running": 1}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    0    60    HARD
    Should Be True    ${result}    resources table not updated

    #other filter on CMA
    Ctn Engine Config Add Command    ${0}    otel_check3   {"check": "service", "args": { "filter-name": "CentreonMonitoringAgent1", "critical-total-running": 1, "delayed" : "true"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    2    60    HARD
    Should Be True    ${result}    resources table not updated


    #no filter bis => OK service
    Ctn Engine Config Add Command    ${0}    otel_check3   {"check": "service", "args": { "start-type":"auto", "filter-name": "CentreonMonitoringAgent1", "critical-total-running": 1, "delayed": "FALSE", "type": "service-own-process"}}    OTEL connector

    Ctn Reload Engine
    ${result}     Ctn Check Service Resource Status With Timeout    host_1    service_1    0    60    HARD
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
    Ctn Engine Config Set Value    0    interval_length    5
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_1    retry_interval    1
    Ctn Engine Config Replace Value In Services    ${0}    service_2    check_interval    2
    Ctn Engine Config Replace Value In Services    ${0}    service_2    retry_interval    1

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


BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_MANY_AGENT
    [Documentation]    Given an engine listening for incomming CMA connections,
    ...    We connect two agent with the same host_name and engine must not crash
    [Tags]    broker    engine    opentelemetry    MON-192382
    Ctn Config Engine    ${1}    ${100}    ${1}

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" == "WSL"    "This test is only for linux"

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key"}, "centreon_agent":{"export_period":5}, "max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name

    FOR     ${host_index}    IN RANGE    ${1}    ${100}
        Ctn Engine Config Replace Value In Hosts    ${0}    host_${host_index}    check_command    otel_check_icmp
        Ctn Set Hosts Passive    ${0}    host_${host_index}
        Ctn Engine Config Set Value In Hosts    ${0}    host_${host_index}    check_interval    1
    END
    
    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp    ${echo_command}    OTEL connector
    Ctn Engine Config Set Value    0    interval_length    10

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    ${token}    Ctn Create Jwt Token    ${600}
    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_grpc.crt    ${token}     ${None}     full     ${True}     ${2}
    Ctn Add Token Otl Server Module    0    ${token}   
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent    ${2}

    # Let's wait for the otel server start
    ${content}    Create List    encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.

    Sleep    1

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    120    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated

    [teardown]    Run Keywords     Ctn Kindly Stop Agent    ${2}     AND     Ctn Stop Engine Broker And Save Logs

BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_SAN_IP
    [Documentation]    Given an engine with an encrypted otel server using a self-signed cert
    ...    whose CN does not match the hostname but has IP:127.0.0.1 and DNS:<hostname> in SAN,
    ...    with minute_certificate_ttl triggering cert regeneration,
    ...    the agent should connect successfully because SANs are copied to the generated cert.
    [Tags]    broker    engine    opentelemetry    MON-195905    Only_linux

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" == "WSL"    "This test is only for linux"

    Ctn Config Engine    ${1}    ${2}    ${2}

    # Create a self-signed cert with CN=san-test (wrong) but SAN=DNS:<hostname>,IP:127.0.0.1
    # Without the SAN copy fix, the generated cert would only have CN=san-test => peer verification fails
    ${host_name}    Ctn Host Hostname
    Run    openssl ecparam -genkey -name prime256v1 -noout -out /tmp/server_san.key
    Run    openssl req -new -x509 -key /tmp/server_san.key -out /tmp/server_san.crt -days 1 -subj '/CN=xxxxx' -addext 'subjectAltName=DNS:${host_name},IP:127.0.0.1'

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_san.crt", "private_key": "/tmp/server_san.key"}, "centreon_agent":{"export_period":5}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp

    ${echo_command}    Ctn Echo Command    "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp    ${echo_command}    OTEL connector
    Ctn Set Hosts Passive    ${0}    host_1
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    1

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    ${token}    Ctn Create Jwt Token    ${3600}
    Ctn Config Centreon Agent    ${None}    ${None}    /tmp/server_san.crt    ${token}
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
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    Sleep    1

    # Agent must connect: the generated cert has SANs (DNS:<hostname>,IP:127.0.0.1) copied from the CA
    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    120    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated - SAN was not copied to generated cert

BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_RENEW_CERT
    [Documentation]    Given an engine with an encrypted otel server with one minute ttl, we expect that otel server
    ...    will restart after on minute, then we recreate certificates and otel server must restart also
    [Tags]    broker    engine    opentelemetry    MON-192057

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" == "WSL"    "This test is only for linux"

    Ctn Config Engine    ${1}    ${2}    ${2}

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "/tmp/server_grpc.crt", "private_key": "/tmp/server_grpc.key", "minute_certificate_ttl": 1}, "centreon_agent":{"export_period":5}, "max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    
    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp    ${echo_command}    OTEL connector
    Ctn Set Hosts Passive    ${0}    host_1 
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    1

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
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    Sleep    1
    ${first_serv_start}    Get Current Date

    #sha should be register in db
    ${host_name}  Ctn Host Hostname
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result    SELECT COUNT(*) FROM instances WHERE LENGTH(cma_certificate_sha) > 20 AND cma_certificate_cn = '${host_name}' AND cma_certificate_peremption > UNIX_TIMESTAMP() + 364*86400    >=    ${1}    retry_timeout=10s    retry_pause=3s
    Disconnect From Database

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    120    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated


    # wait certificate peremption
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${first_serv_start}    ${content}    180
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    ${first_restart_int}    Ctn Get Round Current Date

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    120    ${first_restart_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated

    # to restore once we have done certifiate survey
    # Sleep    1

    # # recreate certificates
    # ${host_name}  Ctn Host Hostname
    # Ctn Create Key And Certificate  ${host_name}  /tmp/server_grpc.key   /tmp/server_grpc.crt
    # ${second_serv_restart}    Get Current Date
    # ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${second_serv_restart}    ${content}    120
    # Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    # Sleep    1

    # ${second_restart_int}    Ctn Get Round Current Date

    # #as ca is not the same as one used by agent, agent can't connect to engine
    # ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    60    ${second_restart_int}    0  HARD  OK - 127.0.0.1
    # Should Not Be True    ${result}    resources table updated, agent must not be able to connect to engine


BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_RENEW_CERT_WITH_DEFAULT_CERT
    [Documentation]    Given an engine with an encrypted otel server with one minute ttl and default engine generated certificates, 
    ...    we expect that otel server will restart after on minute, then we recreate certificates and otel server must restart also
    [Tags]    broker    engine    opentelemetry    MON-192057

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" == "WSL"    "This test is only for linux"

    Ctn Config Engine    ${1}    ${2}    ${2}

    Run    /usr/sbin/centengine -k 

    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4318, "encryption": "full", "public_cert": "", "private_key": "", "minute_certificate_ttl": 1}, "centreon_agent":{"export_period":5}, "max_length_grpc_log":0}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    otel_check_icmp
    
    ${echo_command}   Ctn Echo Command   "OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;"
    Ctn Engine Config Add Command    ${0}    otel_check_icmp    ${echo_command}    OTEL connector
    Ctn Set Hosts Passive    ${0}    host_1 
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    check_interval    1

    Ctn Engine Config Set Value    0    log_level_checks    trace

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    ${token}    Ctn Create Jwt Token    ${3600}
    Ctn Config Centreon Agent    ${None}    ${None}    /etc/pki/centreon-engine/default_cma_ca.crt    ${token}
    Ctn Add Token Otl Server Module    0    ${token}   
    Ctn Broker Config Log    central    sql    trace

    Ctn Config BBDO3    1
    Ctn Clear Retention
    Ctn Clear Db    instances

    ${start}    Get Current Date
    ${start_int}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent

    # Let's wait for the otel server start
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    20
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    Sleep    1
    ${first_serv_start}    Get Current Date

    #sha should be register in db
    ${host_name}  Ctn Host Hostname
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result    SELECT COUNT(*) FROM instances WHERE LENGTH(cma_certificate_sha) > 20 AND cma_certificate_cn = '${host_name}' AND cma_certificate_peremption > UNIX_TIMESTAMP() + 10*365*86400    >=    ${1}    retry_timeout=10s    retry_pause=3s
    Disconnect From Database

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    120    ${start_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated

    # wait certificate peremption
    ${content}    Create List    ] encrypted server listening on 0.0.0.0:4318
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${first_serv_start}    ${content}    180
    Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    ${first_restart_int}    Ctn Get Round Current Date

    ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    120    ${first_restart_int}    0  HARD  OK - 127.0.0.1
    Should Be True    ${result}    resources table not updated

    # to restore once we have done certifiate survey
    # Sleep    1

    # # recreate certificates
    # Run    /usr/sbin/centengine -k 
    # ${second_serv_restart}    Get Current Date
    # ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${second_serv_restart}    ${content}    120
    # Should Be True    ${result}    "encrypted server listening on 0.0.0.0:4318" should be available.
    # Sleep    1

    # ${second_restart_int}    Ctn Get Round Current Date

    # #as ca is not the same as one used by agent, agent can't connect to engine
    # ${result}    Ctn Check Host Output Resource Status With Timeout    host_1    60    ${second_restart_int}    0  HARD  OK - 127.0.0.1
    # Should Not Be True    ${result}    resources table updated, agent must not be able to connect to engine


BEOTEL_CENTREON_AGENT_CHECK_SERVICE_TIMEPERIODS
    [Documentation]    CMA timeperiod gating: services with always-active periods are checked,
    ...    services outside their period are deferred with the correct next-open offset.
    ...      service_1 -> 24x7           always active (built-in)
    ...      service_4 -> opens_in_1h    inactive now, opens in ~1 hour
    [Tags]    broker    engine    opentelemetry    MON-171482

    ${run_env}    Ctn Run Env
    Pass Execution If    "${run_env}" == "WSL"    This test requires the Linux agent

    Ctn Config Engine    ${1}    ${1}    ${4}
    Ctn Add Otl ServerModule
    ...    0
    ...    {"otel_server":{"host": "0.0.0.0","port": 4317},"max_length_grpc_log":0,"centreon_agent":{"export_period":5}}
    Ctn Config Add Otl Connector
    ...    0
    ...    OTEL connector
    ...    opentelemetry --processor=centreon_agent --extractor=attributes --host_path=resource_metrics.resource.attributes.host.name --service_path=resource_metrics.resource.attributes.service.name

    # opens_in_1h: today only, window from now+1h to now+2h (inactive right now)
    ${opens_in_1h}    Ctn Build Timeperiod Opening In    3600
    Ctn Engine Config Add Timeperiod    ${0}    opens_in_1h    Opens In 1 Hour    ${opens_in_1h}

    ${check_cmd}    Ctn Check Pl Command    --id 456
    Ctn Engine Config Add Command    ${0}    otel_check    ${check_cmd}    OTEL connector

    FOR    ${svc}    IN    service_1    service_4
        Ctn Engine Config Replace Value In Services    ${0}    ${svc}    check_command    otel_check
        Ctn Engine Config Replace Value In Services    ${0}    ${svc}    check_interval    2
        Ctn Engine Config Replace Value In Services    ${0}    ${svc}    retry_interval    1
        Ctn Engine Config Replace Value In Services    ${0}    ${svc}    max_check_attempts    2
    END

    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_period    24x7
    Ctn Engine Config Replace Value In Services    ${0}    service_4    check_period    opens_in_1h

    Ctn Set Services Passive    0    service_[1-4]
    Ctn Engine Config Set Value    0    interval_length    10
    Ctn Set Command Status    456    ${0}

    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Centreon Agent

    Ctn Config BBDO3    1
    Ctn Clear Db    resources
    Ctn Clear Retention

    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    module0    core    warning
    Ctn Broker Config Log    module0    processing    warning
    Ctn Broker Config Log    module0    neb    warning
    Ctn Engine Config Set Value    0    log_level_checks    error
    Ctn Engine Config Set Value    0    log_level_functions    error
    Ctn Engine Config Set Value    0    log_level_config    error
    Ctn Engine Config Set Value    0    log_level_events    error

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Start Agent
    Ctn Wait For Otel Server To Be Ready    ${start}

    # service_4 is outside its period: agent must log the deferral with next open ~3600s away.
    # Allow 35xx–36xx seconds to account for the few seconds elapsed since timeperiod computation.
    ${content}    Create List
    ...    service_4.*outside period 'opens_in_1h'.*next open at.*3[56][0-9][0-9]s from now
    ${result}    Ctn Find Regex In Log With Timeout    ${agentlog}    ${start}    ${content}    60    True
    Should Be True    ${result}    service_4 (opens_in_1h): agent did not log deferral with ~3600s next open

    # Always-active services: the agent must log a check start.
    ${content}    Create List    service 'service_1': inside period '24x7', starting check
    ${result}    Ctn Find In Log With Timeout    ${agentlog}    ${start}    ${content}    60    agent_format=${True}
    Should Be True    ${result}    service_1 (24x7): agent did not log check start

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

*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/resources.robot
Library             Process
Library             OperatingSystem
Library             DateTime
Library             Collections
Library             DatabaseLibrary
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Test Cases ***
EBNSVC1
    [Documentation]    New services with several pollers
    [Tags]    broker    engine    services    protobuf
    Config Engine    ${3}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${3}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    module1    bbdo_version    3.0.1
    Broker Config Add Item    module2    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    FOR    ${i}    IN RANGE    ${3}
        Sleep    10s
        ${srv_by_host}    Evaluate    20 + 4 * $i
        Log To Console    ${srv_by_host} services by host with 50 hosts among 3 pollers.
        Config Engine    ${3}    ${50}    ${srv_by_host}
        Reload Engine
        Reload Broker
        ${nb_srv}    Evaluate    17 * (20 + 4 * $i)
        ${nb_res}    Evaluate    $nb_srv + 17
        ${result}    Check Number Of Resources Monitored by Poller is    ${1}    ${nb_res}    30
        Should Be True    ${result}    Poller 1 should monitor ${nb_srv} services and 17 hosts.
        ${result}    Check Number Of Resources Monitored by Poller is    ${2}    ${nb_res}    30
        Should Be True    ${result}    Poller 2 should monitor ${nb_srv} services and 17 hosts.
        ${nb_srv}    Evaluate    16 * (20 + 4 * $i)
        ${nb_res}    Evaluate    $nb_srv + 16
        ${result}    Check Number Of Resources Monitored by Poller is    ${3}    ${nb_res}    30
        Should Be True    ${result}    Poller 3 should monitor ${nb_srv} services and 16 hosts.
    END
    Stop Engine
    Kindly Stop Broker

Service_increased_huge_check_interval
    [Documentation]    New services with high check interval at creation time.
    [Tags]    broker    engine    services    protobuf
    Config Engine    ${1}    ${10}    ${10}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Source Log    central    1
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Log    rrd    rrd    trace
    Broker Config Log    central    sql    debug
    Broker Config Log    rrd    core    error
    Config Broker Sql Output    central    unified_sql    10
    Broker Config Flush Log    central    0
    Broker Config Flush Log    rrd    0
    Clear Retention
    Clear Db    services
    Clear Db    index_data
    Clear Db    metrics

    Delete All Rrd Metrics

    ${start}    Get Current Date
    Start Broker
    Start Engine
    # Start Checkers
    ${result}    Check Host Status    host_1    4    1    False
    Should Be True    ${result}    host_1 should be pending

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "host_1 init not found in log"
    # End Checkers

    Process Service Check Result With Metrics    host_1    service_1    1    warning0    1

    ${content}    Create List    new pb data for metric
    ${result}    Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    60

    ${index}    Get Indexes To Rebuild    2
    ${metrics}    Get Metrics Matching Indexes    ${index}
    Log To Console    Metrics: ${metrics}

    FOR    ${m}    IN    @{metrics}
        ${result}    Check RRD Info    ${m}    ds[value].minimal_heartbeat    3000
        Should Be True
        ...    ${result}
        ...    ds[value].minimal_heartbeat must be equal to 3000
        ${result}    Check RRD Info    ${m}    rra[0].pdp_per_row    300
        Should Be True
        ...    ${result}
        ...    rra[0].pdp_per_row must be equal to 300
    END

    ${new_service_id}    Create Service    0    1    1

    Log To Console    new service: ${new_service_id}

    # do the same insert as php
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute Sql String
    ...    INSERT INTO index_data (host_id, service_id, host_name, service_description) VALUES (1, ${new_service_id}, 'host1', 'service_${new_service_id}')

    Engine Config Replace Value In Services    0    service_${new_service_id}    check_interval    90

    ${start}    Get Current Date

    Reload Engine

    ${content}    Create List    INITIAL SERVICE STATE: host_1;service_${new_service_id};
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "service_"${new_service_id}" init not found in log"

    ${start}    Get Current Date

    Sleep    5

    Process Service Check Result With Metrics    host_1    service_${new_service_id}    1    warning0    1

    ${metrics}    Get Metrics For Service    ${new_service_id}

    Should Not Be Equal    ${metrics}    None    no metric found for service ${new_service_id}

    FOR    ${m}    IN    @{metrics}
        ${result}    Wait Until File Modified    ${VarRoot}/lib/centreon/metrics/${m}.rrd    ${start}
        Should Be True
        ...    ${result}
        ...    ${VarRoot}/lib/centreon/metrics/${m}.rrd should have been modified since ${start}

        ${result}    Check RRD Info    ${m}    ds[value].minimal_heartbeat    54000
        Should Be True
        ...    ${result}
        ...    ds[value].minimal_heartbeat must be equal to 54000 for metric ${m}
        ${result}    Check RRD Info    ${m}    rra[0].pdp_per_row    5400
        Should Be True
        ...    ${result}
        ...    rra[0].pdp_per_row must be equal to 5400 for metric ${m}
    END

    [Teardown]    Run Keywords    Stop Engine    AND    Kindly Stop Broker

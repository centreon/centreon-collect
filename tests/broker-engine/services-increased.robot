*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
EBNSVC1
    [Documentation]    New services with several pollers
    [Tags]    broker    engine    services    protobuf
    Ctn Config Engine    ${3}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}
    Ctn Config BBDO3    3
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    FOR    ${i}    IN RANGE    ${3}
        Sleep    10s
        ${srv_by_host}    Evaluate    20 + 4 * $i
        Log To Console    ${srv_by_host} services by host with 50 hosts among 3 pollers.
        Ctn Config Engine    ${3}    ${50}    ${srv_by_host}
        Ctn Reload Engine
        Ctn Reload Broker
        ${nb_srv}    Evaluate    17 * (20 + 4 * $i)
        ${nb_res}    Evaluate    $nb_srv + 17
        ${result}    Ctn Check Number Of Resources Monitored By Poller Is    ${1}    ${nb_res}    30
        Should Be True    ${result}    Poller 1 should monitor ${nb_srv} services and 17 hosts.
        ${result}    Ctn Check Number Of Resources Monitored By Poller Is    ${2}    ${nb_res}    30
        Should Be True    ${result}    Poller 2 should monitor ${nb_srv} services and 17 hosts.
        ${nb_srv}    Evaluate    16 * (20 + 4 * $i)
        ${nb_res}    Evaluate    $nb_srv + 16
        ${result}    Ctn Check Number Of Resources Monitored By Poller Is    ${3}    ${nb_res}    30
        Should Be True    ${result}    Poller 3 should monitor ${nb_srv} services and 16 hosts.
    END
    Ctn Stop Engine
    Ctn Kindly Stop Broker

Service_increased_huge_check_interval
    [Documentation]    New services with huge check interval at creation time.
    ...    Given Engine and Broker are configured with 1 poller and 10 hosts
    ...    When Engine is started
    ...    Then host_1 should be pending
    ...    When a check result with metrics is processed for service_1
    ...    Then metrics should be created and sent to rrd broker
    ...    When service_1 metrics are analyzed
    ...    Then metrics should have minimal heartbeat of 3000 and pdp_per_row of 300
    ...    When a new service is created with a check interval of 90
    ...    And Engine is reloaded
    ...    Then the new service should be pending
    ...    When a check result with metrics is processed for the new service
    ...    Then metrics should be created and sent to rrd Broker
    ...    When new service metrics are analyzed
    ...    Then metrics should have minimal heartbeat of 54000 and pdp_per_row of 5400

    [Tags]    broker    engine    services    protobuf
    Ctn Config Engine    ${1}    ${10}    ${10}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Source Log    central    1
    Ctn Config BBDO3    1
    Ctn Broker Config Log    rrd    rrd    trace
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    rrd    processing    error
    Ctn Broker Config Log    central    processing    error
    Ctn Broker Config Log    module0    processing    error
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    module0    core    error
    Ctn Config Broker Sql Output    central    unified_sql    10
    Ctn Engine Config Set Value    0    log_level_checks    trace    1
    Ctn Engine Config Set Value    0    log_level_events    trace    1
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Clear Db    index_data
    Ctn Clear Db    metrics

    Ctn Delete All Rrd Metrics

    Ctn Start Broker
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    # Start Checkers
    ${result}    Ctn Check Host Status    host_1    4    1    False
    Should Be True    ${result}    host_1 should be pending

    # End Checkers

    Ctn Process Service Check Result With Metrics    host_1    service_1    0    ok0    1

    ${content}    Create List    new pb data for metric
    ${result}    Ctn Find In Log With Timeout    ${rrdLog}    ${start}    ${content}    60
    Should Be True    ${result}    'new pb data for metric' not found in RRD logs

    FOR    ${idx}    IN RANGE    60
	Sleep    1s
        ${index}    Ctn Get Indexes To Rebuild    2
	IF    len(${index}) == 2
            BREAK
	ELSE
	    # If not available, we force checks to have them.
            Ctn Schedule Forced Service Check    host_1    service_1
            Ctn Schedule Forced Service Check    host_1    service_2
        END
    END
    ${metrics}    Ctn Get Metrics Matching Indexes    ${index}
    Log To Console    Metrics: ${metrics}

    FOR    ${m}    IN    @{metrics}
        ${result}    Ctn Check Rrd Info    ${m}    ds[value].minimal_heartbeat    3000
        Should Be True
        ...    ${result}
        ...    ds[value].minimal_heartbeat must be equal to 3000
        ${result}    Ctn Check Rrd Info    ${m}    rra[0].pdp_per_row    300
        Should Be True
        ...    ${result}
        ...    rra[0].pdp_per_row must be equal to 300
    END

    # Create a new service on poller 0, host_1 and command_id 1 of Service_id 'new_service_id'
    ${new_service_id}    Ctn Create Service    0    1    1

    Log To Console    new service: ${new_service_id}

    # The database is prepared as done by the php.
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute Sql String
    ...    INSERT INTO index_data (host_id, service_id, host_name, service_description) VALUES (1, ${new_service_id}, 'host_1', 'service_${new_service_id}')

    # The service's check_interval is set to 90
    Ctn Engine Config Replace Value In Services    0    service_${new_service_id}    check_interval    90

    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${content}    Create List    INITIAL SERVICE STATE: host_1;service_${new_service_id};
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "service_"${new_service_id}" init not found in log"

    ${start}    Get Current Date

    Sleep    5s

    Ctn Process Service Check Result With Metrics
    ...    host_1
    ...    service_${new_service_id}
    ...    0    ok0    1

    ${metrics}    Ctn Get Metrics For Service    ${new_service_id}

    Should Not Be Equal    ${metrics}    ${None}    no metric found for service ${new_service_id}

    FOR    ${m}    IN    @{metrics}
        ${result}    Ctn Wait Until File Modified    ${VarRoot}/lib/centreon/metrics/${m}.rrd    ${start}
        Should Be True
        ...    ${result}
        ...    ${VarRoot}/lib/centreon/metrics/${m}.rrd should have been modified since ${start}

        ${result}    Ctn Check Rrd Info    ${m}    ds[value].minimal_heartbeat    54000
        Should Be True
        ...    ${result}
        ...    ds[value].minimal_heartbeat must be equal to 54000 for metric ${m}
        ${result}    Ctn Check Rrd Info    ${m}    rra[0].pdp_per_row    5400
        Should Be True
        ...    ${result}
        ...    rra[0].pdp_per_row must be equal to 5400 for metric ${m}
    END

    Ctn Stop Engine
    Ctn Kindly Stop Broker    no_rrd_test=True

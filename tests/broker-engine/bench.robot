*** Settings ***
Documentation       Centreon Broker and Engine benchmark

Resource            ../resources/resources.robot
Library             DateTime
Library             Process
Library             OperatingSystem
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py
Library             ../resources/Bench.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save logs If Failed


*** Test Cases ***
BENCH_1000STATUS
    [Documentation]    external command CHECK_SERVICE_RESULT 1000 times
    [Tags]    broker    engine    bench
    Config Engine    ${1}    ${50}    ${20}
    # We want all the services to be passive to avoid parasite checks during our test.
    Set Services Passive    ${0}    service_.*
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Log    central    sql    trace
    Broker Config Log    central    core    info
    Broker Config Log    central    processing    error
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Config Broker Sql Output    central    unified_sql
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    ${broker_stat_before}    get_broker_process_stat    51001
    ${engine_stat_before}    get_engine_process_stat    50001
    Process Service Check result    host_1    service_1    1    warning    config0    1    1000
    send_bench    1    50001
    ${bench_data}    get_last_bench_result_with_timeout    ${rrdLog}    1    central-rrd-master-output    60
    ${broker_stat_after}    get_broker_process_stat    51001
    ${engine_stat_after}    get_engine_process_stat    50001
    ${diff_broker}    diff_process_stat    ${broker_stat_after}    ${broker_stat_before}
    ${diff_engine}    diff_process_stat    ${engine_stat_after}    ${engine_stat_before}

    download_database_from_s3    bench.unqlite

    ${success}    store_result_in_unqlite
    ...    bench.unqlite
    ...    BENCH_1000STATUS
    ...    broker
    ...    ${diff_broker}
    ...    ${broker_stat_after}
    ...    ${bench_data}
    ...    central-broker-master-input-1
    ...    write
    ...    central-rrd-master-output
    ...    publish
    Should Be True    ${success}    "fail to save broker bench to database"

    ${success}    store_result_in_unqlite
    ...    bench.unqlite
    ...    BENCH_1000STATUS
    ...    engine
    ...    ${diff_engine}
    ...    ${engine_stat_after}
    ...    ${bench_data}
    ...    client
    ...    callback_pb_bench
    ...    central-module-master-output
    ...    read
    Should Be True    ${success}    "fail to save engine bench to database"

    upload_database_to_s3    bench.unqlite

    Stop Engine
    Stop Broker

BENCH_10000STATUS
    [Documentation]    external command CHECK_SERVICE_RESULT 10000 times
    [Tags]    broker    engine    bench
    Config Engine    ${1}    ${50}    ${20}
    # We want all the services to be passive to avoid parasite checks during our test.
    Set Services passive    ${0}    service_.*
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Log    central    sql    error
    Broker Config Log    central    core    info
    Broker Config Log    central    processing    error
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Config Broker Sql Output    central    unified_sql
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.
    ${broker_stat_before}    get_broker_process_stat    51001
    ${engine_stat_before}    get_engine_process_stat    50001
    Process Service Check result    host_1    service_1    1    warning    config0    1    10000
    send_bench    1    50001
    ${bench_data}    get_last_bench_result_with_timeout    ${rrdLog}    1    central-rrd-master-output    60
    ${broker_stat_after}    get_broker_process_stat    51001
    ${engine_stat_after}    get_engine_process_stat    50001
    ${diff_broker}    diff_process_stat    ${broker_stat_after}    ${broker_stat_before}
    ${diff_engine}    diff_process_stat    ${engine_stat_after}    ${engine_stat_before}

    download_database_from_s3    bench.unqlite

    ${success}    store_result_in_unqlite
    ...    bench.unqlite
    ...    BENCH_10000STATUS
    ...    broker
    ...    ${diff_broker}
    ...    ${broker_stat_after}
    ...    ${bench_data}
    ...    central-broker-master-input-1
    ...    write
    ...    central-rrd-master-output
    ...    publish
    Should Be True    ${success}    "fail to save broker bench to database"

    ${success}    store_result_in_unqlite
    ...    bench.unqlite
    ...    BENCH_10000STATUS
    ...    engine
    ...    ${diff_engine}
    ...    ${engine_stat_after}
    ...    ${bench_data}
    ...    client
    ...    callback_pb_bench
    ...    central-module-master-output
    ...    read
    Should Be True    ${success}    "fail to save engine bench to database"

    upload_database_to_s3    bench.unqlite

    Stop Engine
    Stop Broker

BENCH_1000STATUS_100ENGINE
    [Documentation]    external command CHECK_SERVICE_RESULT 100 times    with 100 pollers with 20 services
    [Tags]    broker    engine    bench
    Config Engine    ${100}    ${100}    ${20}
    FOR    ${poller_index}    IN RANGE    100
        # We want all the services to be passive to avoid parasite checks during our test.
        Set Services Passive    ${poller_index}    service_.*
    END
    Config Broker    module    ${100}
    Config Broker    central
    Config Broker    rrd
    Broker Config Log    central    sql    trace
    Broker Config Log    central    core    info
    Broker Config Log    central    processing    error
    Config BBDO3    ${100}
    Config Broker Sql Output    central    unified_sql
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${connected}    Wait For Connections    5669    100
    Should Be True    ${connected}    No 100 engine to broker connections
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log with Timeout    ${ENGINE_LOG}/config99/centengine.log    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    Sleep    5

    ${broker_stat_before}    get_broker_process_stat    51001
    ${engine_stat_before}    get_engine_process_stat    50001

    ${start_check}    Get Current Date
    # one host per poller
    FOR    ${poller_index}    IN RANGE    100
        ${host_id}    Evaluate    ${poller_index} + 1
        FOR    ${serv_index}    IN RANGE    20
            ${serv_id}    Evaluate    1 + ${serv_index} + ${poller_index} * 20
            Process Service Check result
            ...    host_${host_id}
            ...    service_${serv_id}
            ...    1
            ...    warning
            ...    config${poller_index}
            ...    1
            ...    100
            IF    ${poller_index} == 1    send_bench    1    50001
        END
    END

    ${bench_data}    get_last_bench_result_with_timeout    ${rrdLog}    1    central-rrd-master-output    60
    ${broker_stat_after}    get_broker_process_stat    51001
    ${engine_stat_after}    get_engine_process_stat    50001
    ${diff_broker}    diff_process_stat    ${broker_stat_after}    ${broker_stat_before}
    ${diff_engine}    diff_process_stat    ${engine_stat_after}    ${engine_stat_before}

    ${content}    Create List    pb service (100, 2000) status 1 type 1 check result output: <<warning_99>>
    ${result}    Find In Log with Timeout with Line    ${centralLog}    ${start_check}    ${content}    240
    Should Be True    ${result[0]}    No check check result received.
    ${date_last_check_received}    extract_date_from_log    ${result[1][0]}
    ${all_check_delay}    Subtract Date From Date    ${date_last_check_received}    ${start_check}

    ${delay_last_result}    Create Dictionary    200000_event_received    ${all_check_delay}

    download_database_from_s3    bench.unqlite

    ${success}    store_result_in_unqlite
    ...    bench.unqlite
    ...    BENCH_1000STATUS_100POLLER
    ...    broker
    ...    ${diff_broker}
    ...    ${broker_stat_after}
    ...    ${bench_data}
    ...    central-broker-master-input-\\d+
    ...    write
    ...    central-rrd-master-output
    ...    publish
    ...    ${delay_last_result}
    Should Be True    ${success}    "fail to save broker bench to database"

    ${success}    store_result_in_unqlite
    ...    bench.unqlite
    ...    BENCH_1000STATUS_100POLLER
    ...    engine
    ...    ${diff_engine}
    ...    ${engine_stat_after}
    ...    ${bench_data}
    ...    client
    ...    callback_pb_bench
    ...    central-module-master-output
    ...    read
    ...    ${delay_last_result}
    Should Be True    ${success}    "fail to save engine bench to database"

    upload_database_to_s3    bench.unqlite

    Stop Engine
    Stop Broker

*** Settings ***
Documentation       Centreon Broker and Engine benchmark

Resource            ../resources/resources.robot
Library             DateTime
Library             Process
Library             OperatingSystem
Library             Examples
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py
Library             ../resources/Bench.py

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Variables ***
@{CONFIG_NAME}      sql    core    processing    tcp    perfdata    victoria_metrics    bam    lua


#s of Cases had been removed in order to disable all bench.robot
*** Test Case ***
BENCH_${nb_check}STATUS
    [Documentation]    external command CHECK_SERVICE_RESULT 1000 times
    [Tags]    broker    engine    bench
    Ctn Config Engine    ${1}    ${50}    ${20}
    # We want all the services to be passive to avoid parasite checks during our test.
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    processing    error
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Config Broker Sql Output    central    unified_sql
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    ${broker_stat_before}    Ctn Get Broker Process Stat    51001
    ${engine_stat_before}    Ctn Get Engine Process Stat    50001
    Ctn Process Service Check Result    host_1    service_1    1    warning    config0    0    ${nb_check}
    Ctn Send Bench    1    50001
    ${bench_data}    Ctn Get Last Bench Result With Timeout    ${rrdLog}    1    central-rrd-master-output    60
    ${broker_stat_after}    Ctn Get Broker Process Stat    51001
    ${engine_stat_after}    Ctn Get Engine Process Stat    50001
    ${diff_broker}    Ctn Diff Process Stat    ${broker_stat_after}    ${broker_stat_before}
    ${diff_engine}    Ctn Diff Process Stat    ${engine_stat_after}    ${engine_stat_before}

    Ctn Download Database From S3    bench.unqlite

    ${success}    Ctn Store Result In Unqlite
    ...    bench.unqlite
    ...    BENCH_${nb_check}STATUS
    ...    broker
    ...    ${diff_broker}
    ...    ${broker_stat_after}
    ...    ${bench_data}
    ...    central-broker-master-input-1
    ...    write
    ...    central-rrd-master-output
    ...    publish
    Should Be True    ${success}    fail to save broker bench to database

    ${success}    Ctn Store Result In Unqlite
    ...    bench.unqlite
    ...    BENCH_${nb_check}STATUS
    ...    engine
    ...    ${diff_engine}
    ...    ${engine_stat_after}
    ...    ${bench_data}
    ...    client
    ...    callback_pb_bench
    ...    central-module-master-output
    ...    read
    Should Be True    ${success}    fail to save engine bench to database

    Ctn Upload Database To S3    bench.unqlite

    Examples:    nb_check    --
    ...    1000
    ...    10000

BENCH_${nb_check}STATUS_TRACES
    [Documentation]    external command CHECK_SERVICE_RESULT ${nb_check} times
    [Tags]    broker    engine    bench
    Ctn Config Engine    ${1}    ${50}    ${20}
    # We want all the services to be passive to avoid parasite checks during our test.
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    FOR    ${name}    IN    @{CONFIG_NAME}
        Ctn Broker Config Log    central    ${name}    trace
    END

    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Config Broker Sql Output    central    unified_sql
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    ${broker_stat_before}    Ctn Get Broker Process Stat    51001
    ${engine_stat_before}    Ctn Get Engine Process Stat    50001
    Ctn Process Service Check Result    host_1    service_1    1    warning    config0    0    ${nb_check}
    Ctn Send Bench    1    50001
    ${bench_data}    Ctn Get Last Bench Result With Timeout    ${rrdLog}    1    central-rrd-master-output    60
    ${broker_stat_after}    Ctn Get Broker Process Stat    51001
    ${engine_stat_after}    Ctn Get Engine Process Stat    50001
    ${diff_broker}    Ctn Diff Process Stat    ${broker_stat_after}    ${broker_stat_before}
    ${diff_engine}    Ctn Diff Process Stat    ${engine_stat_after}    ${engine_stat_before}

    Ctn Download Database From S3    bench.unqlite

    ${success}    Ctn Store Result In Unqlite
    ...    bench.unqlite
    ...    BENCH_${nb_check}STATUS_TRACES
    ...    broker
    ...    ${diff_broker}
    ...    ${broker_stat_after}
    ...    ${bench_data}
    ...    central-broker-master-input-1
    ...    write
    ...    central-rrd-master-output
    ...    publish
    Should Be True    ${success}    fail to save broker bench to database

    ${success}    Ctn Store Result In Unqlite
    ...    bench.unqlite
    ...    BENCH_${nb_check}STATUS_TRACES
    ...    engine
    ...    ${diff_engine}
    ...    ${engine_stat_after}
    ...    ${bench_data}
    ...    client
    ...    callback_pb_bench
    ...    central-module-master-output
    ...    read
    Should Be True    ${success}    fail to save engine bench to database

    Ctn Upload Database To S3    bench.unqlite

    Examples:    nb_check    --
    ...    1000
    ...    10000

BENCH_1000STATUS_100${suffixe}
    [Documentation]    external command CHECK_SERVICE_RESULT 100 times    with 100 pollers with 20 services
    [Tags]    broker    engine    bench
    Ctn Config Engine    ${100}    ${100}    ${20}
    Ctn Config Broker    module    ${100}
    Ctn Config Broker    central
    FOR    ${poller_index}    IN RANGE    100
        # We want all the services to be passive to avoid parasite checks during our test.
        Ctn Set Services Passive    ${poller_index}    service_.*
    END
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    processing    error
    Ctn Config BBDO3    ${100}
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    ${nb_conn}
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${connected}    Ctn Wait For Connections    5669    100
    Should Be True    ${connected}    100 engines should be connected to broker
    ${result}    Ctn Wait For Listen On Range    50001    50100    centengine    60
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${ENGINE_LOG}/config99/centengine.log    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    ${broker_stat_before}    Ctn Get Broker Process Stat    51001
    ${engine_stat_before}    Ctn Get Engine Process Stat    50001

    ${start_check}    Get Current Date
    # one host per poller
    FOR    ${poller_index}    IN RANGE    100
        ${host_id}    Evaluate    ${poller_index} + 1
        FOR    ${serv_index}    IN RANGE    20
            ${serv_id}    Evaluate    1 + ${serv_index} + ${poller_index} * 20
            Ctn Process Service Check Result
            ...    host_${host_id}
            ...    service_${serv_id}
            ...    1
            ...    warning
            ...    config${poller_index}
            ...    0
            ...    100
            IF    ${poller_index} == 1    Ctn Send Bench    1    50001
        END
    END

    ${bench_data}    Ctn Get Last Bench Result With Timeout    ${rrdLog}    1    central-rrd-master-output    60
    ${broker_stat_after}    Ctn Get Broker Process Stat    51001
    ${engine_stat_after}    Ctn Get Engine Process Stat    50001
    ${diff_broker}    Ctn Diff Process Stat    ${broker_stat_after}    ${broker_stat_before}
    ${diff_engine}    Ctn Diff Process Stat    ${engine_stat_after}    ${engine_stat_before}

    ${content}    Create List    pb service (100, 2000) status 1 type 1 check result output: <<warning_99>>
    ${result}    Ctn Find In Log With Timeout With Line    ${centralLog}    ${start_check}    ${content}    240
    Should Be True    ${result[0]}    No check check result received.
    ${date_last_check_received}    Ctn Extract Date From Log    ${result[1][0]}
    ${all_check_delay}    Subtract Date From Date    ${date_last_check_received}    ${start_check}

    ${delay_last_result}    Create Dictionary    200000_event_received    ${all_check_delay}

    Ctn Download Database From S3    bench.unqlite

    ${success}    Ctn Store Result In Unqlite
    ...    bench.unqlite
    ...    BENCH_1000STATUS_100POLLER${suffixe}
    ...    broker
    ...    ${diff_broker}
    ...    ${broker_stat_after}
    ...    ${bench_data}
    ...    central-broker-master-input-[0-9]+
    ...    write
    ...    central-rrd-master-output
    ...    publish
    ...    ${delay_last_result}
    Should Be True    ${success}    "fail to save broker bench to database"

    ${success}    Ctn Store Result In Unqlite
    ...    bench.unqlite
    ...    BENCH_1000STATUS_100POLLER${suffixe}
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

    Ctn Upload Database To S3    bench.unqlite

    Examples:    suffixe    nb_conn    --
    ...    ENGINE    1
    ...    ENGINE_2    2
    ...    ENGINE_3    3

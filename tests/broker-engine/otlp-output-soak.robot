*** Settings ***
Documentation       A never ending Engine/Broker pipeline used to watch the Broker 'otlp' output
...                 live, side by side with unified_sql. Several hosts, each with several
...                 services, are actively checked; the resulting pb_host_status and
...                 pb_service_status events feed both the unified_sql output (database) and the
...                 otlp output (exported to the local OTLP collector started by this suite).
...
...                 BEWARE: BEOTLPSOAK never returns with the default ${SOAK_DURATION} of 0.
...                 That is on purpose, but it means the CI would hang on this file. Either run
...                 it by hand, or bound it:
...
...                 robot -v SOAK_DURATION:600 broker-engine/otlp-output-soak.robot
...
...                 The OTLP output consumes pb_service_status and pb_host_status, so BBDO3 is
...                 mandatory here: in BBDO2 the muxer would never deliver anything to it.
...
...                 To watch the export in Grafana instead of asserting on it, start the stack in
...                 otlp-grafana/ and point the output at its collector:
...
...                 docker compose -f otlp-grafana/compose.yml up -d
...                 robot -v OTLP_PORT:4317 -v EXTERNAL_COLLECTOR:True -t BEOTLPSOAK .

Resource            ../resources/import.resource
Library             ../resources/OtlpCollector.py

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Otlp Soak Teardown


*** Variables ***
${NB_ENGINE}                ${1}
${NB_HOST}                  ${3}
${NB_SERVICE_BY_HOST}       ${5}
${OTLP_PORT}                ${4317}
${OTLP_DUMP}                /tmp/otlp-export.json
${EXTERNAL_COLLECTOR}       ${True}
# 0 means "never stop". Any positive value is a duration in seconds.
${SOAK_DURATION}            ${0}
# Seconds between two progress reports.
${REPORT_PERIOD}            ${30}
# Number of consecutive reports without a single new datapoint before failing.
${STALL_LIMIT}              ${4}

${OTLP_OUTPUT}              {"name": "central-broker-otlp", "type": "otlp", "endpoint": "127.0.0.1:${OTLP_PORT}", "max_datapoints_per_batch": "200", "max_send_interval": "5", "max_inflight_requests": "4", "export_timeout": "30", "send_thresholds": "yes", "send_status": "yes", "send_min_max": "yes","compression":"yes"}


*** Test Cases ***
BEOTLPSOAK
    [Documentation]    ${NB_HOST} hosts with ${NB_SERVICE_BY_HOST} services each are actively
    ...    checked forever. Broker exports their statuses to an OTLP collector while unified_sql
    ...    writes them to the database. The test reports both sides every ${REPORT_PERIOD}s and
    ...    only fails if the OTLP export stalls.
    [Tags]    broker    engine    otlp    unified_sql    soak
    [Timeout]    NONE

    Ctn Clear Retention
    Ctn Clear Db    resources
    Ctn Config Engine    ${NB_ENGINE}    ${NB_HOST}    ${NB_SERVICE_BY_HOST}
    # interval_length defaults to 60, so a check_interval of 5 means 5 minutes. Down to 1 we get
    # a check every 5s per service, which is what makes the pipeline visibly busy.
    FOR    ${i}    IN RANGE    ${NB_ENGINE}
        Ctn Engine Config Set Value    ${i}    interval_length    1
    END

    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${NB_ENGINE}

    # pb_service_status / pb_host_status only exist in BBDO3, and Ctn Config BBDO3 also switches
    # the central sql output to unified_sql.
    Ctn Config BBDO3    ${NB_ENGINE}
    Ctn Config Broker Sql Output    central    unified_sql

    Ctn Broker Config Add Output    central    ${OTLP_OUTPUT}

    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    processing    info
    Ctn Broker Config Log    central    otl    debug
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    central    perfdata    error
    Ctn Broker Config Log    central    tcp    error
    Ctn Broker Config Log    central    grpc    error

    Ctn Clear Retention

    IF    not ${EXTERNAL_COLLECTOR}
        Remove File    ${OTLP_DUMP}
        Ctn Start Otlp Collector    ${OTLP_PORT}    ${OTLP_DUMP}
    ELSE
        Log To Console    \nusing the external OTLP collector on 127.0.0.1:${OTLP_PORT}
    END

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${NB_ENGINE}

    # The otlp module is loaded from the endpoint type, not from a module list: the config parser
    # maps type 'otlp' to 70-otlp.so. If that log line is missing, the module is not installed.
    ${content}    Create List    otlp: endpoint 'central-broker-otlp' exporting to
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    The otlp output should be created by the endpoint applier.

    # Broker side rather than collector side, so this assertion holds with an
    # external collector too.
    ${result}    Ctn Wait For Otlp Export    ${1}    180
    Should Be True    ${result}    Broker reports no datapoint exported by its otlp output.

    IF    not ${EXTERNAL_COLLECTOR}
        ${result}    Ctn Wait For Otlp Datapoints    ${1}    60
        Should Be True    ${result}    Broker exported datapoints but the collector received none.
        ${names}    Ctn Otlp Collector Metric Names
        Log To Console    \nmetric names exported: ${names}
    END

    ${loop_start}    Evaluate    time.time()
    ${previous}    Set Variable    ${0}
    ${stalled}    Set Variable    ${0}
    ${iteration}    Set Variable    ${0}

    WHILE    True    limit=NONE
        Sleep    ${REPORT_PERIOD}s
        ${iteration}    Evaluate    ${iteration} + 1
        ${elapsed}    Evaluate    int(time.time() - ${loop_start})
        Log To Console    \n--- report ${iteration} (${elapsed}s of soak) ---

        ${stats}    Ctn Log Otlp Output Summary
        IF    not ${EXTERNAL_COLLECTOR}    Ctn Log Otlp Collector Summary
        Ctn Report Sql Progress

        IF    ${stats}[datapoints_sent] > ${previous}
            ${stalled}    Set Variable    ${0}
        ELSE
            ${stalled}    Evaluate    ${stalled} + 1
            Log To Console    OTLP export did not progress (${stalled}/${STALL_LIMIT})
        END
        ${previous}    Set Variable    ${stats}[datapoints_sent]

        Should Be True
        ...    ${stalled} < ${STALL_LIMIT}
        ...    OTLP export stalled: no new datapoint for ${stalled} periods of ${REPORT_PERIOD}s.

        IF    ${SOAK_DURATION} > 0 and ${elapsed} >= ${SOAK_DURATION}    BREAK
    END

    Ctn Log Otlp Output Summary
    IF    not ${EXTERNAL_COLLECTOR}
        Ctn Log Otlp Collector Summary
        ${hosts}    Ctn Otlp Collector Hosts
        Log To Console    \nhosts exported: ${hosts}
        Length Should Be    ${hosts}    ${NB_HOST}    All the hosts should show up in the OTLP export.
    END


*** Keywords ***
Ctn Otlp Soak Teardown
    [Documentation]    Stop the collector before the processes so that the last export attempts
    ...    are logged as failures rather than hanging on a dead socket.
    Ctn Log Otlp Output Summary
    IF    not ${EXTERNAL_COLLECTOR}
        Ctn Log Otlp Collector Summary
        Ctn Stop Otlp Collector
    END
    Ctn Stop Engine Broker And Save Logs

Ctn Report Sql Progress
    [Documentation]    Log what unified_sql has written so far. Wrapped in a TRY so a database
    ...    hiccup reports itself instead of killing a soak that has been running for hours.
    TRY
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${resources}    Query    SELECT count(*) FROM resources WHERE enabled=1
        ${fresh}    Query
        ...    SELECT count(*) FROM services WHERE last_check > UNIX_TIMESTAMP() - 300
        ${bins}    Query    SELECT count(*) FROM data_bin
        Log To Console
        ...    SQL: ${resources}[0][0] enabled resources, ${fresh}[0][0] services checked in the last 5min, ${bins}[0][0] rows in data_bin
        Disconnect From Database
    EXCEPT    AS    ${error}
        Log To Console    SQL report failed: ${error}
    END

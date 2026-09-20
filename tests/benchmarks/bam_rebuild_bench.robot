*** Settings ***
Documentation       How long the BI stream of cbd takes to rebuild the event durations of a
...                 BA history. The rebuild deletes every duration of the BAs concerned,
...                 then recomputes one per (closed event, reporting period) and writes it
...                 back, under the availability thread's lock. Its cost follows the size of
...                 the history, which is what the benchmark grows.

Resource            ../resources/import.resource
Library             robot_bench.py
Library             bam_config_gen.py

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Bam Rebuild Bench Teardown


*** Variables ***
${label}                ${EMPTY}    # defaults to the git branch, like ./bench.py does
# A small platform: the configuration load is bam-startup's business, it only has to
# exist here. What is grown is the history below.
${nb_hosts}             ${10}
${svc_by_host}          ${20}
${nb_ba}                ${10}
${kpi_per_ba}           ${2}
# The history: closed events per BA, and their length in seconds. 2000 events per BA
# over 10 BAs is 20000 durations to recompute -- about a week of a BA flapping every
# five minutes, ten times over.
${events_per_ba}        ${2000}
${event_duration}       ${300}
${ready_timeout}        1800


*** Test Cases ***
BENCH_BAM_REBUILD
    [Documentation]    Scenario: measure the BI rebuild of the event durations
    ...    Given a configuration database holding ${nb_ba} BAs
    ...    And a reporting history of ${events_per_ba} closed events per BA, under one 24x7 reporting period
    ...    And every BA flagged must_be_rebuild
    ...    When the central cbd is started alone, with no poller at all
    ...    Then the time reporting_stream spends recomputing the durations is filed in the store
    ...    And the number of durations written is filed alongside it
    [Tags]    broker    bam    bench

    ${params}    Ctn Bam Bench Populate    ${nb_hosts}    ${svc_by_host}    ${nb_ba}
    ...    ${kpi_per_ba}    ${0}    ${0}
    ${history}    Ctn Bam Bench Populate Ba Events    ${nb_ba}    ${events_per_ba}
    ...    ${event_duration}
    Set To Dictionary    ${params}    &{history}
    Log To Console    \nDatabase populated: ${params}

    Ctn Bam Rebuild Bench Configure

    ${start}    Get Current Date
    Ctn Start Broker    only_central=True

    ${content}    Create List    BAM-BI: event durations rebuild finished
    ${found}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}
    ...    ${ready_timeout}
    Should Be True    ${found}    cbd never finished rebuilding the event durations

    ${timings}    Ctn Bench Bam Rebuild Timings    ${centralLog}
    Should Not Be Empty    ${timings}
    ...    no complete rebuild sequence in the broker log: is the bam logger at info?

    # What was actually written, so that a fast run that wrote nothing is caught.
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Query    SELECT COUNT(*) FROM mod_bam_reporting_ba_events_durations
    Disconnect From Database
    ${durations}    Set Variable    ${output[0][0]}
    Should Be Equal As Integers    ${durations}    ${history}[events]
    ...    the rebuild wrote ${durations} durations for ${history}[events] closed events
    Set To Dictionary    ${timings}    durations=${durations}
    Ctn Bam Rebuild Bench File    ${timings}    ${params}


*** Keywords ***
Ctn Bam Rebuild Bench Configure
    [Documentation]    Configure a central broker carrying BAM, and nothing else worth
    ...    paying for. Same shape as the startup benchmark: no poller is started, the BAM
    ...    stream only needs the path of an Engine command FIFO and existing directories.
    Ctn Clear Retention
    Ctn Clear Logs
    Ctn Clear Broker Cache
    Ctn Clear Prot Files
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Add Bam Config To Broker    central
    # The two lines framing the rebuild are logged by the bam logger at info. The others
    # are turned down: a rebuild that writes its statements out would be measuring
    # spdlog, and it sends one statement per duration.
    Ctn Broker Config Log    central    bam    info
    Ctn Broker Config Log    central    sql    error
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    perfdata    error
    Ctn Broker Config Flush Log    central    0

Ctn Bam Rebuild Bench File
    [Documentation]    File the measurement in the store, under the campaign name.
    [Arguments]    ${timings}    ${params}
    ${campaign}    Set Variable If    "${label}" == "${EMPTY}"    ${None}    ${label}
    IF    $campaign is None
        ${campaign}    Ctn Bench Git Branch
    END
    ${run}    Ctn Bench Record Run    ${campaign}    bam-rebuild    durations
    ...    ${timings}    ${params}    unit=${EMPTY}
    Log To Console    \nbam-rebuild: ${timings} -> run ${run} of campaign '${campaign}'

Ctn Bam Rebuild Bench Teardown
    [Documentation]    Stop the central broker. No rrd broker was started, so its check is
    ...    skipped.
    Ctn Stop Engine Broker And Save Logs    only_central=True    no_rrd_test=True
    Remove File    ${rrdLog}

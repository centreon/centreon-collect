*** Settings ***
Documentation     What BAM costs the database in steady state: how many UPDATE statements a
...               passive check result turns into, when the result changes nothing. Measured
...               twice on the same load, with and without the BAM outputs, so that the
...               share of BAM stands out of what unified_sql writes anyway.

Resource          ../resources/import.resource
Library           robot_bench.py

Suite Setup       Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup        Ctn Stop Processes
Test Teardown     Ctn Bam Steady Bench Teardown


*** Variables ***
${label}           ${EMPTY}    # defaults to the git branch, like ./bench.py does
${nb_hosts}        ${50}
${svc_by_host}     ${20}
# Every service of the configuration is the KPI of exactly one BA, so that every
# submitted result reaches BAM: 100 BAs of 10 service KPIs = the 1000 services.
${nb_ba}           ${100}
${kpi_per_ba}      ${10}
# The load: passive results at a steady rate, all OK, on services already OK. Nothing
# changes state, which is what a platform looks like most of the time.
${passive_rate}    ${200}
${duration}        ${120}
# Time for the first result of every service to settle the KPIs and the BAs before the
# window opens, and for the last results to reach the database after it closes.
${settle}          ${30}
${drain}           ${20}


*** Test Cases ***
BENCH_BAM_STEADY_WITH_BAM
    [Documentation]    Scenario: count the UPDATE statements a passive result costs with BAM
    ...    Given ${nb_ba} BAs whose ${kpi_per_ba} service KPIs each cover the whole platform
    ...    And every service already OK
    ...    When ${passive_rate} OK results per second are submitted for ${duration}s
    ...    Then the UPDATE statements MariaDB ran during the window are counted
    ...    And filed per submitted result, under the variant "with-bam"
    [Tags]    broker    engine    bam    bench
    Ctn Bam Steady Bench Configure    ${True}
    Ctn Bam Steady Bench Run    with-bam

BENCH_BAM_STEADY_NO_BAM
    [Documentation]    Scenario: the same load, the BAM outputs left out of the central broker
    ...    Given the same configuration, the BAM outputs not configured
    ...    When the same load is submitted
    ...    Then the UPDATE statements are counted and filed under the variant "no-bam"
    ...    And the difference with "with-bam" is what BAM adds per result
    [Tags]    broker    engine    bam    bench
    Ctn Bam Steady Bench Configure    ${False}
    Ctn Bam Steady Bench Run    no-bam


*** Keywords ***
Ctn Bam Steady Bench Configure
    [Documentation]    A passive platform with a BAM configuration in the database, the BAM
    ...    outputs added to the central broker or not.
    [Arguments]    ${with_bam}
    Ctn Clear Retention
    Ctn Clear Logs
    Ctn Clear Broker Cache
    Ctn Clear Prot Files
    Ctn Clear Db Conf    mod_bam
    Ctn Config Engine    ${1}    ${nb_hosts}    ${svc_by_host}
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Set Hosts Passive    ${0}    host_.*
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config BBDO3    ${1}
    # A status that writes its statements out would be measuring spdlog.
    Ctn Broker Config Log    central    bam    error
    Ctn Broker Config Log    central    sql    error
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    perfdata    error

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine
    ${kpis}    Ctn Bench Bam Kpi Services    ${nb_ba}    ${kpi_per_ba}    ${nb_hosts}    ${svc_by_host}
    FOR    ${index}    ${services}    IN ENUMERATE    @{kpis}
        Ctn Create Ba With Services    ba-${index}    worst    ${services}
    END
    IF    ${with_bam}    Ctn Add Bam Config To Broker    central

Ctn Bam Steady Bench Run
    [Documentation]    Start the daemons, settle, count the UPDATEs over the window, file.
    [Arguments]    ${variant}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}

    # One result per service: the KPIs and the BAs take their state once, here, and
    # nothing in the window is a state change.
    ${services}    Evaluate    ${nb_hosts} * ${svc_by_host}
    Ctn Bench Submit Passive Results    ${services}    ${nb_hosts}    ${svc_by_host}    ${0}
    Sleep    ${settle}s

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${before}    Query    SHOW GLOBAL STATUS LIKE 'Com_update'
    Disconnect From Database

    ${submitted}    Ctn Bench Sustain Passive Load
    ...    ${passive_rate}
    ...    ${duration}
    ...    ${nb_hosts}
    ...    ${svc_by_host}
    ...    ${0}
    Sleep    ${drain}s

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${after}    Query    SHOW GLOBAL STATUS LIKE 'Com_update'
    # The proof that the load reached the database, and BAM when it is there.
    ${status}    Query    SELECT COUNT(*) FROM services WHERE state=0 AND last_check>=${start}
    Disconnect From Database
    Should Be True    ${status[0][0]} >= ${services}    the submitted results never reached the database

    ${updates}    Evaluate    int(${after[0][1]}) - int(${before[0][1]})
    ${per_result}    Evaluate    ${updates} / ${submitted}
    Log To Console    \n${variant}: ${submitted} results, ${updates} UPDATE statements, ${per_result} per result

    ${campaign}    Set Variable If    "${label}" == "${EMPTY}"    ${None}    ${label}
    IF    $campaign is None
        ${campaign}    Ctn Bench Git Branch
    END
    &{metrics}    Create Dictionary    results=${submitted}    updates=${updates}    updates_per_result=${per_result}
    &{params}    Create Dictionary
    ...    hosts=${nb_hosts}
    ...    services=${services}
    ...    bas=${nb_ba}
    ...    kpi_per_ba=${kpi_per_ba}
    ...    rate=${passive_rate}
    ...    duration=${duration}
    ${run}    Ctn Bench Record Run
    ...    ${campaign}
    ...    bam-steady
    ...    ${variant}
    ...    ${metrics}
    ...    ${params}
    ...    unit=${EMPTY}
    Log To Console    bam-steady/${variant}: ${metrics} -> run ${run} of campaign '${campaign}'

Ctn Bam Steady Bench Teardown
    [Documentation]    Stop everything. The rrd check is skipped: a window this long makes
    ...    it report metrics it did not produce.
    Ctn Stop Engine Broker And Save Logs    no_rrd_test=True
    Remove File    ${rrdLog}

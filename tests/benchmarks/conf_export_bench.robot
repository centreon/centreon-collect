*** Settings ***
Documentation     What a configuration export costs a running platform.
...
...               A whole export is announced at once with pollers.lck: the central cbd reads the
...               configuration of every poller, computes and writes a diff for each, sends them,
...               waits for the acknowledgements, then publishes the global diff. The pollers keep
...               monitoring meanwhile, and their events keep going through the same streams.
...
...               Two questions, measured together on the same exports:
...               - how long the export takes, from the announcement to the global diff, and what
...               CPU the central cbd spends on it;
...               - whether the real-time flow stalls while it runs. A probe flips the
...               notifications of one service per poller and times the change on its way to the
...               database, through the adaptive path, which unified_sql writes at once (see
...               conf_export_probe.py). A stream that waits for a diff to be computed shows up as
...               a latency peak.
...
...               Nothing about performance is asserted: the verdict comes from comparing two
...               campaigns with ./bench.py compare. What is asserted is that the probe did
...               measure something, before and during the exports.
...
...               | robot benchmarks/conf_export_bench.robot
...               | robot -v nb_pollers:2 -v nb_hosts:500 -v exports:5 benchmarks/conf_export_bench.robot
...
...               MariaDB has to be running: the probe reads the services table.

Resource          ../resources/import.resource
Library           robot_bench.py

Suite Setup       Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup        Ctn Stop Processes
Test Teardown     Ctn Conf Export Bench Teardown


*** Variables ***
# Overridable from the command line with -v name:value.
${label}             ${EMPTY}    # defaults to the git branch, like ./bench.py does
${nb_pollers}        ${4}
# Hosts of the whole platform, split evenly between the pollers: 250 hosts of 80
# services give each poller 20000 services, enough for a diff to take a while.
${nb_hosts}          ${1000}
${svc_by_host}       ${80}
# How many exports are measured, one after the other. Each one changes the
# retry_interval of the services whose number ends with 0 -- a tenth of them --
# alternating between two values, so that every export has a real diff to carry.
${exports}           ${3}
# Results per second for the whole platform, so that every stream is reading
# all the time.
${passive_rate}      ${200}
# Seconds of probing before the first export, for the baseline latency.
${baseline}          ${60}
# Seconds left between two exports, for the platform to come back to rest.
${settle}            ${30}
# Seconds after which an export is given up.
${export_timeout}    ${600}
# Seconds between two flips on each poller, and after which a flip is given up.
${probe_period}      ${0.2}
${probe_timeout}     ${60}
# Log level of the sql channel of the central cbd: error for a measurement,
# trace to see what unified_sql writes when something goes wrong.
${sql_log}           error

${probe_py}          /work/tests/benchmarks/conf_export_probe.py
${probe_out}         /tmp/bench-conf-export-probe.json
${probe_log}         /tmp/bench-conf-export-probe.log


*** Test Cases ***
BENCH_CONF_EXPORT
    [Documentation]    Scenario: measure an export on a platform under load
    ...    Given ${nb_pollers} pollers sharing ${nb_hosts} hosts of ${svc_by_host} passive services
    ...    And ${passive_rate} passive results per second spread over them
    ...    And a probe timing adaptive changes on each poller, on their way to the database
    ...    When ${exports} exports changing a tenth of the services are announced with pollers.lck
    ...    Then the duration of each export, the CPU of the central cbd and the latency of
    ...    the probe before and during the exports are filed in the store
    ...    And the run is rejected if the probe measured nothing
    [Tags]    broker    engine    bench
    Ctn Conf Export Bench Configure
    Ctn Conf Export Bench Start

    ${probe}    Start Process
    ...    python3    ${probe_py}
    ...    --pollers    ${nb_pollers}    --hosts    ${nb_hosts}
    ...    --services-by-host    ${svc_by_host}    --rate    ${passive_rate}
    ...    --period    ${probe_period}    --timeout    ${probe_timeout}
    ...    --out    ${probe_out}
    ...    --db-host    ${DBHost}    --db-port    ${DBPort}
    ...    --db-user    ${DBUser}    --db-password    ${DBPass}    --db-name    ${DBName}
    ...    stdout=${probe_log}    stderr=STDOUT
    Log To Console    \nProbing ${baseline}s before the first export
    Sleep    ${baseline}s

    ${windows}    Create List
    ${ids}    Evaluate    list(range(${nb_pollers}))
    FOR    ${i}    IN RANGE    ${exports}
        ${value}    Evaluate    3 if ${i} % 2 == 0 else 5
        FOR    ${p}    IN RANGE    ${nb_pollers}
            Ctn Engine Config Replace Value In Services    ${p}    service_\\d*0    retry_interval    ${value}
        END
        ${offset}    Ctn Bench Log Size    ${centralLog}
        ${cpu0}    Ctn Bench Cbd Cpu S
        ${t0}    Ctn Bench Now
        Ctn Notify Broker Of Engine Config Change    @{ids}
        ${t1}    Ctn Bench Wait Log Line    ${centralLog}    ${offset}
        ...    BBDO: Publishing global diff state    ${export_timeout}
        ${cpu1}    Ctn Bench Cbd Cpu S
        Should Be True    ${t1} > 0    export ${i} never reached its global diff
        ${cpu}    Evaluate    ${cpu1} - ${cpu0}
        ${window}    Create Dictionary    start=${t0}    end=${t1}    cpu_s=${cpu}
        Append To List    ${windows}    ${window}
        ${d}    Evaluate    round(${t1} - ${t0}, 3)
        Log To Console    export ${i}: ${d}s, ${cpu}s of cbd CPU
        Wait Until Removed    ${VarRoot}/lib/centreon/config/pollers.lck    60s
        Sleep    ${settle}s
    END

    Send Signal To Process    SIGTERM    ${probe}
    ${result}    Wait For Process    ${probe}    timeout=120s    on_timeout=kill
    ${out}    Get File    ${probe_log}
    Log To Console    ${out}
    Should Be Equal As Integers    ${result.rc}    0    the probe failed, see its output above

    ${metrics}    Ctn Bench Conf Export Summary    ${probe_out}    ${windows}    ${probe_timeout}
    Log To Console    ${metrics}
    Should Be True    ${metrics}[probe_samples_baseline] > 0
    ...    the probe measured nothing before the exports: no baseline to compare with
    Should Be True    ${metrics}[probe_samples_export] > 0
    ...    the probe measured nothing during the exports

    ${campaign}    Set Variable If    "${label}" == "${EMPTY}"    ${None}    ${label}
    IF    $campaign is None
        ${campaign}    Ctn Bench Git Branch
    END
    ${services}    Evaluate    ${nb_hosts} * ${svc_by_host}
    ${params}    Create Dictionary
    ...    pollers=${nb_pollers}
    ...    hosts=${nb_hosts}
    ...    services=${services}
    ...    rate=${passive_rate}
    ${run}    Ctn Bench Record Run    ${campaign}    conf-export    batch
    ...    ${metrics}    ${params}    unit=${EMPTY}
    Log To Console    conf-export: run ${run} of campaign '${campaign}'


*** Keywords ***
Ctn Conf Export Bench Configure
    [Documentation]    A centralized platform whose services and hosts are all passive, so
    ...    that no plugin is ever forked: the load is the one the probe submits.
    Ctn Clear Engine Configurations
    Ctn Clear Prot Files
    Ctn Clear Retention
    Ctn Clear Logs
    # The rows of the previous test would otherwise answer the probe before the
    # first global diff of this one is written.
    Ctn Clear Db    resources
    Ctn Clear Db    services
    Ctn Clear Db    hosts

    Ctn Config Centralized Engine    ${nb_pollers}    ${nb_hosts}    ${svc_by_host}
    FOR    ${p}    IN RANGE    ${nb_pollers}
        Ctn Set Services Passive    ${p}    service_.*
        Ctn Set Hosts Passive    ${p}    host_.*
        # The default configuration traces every scheduler call: at this size,
        # that would measure spdlog.
        Ctn Engine Config Set Value    ${p}    log_level_functions    error
        Ctn Engine Config Set Value    ${p}    log_level_checks    error
    END
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${nb_pollers}
    Ctn Config BBDO3    ${nb_pollers}
    Ctn Config Broker Sql Output    central    unified_sql
    # debug for config: the global diff line is what ends an export.
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    ${sql_log}
    Ctn Broker Config Log    central    bbdo    error
    Ctn Broker Config Log    rrd    core    error
    FOR    ${p}    IN RANGE    ${nb_pollers}
        Ctn Broker Config Log    module${p}    neb    error
    END

Ctn Conf Export Bench Start
    [Documentation]    Start the platform and wait for the initial export to be over: every
    ...    poller acknowledged, the global diff published, the command pipes open.
    ${offset}    Ctn Bench Log Size    ${centralLog}
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    ${t}    Ctn Bench Wait Log Line    ${centralLog}    ${offset}
    ...    BBDO: Publishing global diff state    ${export_timeout}
    Should Be True    ${t} > 0    the initial export never reached its global diff
    FOR    ${p}    IN RANGE    ${nb_pollers}
        ${ready}    Ctn Wait For External Command Pipe    ${p}    120
        Should Be True    ${ready}    the command pipe of poller ${p} never opened
    END
    Wait Until Removed    ${VarRoot}/lib/centreon/config/pollers.lck    60s

Ctn Conf Export Bench Teardown
    [Documentation]    Stop everything. The rrd check is skipped: the probe and the load share
    ...    step boundaries with Engine, and rrd rightfully complains about metrics it has
    ...    already seen. That has nothing to do with what is measured.
    Ctn Stop Engine Broker And Save Logs    no_rrd_test=True
    Remove File    ${rrdLog}
    Remove File    ${probe_log}

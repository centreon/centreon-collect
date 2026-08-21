*** Settings ***
Documentation       What the collect chain costs in steady state, in two profiles.
...
...                 BENCH_LOAD_ACTIVE is the production shape: centengine schedules its own
...                 checks, cbmod publishes, the central cbd writes to the database and forwards
...                 to the rrd one. It answers "what does this version cost to run".
...
...                 BENCH_LOAD_PASSIVE submits results at a fixed rate instead, so no plugin is
...                 ever forked. It answers a narrower question -- what processing one result
...                 costs -- with an exact denominator, which the active profile cannot give
...                 since the number of checks it runs is not controlled.
...
...                 Neither asserts anything about performance: they measure, and the verdict
...                 comes from comparing two campaigns with ./bench.py compare. What they do
...                 assert is that the load was real, because a window measuring an idle daemon
...                 would otherwise look like an excellent result.
...
...                 Run them through the driver, which names the campaign after the git branch
...                 and files the result in results/bench.db:
...
...                 | robot --test BENCH_LOAD_ACTIVE benchmarks/collect_load_bench.robot
...                 | robot -v duration:1800 -v nb_hosts:100 benchmarks/collect_load_bench.robot
...
...                 MariaDB has to be running: the configuration writes through unified_sql, and
...                 a broker retrying against a missing database is not the profile anyone wants
...                 to measure. The unstable tag keeps both out of the default selection.

Resource            ../resources/import.resource
Library             robot_bench.py

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Load Bench Teardown


*** Variables ***
# Overridable from the command line with -v name:value.
${label}            ${EMPTY}    # defaults to the git branch, like ./bench.py does
${duration}         600
# Not a round number chosen for comfort: the generated configuration carries
# max_service_check_spread=5, so Engine spreads its first checks over five minutes and
# the check rate only reaches its steady state after that. Measured on a 120s window
# with a 30s warm-up: 50 service checks per minute where the configuration calls for
# about 200. A shorter warm-up does not make the benchmark noisier, it makes it wrong.
${warmup}           330
${interval}         60
${nb_hosts}         ${50}
${svc_by_host}      ${20}

# Passive profile only: results per second. Deliberately not the rate of the active
# profile -- the two profiles answer different questions, and reading one against the
# other would be a mistake.
${passive_rate}     ${20}

# Long enough to defeat the libstdc++ small string optimization (15 bytes), with perf
# data, as a real plugin would produce. A shorter output allocates nothing when copied
# and would understate every figure.
${check_output}     OK - CPU usage is 12 percent | cpu=12%;80;90;0;100 mem=45%;70;85;0;100

# Active profile only: the throw-away plugin every command is rewritten to use.
${bench_plugin}     ${VarRoot}/lib/centreon-engine/bench-check.sh

${bench_py}         /work/tests/benchmarks/bench.py
${engine_cfg}       ${EtcRoot}/centreon-engine/config0/centengine.cfg


*** Test Cases ***
BENCH_LOAD_ACTIVE
    [Documentation]    Scenario: measure what a nominal poller costs its machine
    ...    Given an engine with ${nb_hosts} hosts and their services, actively checked
    ...    And the two cbd running in BBDO3 with unified_sql
    ...    When the collect daemons are measured for ${duration}s after a ${warmup}s warm-up
    ...    Then the CPU, the memory and the cost per check are filed in the store
    ...    And the run is rejected if no check was actually running
    [Tags]    broker    engine    bench    unstable
    Ctn Load Bench Configure    active
    Ctn Load Bench Start
    ${metrics}    Ctn Load Bench Measure    active

    # centenginestats gives the denominator, and it is also the proof that the scheduler
    # really was running: an idle engine would produce a beautiful CPU figure.
    Should Be True    ${metrics}[active_checks.service_per_min] > 0
    ...    no service check ran during the window: the figures measure an idle daemon
    Should Be True    ${metrics}[cpu_ms_per_active_check] > 0

BENCH_LOAD_PASSIVE
    [Documentation]    Scenario: measure what processing one check result costs
    ...    Given an engine whose services are all passive, so no plugin is ever forked
    ...    And ${passive_rate} results submitted every second, at a steady rate
    ...    When the collect daemons are measured for ${duration}s after a ${warmup}s warm-up
    ...    Then the cost of the chain is filed with the exact number of results submitted
    ...    And the run is rejected if the results never reached the database
    [Tags]    broker    engine    bench    unstable
    Ctn Load Bench Configure    passive
    Ctn Load Bench Start
    ${metrics}    Ctn Load Bench Measure    passive

    # The proof that the load went through the whole chain and not just into the FIFO:
    # a submitted WARNING has to be readable in the database. SOFT, not HARD: the
    # sustained load submits OK results, so a single WARNING is the first of the three
    # attempts the configuration asks for. And the return value is asserted -- the
    # keyword returns False on timeout rather than failing, so calling it bare would be
    # an assertion that can never fail.
    Ctn Bench Submit Passive Results    ${1}    ${nb_hosts}    ${svc_by_host}    ${1}
    ...    ${check_output}
    ${seen}    Ctn Check Service Status With Timeout    host_1    service_1    ${1}    60    SOFT
    Should Be True    ${seen}
    ...    the submitted results never reached the database: the window measured a chain that was not carrying the load
    Should Be True    ${metrics}[collect.cpu_total_s] > 0


*** Keywords ***
Ctn Load Bench Configure
    [Documentation]    Build the configuration of the given profile. Everything the two have
    ...    in common lives here; what differs is whether the services are checked actively.
    [Arguments]    ${profile}

    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Logs
    Ctn Clear Db    data_bin
    Ctn Clear Db    logs

    Ctn Config Engine    ${1}    ${nb_hosts}    ${svc_by_host}

    IF    "${profile}" == "active"
        # The plugin is reduced to a single echo: a real check.pl would make the machine
        # fork-bound, and since the probe deliberately excludes the CPU of the children,
        # the benchmark would then measure how fast the machine forks rather than what
        # the collect daemons cost. Written after Ctn Config Engine, which recreates the
        # directory.
        Create File    ${bench_plugin}    \#!/bin/sh\necho "${check_output}"\n
        Run    chmod +x ${bench_plugin}
        # Every command is rewritten, not only the command_<n> ones: host commands are
        # named checkh<n> and would keep running check.pl. The connector lines have to go
        # too, or the commands bound to the Perl Connector feed a shell script to an
        # embedded interpreter, fail, and bottleneck the scheduler on max_concurrent_checks.
        Run
        ...    sed -i -E "s|^[[:space:]]*command_line[[:space:]]+.*| command_line ${bench_plugin}|; /^[[:space:]]*connector[[:space:]]/d" ${EtcRoot}/centreon-engine/config0/commands.cfg
        ${cmds}    Get File    ${EtcRoot}/centreon-engine/config0/commands.cfg
        Should Not Contain    ${cmds}    check.pl
        Should Not Contain    ${cmds}    connector
    ELSE
        # No fork at all, so the profile holds only what Engine and the two cbd do with a
        # result. Hosts too: an active host check would fork just as much as a service one.
        Ctn Set Services Passive    ${0}    service_.*
        Ctn Set Hosts Passive    ${0}    host_.*
    END

    # The default configuration logs every scheduler call (functions=trace) and every
    # published event: hundreds of megabytes over a long window, which throttles the
    # daemons and turns the benchmark into a measurement of spdlog.
    Ctn Engine Config Set Value    ${0}    log_level_functions    error
    Ctn Engine Config Set Value    ${0}    log_level_checks    error
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    module0    neb    error
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    sql    error
    Ctn Broker Config Log    rrd    core    error
    Ctn Config BBDO3    1
    Ctn Config Broker Sql Output    central    unified_sql

Ctn Load Bench Start
    [Documentation]    Start the three daemons and let the scheduler reach its cruising rate.
    ...    Ctn Wait For Engine To Be Ready is not usable here: it greps a message of the
    ...    functions logger, which the configuration above turns off.
    Ctn Start Broker
    Ctn Start Engine
    Sleep    15s

Ctn Load Bench Measure
    [Documentation]    Measure the window with ./bench.py probe, and return what it filed.
    ...
    ...    The probe is a separate process on purpose: it is the same code that measures a
    ...    real installation, so a figure from the container and a figure from the field are
    ...    directly comparable. The variant and the parameters are passed to it because they
    ...    are what pairs two runs when two versions are compared -- without them an active
    ...    window and a passive one would look like the same measured point.
    [Arguments]    ${profile}
    ${services}    Evaluate    ${nb_hosts} * ${svc_by_host}
    ${campaign}    Set Variable If    "${label}" == "${EMPTY}"
    ...    ${None}    ${label}
    IF    $campaign is None
        ${campaign}    Ctn Bench Git Branch
    END
    Log To Console    \nMeasuring ${profile}: ${duration}s window after ${warmup}s of warm-up, filed under '${campaign}'

    ${probe}    Start Process
    ...    python3    ${bench_py}    probe    --label    ${campaign}
    ...    --variant    ${profile}    --duration    ${duration}    --warmup    ${warmup}
    ...    --interval    ${interval}    --engine-config    ${engine_cfg}
    # The backslashes are not decoration: robot reads a bare key=value argument as a
    # named argument, so "profile=active" would never reach the probe at all.
    ...    --param    profile\=${profile}    --param    hosts\=${nb_hosts}
    ...    --param    services\=${services}
    ...    stdout=/tmp/bench-load-probe.log    stderr=STDOUT

    # The passive profile has to keep feeding results for the whole window, warm-up
    # included: a load that stops when the measurement starts would be measured as idle.
    # The ten extra seconds cover the probe starting a moment after this keyword does.
    ${feeding}    Evaluate    ${warmup} + ${duration} + 10
    IF    "${profile}" == "passive"
        ${total}    Ctn Bench Sustain Passive Load    ${passive_rate}    ${feeding}
        ...    ${nb_hosts}    ${svc_by_host}
        Log To Console    ${total} passive results submitted
    END

    ${timeout}    Evaluate    ${warmup} + ${duration} + 300
    ${result}    Wait For Process    ${probe}    timeout=${timeout}s
    ${out}    Get File    /tmp/bench-load-probe.log
    Log To Console    \n${out}
    Should Be Equal As Integers    ${result.rc}    0
    ...    the probe failed, see the output above

    ${metrics}    Ctn Bench Last Run Metrics    ${campaign}    load    ${profile}
    Should Not Be Empty    ${metrics}    the probe filed no metric for '${campaign}'

    # The probe cannot know what a passive load submitted -- centenginestats counts only
    # active checks, and reports none here -- so the denominator is filed by the test.
    # The effective rate is used rather than the requested one: what matters is how many
    # results really went in, and the rate is steady by construction, so the share that
    # falls inside the measured window is a proportion of the total.
    IF    "${profile}" == "passive"
        ${in_window}    Evaluate    round(${total} * ${duration} / ${feeding})
        ${cpu_ms}    Evaluate    ${metrics}[collect.cpu_total_s] * 1000.0 / ${in_window}
        Ctn Bench Add Metric    ${campaign}    results_submitted    ${total}
        ...    results    load    passive
        Ctn Bench Add Metric    ${campaign}    results_in_window    ${in_window}
        ...    results    load    passive
        Ctn Bench Add Metric    ${campaign}    cpu_ms_per_result    ${cpu_ms}
        ...    ms    load    passive
        Set To Dictionary    ${metrics}    results_submitted=${total}
        ...    results_in_window=${in_window}    cpu_ms_per_result=${cpu_ms}
        Log To Console    ${in_window} results inside the window, ${cpu_ms} ms of CPU each
    END
    RETURN    ${metrics}

Ctn Load Bench Teardown
    [Documentation]    Stop everything. The rrd check is skipped: a window this long makes
    ...    engine and the injector share step boundaries, and rrd rightfully complains about
    ...    metrics it has already seen. That has nothing to do with what is measured.
    Ctn Stop Engine Broker And Save Logs    no_rrd_test=True
    # no_rrd_test only skips the check, it does not clean the log: the next test running the
    # usual teardown would fail on errors it did not produce.
    Remove File    ${rrdLog}
    Remove File    /tmp/bench-load-probe.log

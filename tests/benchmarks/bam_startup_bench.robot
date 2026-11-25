*** Settings ***
Documentation       How long cbd takes to load a BAM configuration, step by step, as the
...                 platform grows.
...
...                 At startup BAM reads its whole configuration from the Centreon database.
...                 Four of the five steps read BAM tables, whose size follows the number of
...                 BAs; the fifth, "loading mapping hosts <-> services", reads the *entire*
...                 service table of the platform through two joins and a DISTINCT, and keeps
...                 all of it in memory -- to resolve the handful of names the boolean rules
...                 mention and to check whether the KPI services are activated.
...
...                 So the benchmark varies two things independently, and the whole point is
...                 that they should not behave alike: growing the number of services must
...                 cost only in the mapping step, growing the number of BAs must cost in the
...                 other four. If the mapping step does not separate from the rest as the
...                 platform grows, the diagnosis behind this work is wrong and the
...                 optimisation is not worth writing.
...
...                 No centengine here, and that is not an omission: reader_v2 reads the
...                 database, never the .cfg files, so nothing in what is measured needs a
...                 poller. Leaving it out is what makes a 200k-service point fit on a
...                 laptop -- the memory would have gone to Engine, not to cbd.
...
...                 The figures come from cbd itself, which announces every step of
...                 reader_v2::read() at info. A run whose log carries no complete sequence
...                 fails on purpose: it means the bam logger is not at info, and a missing
...                 step would otherwise be read as a fast one.
...
...                 | robot --test BENCH_BAM_STARTUP benchmarks/bam_startup_bench.robot
...                 | robot -v nb_hosts:2500 -v nb_ba:200 benchmarks/bam_startup_bench.robot
...
...                 The unstable tag keeps it out of the default selection.

Resource            ../resources/import.resource
Library             robot_bench.py
Library             bam_config_gen.py

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Bam Startup Bench Teardown


*** Variables ***
${label}                ${EMPTY}    # defaults to the git branch, like ./bench.py does
# 500 hosts x 20 services = 10000 services, the smallest point of the campaign. Raise
# nb_hosts to move along the axis this benchmark is about; the others stay put.
${nb_hosts}             ${500}
${svc_by_host}          ${20}
# The BA side. Deliberately modest against the service count: a real platform has far
# fewer BAs than services, and it is precisely that asymmetry that makes reading the
# whole service table wasteful.
${nb_ba}                ${100}
${kpi_per_ba}           ${5}
${boolexp_per_ba}       ${1}
# Meta-service KPIs. Zero by default: meta-services are a legacy shape that a
# modern platform rarely uses, and including them in every point would make the
# BA axis measure two things at once. Raise it to exercise that path.
${nb_meta}              ${0}
${ready_timeout}        600


*** Test Cases ***
BENCH_BAM_STARTUP
    [Documentation]    Scenario: measure the BAM configuration load of a central cbd
    ...    Given a configuration database holding ${nb_hosts} hosts and their services
    ...    And ${nb_ba} BAs, each with ${kpi_per_ba} service KPIs and ${boolexp_per_ba} boolean rules
    ...    And ${nb_meta} meta-service KPIs
    ...    When the central cbd is started alone, with no poller at all
    ...    Then the duration of every step of reader_v2::read() is filed in the store
    ...    And the resident memory of cbd once loaded is filed alongside them
    [Tags]    broker    bam    bench

    ${params}    Ctn Bam Bench Populate    ${nb_hosts}    ${svc_by_host}    ${nb_ba}
    ...    ${kpi_per_ba}    ${boolexp_per_ba}    ${nb_meta}
    Log To Console    \nDatabase populated: ${params}

    Ctn Bam Startup Bench Configure

    ${start}    Get Current Date
    # Only the central: the rrd broker takes no part in loading a BAM configuration, and
    # a second cbd would only add noise to the memory figure.
    Ctn Start Broker    only_central=True

    ${content}    Create List    bam configuration loaded.
    ${found}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}
    ...    ${ready_timeout}
    Should Be True    ${found}    cbd never finished loading its BAM configuration

    # Read before stopping: /proc disappears with the process, and this is the figure
    # that says what the in-memory mapping costs.
    ${pid}    Get Process Id    b1
    ${rss}    Ctn Bench Process Rss Kb    ${pid}

    ${timings}    Ctn Bench Bam Load Timings    ${centralLog}
    Should Not Be Empty    ${timings}
    ...    no complete BAM load sequence in the broker log: is the bam logger at info?
    Set To Dictionary    ${timings}    rss_kb=${rss}
    Ctn Bam Startup Bench File    ${timings}    ${params}


*** Keywords ***
Ctn Bam Startup Bench Configure
    [Documentation]    Configure a central broker carrying BAM, and nothing else worth
    ...    paying for. The engine side is generated small: no poller is started, but the
    ...    BAM stream is configured with the path of an Engine command FIFO and the
    ...    directories have to exist.
    Ctn Clear Retention
    Ctn Clear Logs
    Ctn Clear Broker Cache
    Ctn Clear Prot Files
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Add Bam Config To Broker    central
    # The steps are logged by the bam logger at info, so it has to stay at info at
    # least. The others are turned down: a startup that writes its sql statements out
    # would be measuring spdlog.
    Ctn Broker Config Log    central    bam    info
    Ctn Broker Config Log    central    sql    error
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    perfdata    error
    # Without this the last lines of the sequence can still sit in spdlog's buffer when
    # the test reads the file, and a step would come out as a gap of several seconds.
    Ctn Broker Config Flush Log    central    0

Ctn Bam Startup Bench File
    [Documentation]    File the measurement in the store, under the campaign name.
    [Arguments]    ${timings}    ${params}
    ${campaign}    Set Variable If    "${label}" == "${EMPTY}"    ${None}    ${label}
    IF    $campaign is None
        ${campaign}    Ctn Bench Git Branch
    END
    ${run}    Ctn Bench Record Run    ${campaign}    bam-startup    load
    ...    ${timings}    ${params}    unit=${EMPTY}
    Log To Console    \nbam-startup: ${timings} -> run ${run} of campaign '${campaign}'

Ctn Bam Startup Bench Teardown
    [Documentation]    Stop the central broker. The rrd check is skipped: no rrd broker was
    ...    ever started, so there are no metrics for it to look at.
    Ctn Stop Engine Broker And Save Logs    only_central=True    no_rrd_test=True
    Remove File    ${rrdLog}

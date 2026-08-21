*** Settings ***
Documentation       How long Engine takes to read its configuration and start monitoring, in
...                 the three shapes that configuration can take.
...
...                 BENCH_START_LEGACY parses centengine.cfg and its object files, the historical
...                 path. BENCH_START_PROTO starts from the state.prot Engine wrote on a previous
...                 run, which is what a warm poller does in centralized mode: a protobuf
...                 deserialization instead of a text parse, and no expand or resolve at all
...                 since Broker has already validated it. Reading one against the other is the
...                 first measurement of what centralized configuration buys at startup.
...
...                 BENCH_START_CENTRALIZED_COLD is the third shape: no state.prot, so Broker
...                 pushes the whole configuration and Engine applies it as a diff. It measures
...                 something the other two do not -- the negotiation -- and it is the shape a
...                 poller has after a reinstall.
...
...                 The figures come from Engine itself. It emits one
...                 "Startup timing: <phase> = <n> ms" line per phase, at info, and this test
...                 collects them: measuring from the outside would give a total and nothing
...                 else, since --verify-config runs parse, expand, resolve and apply as one
...                 block. A run whose log carries no such line fails on purpose -- it means the
...                 binary predates the instrumentation, and a total alone would be read as a
...                 phase breakdown that is not there.
...
...                 | robot --test BENCH_START_LEGACY benchmarks/startup_bench.robot
...                 | robot -v nb_hosts:500 -v label:dt-broker benchmarks/startup_bench.robot
...
...                 The unstable tag keeps all three out of the default selection.

Resource            ../resources/import.resource
Library             robot_bench.py

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Startup Bench Teardown


*** Variables ***
${label}            ${EMPTY}    # defaults to the git branch, like ./bench.py does
# Ten thousand services. Fifty hosts made every phase land between 0 and 4 ms, where
# rounding to the millisecond is most of the signal; at this size the phases are worth
# reading and the configuration still generates in seconds.
${nb_hosts}         ${500}
${svc_by_host}      ${20}
# The engine log file is ${engineLog0}, from resources.resource. Do not define a local
# variable for it: robot variable names ignore case and underscores, so an ${engine_log}
# here would silently shadow ${ENGINE_LOG} -- the log *directory* the suite setup wipes --
# and Ctn Clear Engine Logs would then try to remove a file as a directory. Cost: three
# failed runs whose setup died before any test started.
${ready_timeout}    300


*** Test Cases ***
BENCH_START_LEGACY
    [Documentation]    Scenario: measure a startup that parses the text configuration
    ...    Given an engine configured with ${nb_hosts} hosts and their services as .cfg files
    ...    And no state.prot, so the text files are what gets read
    ...    When engine is started and reaches its event loop
    ...    Then the duration of every startup phase is filed in the store
    [Tags]    broker    engine    bench    unstable
    Ctn Startup Bench Configure
    # No state.prot: this is what makes Engine take the text path rather than
    # deserializing a State, and it is the whole difference with the next test.
    Ctn Clear Prot Files
    ${timings}    Ctn Startup Bench Run    legacy

    # The text path is the only one that expands and resolves: the proto Broker pushes
    # has already been validated, so those two phases are skipped there. Asserting them
    # here is what proves the scenario really took the path it claims to measure.
    Dictionary Should Contain Key    ${timings}    expand
    Dictionary Should Contain Key    ${timings}    resolve
    Dictionary Should Contain Key    ${timings}    apply

BENCH_START_PROTO
    [Documentation]    Scenario: measure a startup that reads a serialized configuration
    ...    Given a poller that has already received its configuration from the broker once
    ...    And therefore left a state.prot behind
    ...    When engine is started again, the broker still running
    ...    Then the configuration is deserialized instead of parsed, and expand and resolve are skipped
    ...
    ...    The first start has to go through the centralized path: a plain BBDO3 engine
    ...    configured from .cfg files never writes a state.prot, so a legacy first start
    ...    would leave nothing to measure and the test would time out waiting for it.
    [Tags]    broker    engine    bench    unstable
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Logs
    Ctn Config Centralized Engine    ${1}    ${nb_hosts}    ${svc_by_host}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Broker Sql Output    central    unified_sql

    # First start only exists to produce the state.prot; nothing is measured here.
    ${first}    Get Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    # Not "Wait Until Created": engine rewrites state.prot after *every* apply, so the
    # file appears within milliseconds holding the near-empty configuration it started
    # on. Waiting for that gave a second start with nothing to deserialize -- zero
    # milliseconds of config-read and the whole cost showing up as a reload instead.
    # What has to be waited for is the configuration from Broker being applied, and then
    # the file being rewritten with it.
    ${content}    Create List    Reload timing: objects
    ${applied}    Ctn Find In Log With Timeout    ${engineLog0}    ${first}    ${content}
    ...    ${ready_timeout}
    Should Be True    ${applied}    engine never received its configuration from broker
    Wait Until Keyword Succeeds    2 min    2 s    Ctn Startup Bench Prot Is Full

    # Engine alone goes down: a warm poller restarts against a broker that is already
    # there, and that is the shape being measured. Only the engine log is cleared, so
    # that the phases read back belong to the second start.
    Ctn Stop Engine
    Ctn Clear Engine Logs

    # newGeneration again, and not only because the first start used it: that is the
    # invocation that passes -p <proto dir> and -b <module.json>, and without -p Engine
    # has no state.prot to read at all -- it would fall back to the text files and
    # measure the legacy scenario a second time.
    ${timings}    Ctn Startup Bench Run    proto    ${True}    ${True}

    # If expand or resolve showed up here, Engine fell back to the text files and the
    # comparison with the legacy scenario would be between two identical things.
    Dictionary Should Not Contain Key    ${timings}    expand
    Dictionary Should Not Contain Key    ${timings}    resolve
    Dictionary Should Contain Key    ${timings}    apply

BENCH_START_CENTRALIZED_COLD
    [Documentation]    Scenario: measure a startup where Broker owns the configuration
    ...    Given a poller whose configuration lives on the broker side
    ...    And no state.prot on the engine side, so nothing local to start from
    ...    When broker and engine are started in new generation
    ...    Then the cost of receiving and applying the whole configuration is filed
    [Tags]    broker    engine    bench    unstable
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Logs
    Ctn Config Centralized Engine    ${1}    ${nb_hosts}    ${svc_by_host}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config Broker Sql Output    central    unified_sql
    # The diff phases are logged at debug, unlike the startup ones: they also run on
    # every configuration change at runtime, where a burst of info lines would be noise.
    Ctn Broker Config Log    central    config    debug
    Ctn Engine Config Set Value    ${0}    log_level_config    debug

    ${start}    Get Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    ${content}    Create List    Event loop start at
    ${found}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}
    ...    ${ready_timeout}
    Should Be True    ${found}    engine never reached its event loop

    # A cold centralized poller has almost nothing to start on: it reaches its event loop
    # in milliseconds, then receives the whole configuration from Broker and applies it as
    # a diff carrying the full state. That application is logged as "Reload timing", so
    # the real cost of this scenario is under reload.*, and waiting for it is part of the
    # measurement -- the poller is not monitoring anything until it has landed.
    ${content}    Create List    Reload timing: objects
    ${applied}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}
    ...    ${ready_timeout}
    Should Be True    ${applied}    the broker never got its configuration applied by engine

    ${timings}    Ctn Bench Startup Timings    ${engineLog0}
    Should Not Be Empty    ${timings}
    ...    no timing line in the engine log: this binary has no startup instrumentation
    Dictionary Should Contain Key    ${timings}    reload.objects
    Ctn Startup Bench File    centralized-cold    ${timings}


*** Keywords ***
Ctn Startup Bench Configure
    [Documentation]    Build a plain BBDO3 configuration of the requested size. Nothing here
    ...    is specific to a scenario: what distinguishes them is whether a state.prot is left
    ...    in place, and whether the configuration is owned by Broker.
    Ctn Clear Retention
    Ctn Clear Logs
    Ctn Config Engine    ${1}    ${nb_hosts}    ${svc_by_host}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Config Broker Sql Output    central    unified_sql
    # The startup phases are emitted by the config logger at info, so it has to stay at
    # info at least. The others are turned down: a startup that writes hundreds of
    # thousands of scheduler lines is measuring spdlog.
    Ctn Engine Config Set Value    ${0}    log_level_config    info
    Ctn Engine Config Set Value    ${0}    log_level_functions    error
    Ctn Engine Config Set Value    ${0}    log_level_checks    error

Ctn Startup Bench Run
    [Documentation]    Start the daemons, wait for the event loop, and file what Engine logged.
    [Arguments]    ${variant}    ${engine_only}=${False}    ${new_generation}=${False}

    # The three scenarios have to give the retention phase the same work, or that column
    # is not comparable between them. They already did, by accident: retention.dat lives
    # under the engine log directory, which Ctn Clear Engine Logs wipes. Doing it here on
    # purpose, so that a future change to the log handling cannot silently make one
    # scenario parse a full retention while another parses none.
    # Ctn Clear Retention is deliberately not used: it also deletes broker's queue and
    # cache files, and the broker is still running at this point in the proto scenario.
    Remove File    ${VarRoot}/log/centreon-engine/config0/retention.dat

    ${start}    Get Current Date
    IF    not ${engine_only}
        Ctn Start Broker    newGeneration=${new_generation}
    END
    Ctn Start Engine    newGeneration=${new_generation}

    # Waiting on the event loop message rather than on a check result: what is measured
    # ends there, and anything after it belongs to the load benchmark.
    ${content}    Create List    Event loop start at
    ${found}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}
    ...    ${ready_timeout}
    Should Be True    ${found}    engine never reached its event loop

    ${timings}    Ctn Bench Startup Timings    ${engineLog0}
    Should Not Be Empty    ${timings}
    ...    no "Startup timing" line in the engine log: this binary has no startup instrumentation
    Dictionary Should Contain Key    ${timings}    total
    Ctn Startup Bench File    ${variant}    ${timings}
    RETURN    ${timings}

Ctn Startup Bench Prot Is Full
    [Documentation]    Fail while state.prot still holds the configuration engine started on
    ...    rather than the one Broker sent. Ten thousand services serialize to well over a
    ...    megabyte and an empty state to a few hundred bytes, so any threshold in between
    ...    tells the two apart without depending on an exact size.
    ${size}    Get File Size    ${VarRoot}/lib/centreon-engine/config0/state.prot
    Should Be True    ${size} > 100000
    ...    state.prot is only ${size} bytes: engine has not written the received configuration yet

Ctn Startup Bench File
    [Documentation]    File one scenario in the store, under the campaign name.
    [Arguments]    ${variant}    ${timings}
    ${campaign}    Set Variable If    "${label}" == "${EMPTY}"    ${None}    ${label}
    IF    $campaign is None
        ${campaign}    Ctn Bench Git Branch
    END
    ${services}    Evaluate    ${nb_hosts} * ${svc_by_host}
    ${params}    Create Dictionary    hosts=${nb_hosts}    services=${services}
    ${run}    Ctn Bench Record Run    ${campaign}    engine-startup    ${variant}
    ...    ${timings}    ${params}    unit=ms
    Log To Console    \n${variant}: ${timings} -> run ${run} of campaign '${campaign}'

Ctn Startup Bench Teardown
    [Documentation]    Stop everything. The rrd check is skipped: these scenarios start and
    ...    stop engine several times, so the same metrics can legitimately be sent twice.
    Ctn Stop Engine Broker And Save Logs    no_rrd_test=True
    Remove File    ${rrdLog}

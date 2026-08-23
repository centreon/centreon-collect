*** Settings ***
Documentation       Heap allocation benchmark, in four profiles, all of them making the
...                 check path the dominant code path of the daemon being traced. The first
...                 three trace centengine, EALLOC4 traces the central cbd instead --
...                 whatever touches the perf data parser only shows in that one, since
...                 centengine never parses a perf data string. EALLOC1 submits a
...                 large number of passive results, so no plugin is ever forked. EALLOC2 runs
...                 active checks on a bare plugin path, EALLOC3 the same with a command line
...                 the length of a real one — their difference isolates what forking costs
...                 per argument. Each test pauses twice, once with centengine warm and once
...                 with the workload finished, so that the very same scenario can be replayed
...                 against two centengine binaries and their traces compared.
...
...                 Those two pauses are driven by sentinel files rather than by prompts, so the
...                 tests run either way. Unattended, "bench.py run alloc" attaches and detaches
...                 heaptrack by itself; by hand, the console still prints the two commands to
...                 type. The protocol is four files under /tmp:
...
...                 | bench-alloc.ready | robot | the daemon is warm, and here is its pid |
...                 | bench-alloc.go | driver | heaptrack is attached, run the workload |
...                 | bench-alloc.done | robot | the workload is over, the daemon is stopping |
...                 | bench-alloc.go removed | driver | the trace has been read, the test may end |
...
...                 The unstable tag keeps all three out of the default selection
...                 (robot -e unstable .): they take twenty minutes and measure rather than
...                 assert.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Alloc Bench Setup
Test Teardown       Ctn Alloc Bench Teardown


*** Variables ***
${nb_checks}            100000

# The output has to be long enough to defeat the libstdc++ small string optimization
# (15 bytes): with a shorter one, copying a std::string allocates nothing at all and
# the measure would show no difference between the two binaries. Semicolons are safe
# here, the external command parser splits its arguments with MaxSplits(';', 3), so
# everything after the state is kept as the output.
${check_output}         OK - CPU usage is 12 percent | cpu=12%;80;90;0;100 mem=45%;70;85;0;100

# The sentinels synchronising the test with whoever drives heaptrack. They live in /tmp
# rather than under ${VarRoot}, which the suite setup and Ctn Config Engine wipe.
${ready_file}           /tmp/bench-alloc.ready
${go_file}              /tmp/bench-alloc.go
${done_file}            /tmp/bench-alloc.done

# How long the driver is given to attach, and then to detach. Generous on purpose: the
# manual mode has a human in the loop, and heaptrack takes a while to flush a large
# trace before it lets go.
${attach_timeout}       10 minutes

# EALLOC2 and EALLOC3 only: how long the active checks are left running under
# heaptrack, and the throw-away plugin they run.
${duration}             120s
${bench_plugin}         ${VarRoot}/lib/centreon-engine/bench-check.sh

# EALLOC3 only. Ten arguments, so eleven tokens with the plugin path, in the shape a
# real check has once its macros are expanded. The count is the whole point of the
# variant: misc::command_line::parse pushes one char* per token plus a trailing
# nullptr into a std::vector rebuilt at every exec, and libstdc++ doubles from zero.
# EALLOC2 has 1 token, so 2 pushes, so capacities 1 then 2: 2 allocations.
# EALLOC3 has 11 tokens, so 12 pushes, so capacities 1, 2, 4, 8, 16: 5 allocations.
# So EALLOC3 minus EALLOC2, on the same binary, should show +3 allocations per check
# on that one function. No pipe, no dollar and no ampersand in there: the value goes
# through a sed replacement using | as its delimiter.
${bench_args}           -H 127.0.0.1 -w 3000.0,80% -c 5000.0,100% -p 5 -t 30


*** Test Cases ***
EALLOC1
    [Documentation]    Scenario: count the heap allocations done while processing check results
    ...    Given an engine with 50 hosts and 1000 services, all of them passive
    ...    And heaptrack attached to the running centengine
    ...    Then ${nb_checks} check results carrying a realistic output are processed
    ...    And the trace is complete once heaptrack has been detached
    [Tags]    broker    engine    bench    unstable
    Ctn Clear Retention
    Ctn Clear Logs

    Ctn Config Engine    ${1}    ${50}    ${20}
    # Everything is passive: centengine forks no plugin at all, so the only heap
    # activity left is the one of engine itself. Without this, heaptrack would also
    # have to be kept away from the thousands of forked check commands.
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Set Hosts Passive    ${0}    host_.*
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Config Broker Sql Output    central    unified_sql

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Alloc Bench Wait For Attach

    ${send_start}    Get Current Date
    Ctn Process Service Check Result
    ...    host_1    service_1    1    ${check_output}    config0    0    ${nb_checks}
    ${send_end}    Get Current Date
    ${elapsed}    Subtract Date From Date    ${send_end}    ${send_start}
    Log To Console    ${nb_checks} check results submitted in ${elapsed}s

    # The submitted outputs are suffixed with _<index> by the keyword, and that suffix
    # lands at the end of the perf data. Waiting for the last one guarantees the whole
    # batch has really been processed, so both runs cover the exact same work.
    ${last}    Evaluate    ${nb_checks} - 1
    Wait Until Keyword Succeeds    10 min    5 s    Ctn Last Check Result Is Processed    ${last}

    Ctn Alloc Bench Wait For Detach    All ${nb_checks} check results processed.

EALLOC2
    [Documentation]    Scenario: count the heap allocations of the nominal, active check profile
    ...    Given an engine with 50 hosts and 1000 services, all actively checked once a second
    ...    And heaptrack attached to the running centengine
    ...    Then checks run for ${duration} and the allocations are attributed per stack
    ...    And the count per check is derived from the parse_check_output ratio
    [Tags]    broker    engine    bench    unstable
    Ctn Run Active Check Bench    ${bench_plugin}

EALLOC3
    [Documentation]    Scenario: same as EALLOC2, with a command line the length of a real check
    ...    Given an engine with 50 hosts and 1000 services, all actively checked once a second
    ...    And every check command carrying ten arguments instead of none
    ...    And heaptrack attached to the running centengine
    ...    Then checks run for ${duration} and the allocations are attributed per stack
    ...    And the cost of parsing an argument vector can be read against EALLOC2
    ...
    ...    EALLOC2 runs a bare plugin path, which is not what production looks like: an
    ...    expanded check command carries ten or so arguments, and misc::command_line
    ...    rebuilds its std::vector<char*> from an empty capacity at every exec. Measuring
    ...    the fork path on EALLOC2 alone therefore understates it. Run both against the
    ...    same binary and the difference is the price of the argument vector.
    ...
    ...    Careful when reading the difference: a longer command line also makes macro
    ...    expansion produce a longer string, so the two runs differ in more than argv.
    ...    Attribution per stack separates them — misc::command_line::parse on one side,
    ...    the macro functions on the other — a comparison of totals would not.
    [Tags]    broker    engine    bench    unstable
    Ctn Run Active Check Bench    ${bench_plugin} ${bench_args}

EALLOC4
    [Documentation]    Scenario: count the heap allocations of cbd while it stores results
    ...    Given an engine with 50 hosts and 1000 services, all actively checked once a second
    ...    And heaptrack attached to the central cbd instead of to centengine
    ...    Then checks run for ${duration} and the allocations are attributed per stack
    ...    And the perf data parsing path of unified_sql is the dominant one
    ...
    ...    The three profiles above trace centengine, which never parses a perf data string:
    ...    common::perfdata::parse_perfdata is called by unified_sql and by the lua module,
    ...    both of them living in cbd, and by the agent. Engine's own parse_perfdata, in
    ...    anomalydetection.cc, is an unrelated namesake. So anything done to the perf data
    ...    parser is invisible to EALLOC1-3 by construction, and this profile is where it
    ...    shows: cbd stores the two metrics of every check result of every service.
    ...
    ...    Same workload as EALLOC2 on purpose -- same plugin, same bare command line -- so
    ...    that the two traces answer "who allocates on this workload, Engine or Broker?"
    [Tags]    broker    engine    bench    unstable
    Ctn Run Active Check Bench    ${bench_plugin}    traced=broker


*** Keywords ***
Ctn Run Active Check Bench
    [Documentation]    Run the active check profile with the given command line, pausing
    ...    twice so that heaptrack can be attached and detached.
    ...
    ...    ${traced} selects which daemon heaptrack is pointed at: "engine" for centengine,
    ...    the scheduling and forking side, or "broker" for the central cbd, the storing
    ...    side. The workload is identical either way, which is what lets the two profiles
    ...    be read against each other.
    [Arguments]    ${command_line}    ${traced}=engine

    Ctn Clear Retention
    Ctn Clear Logs

    Ctn Config Engine    ${1}    ${50}    ${20}
    # Checks stay active: this is the production profile. The plugin is reduced to a
    # single echo so that the fork+exec neither throttles the check rate nor drowns
    # the profile of centengine itself; it ignores whatever arguments it is given,
    # which is what lets the caller lengthen the command line freely. Written after
    # Ctn Config Engine, which recreates that directory.
    Create File    ${bench_plugin}    \#!/bin/sh\necho "${check_output}"\n
    Run    chmod +x ${bench_plugin}
    # Every command is rewritten, not just the command_<n> ones that
    # Ctn Engine Config Change Command reaches: the host commands are named checkh<n>
    # and would keep running check.pl. The connector lines have to go as well, or the
    # 25 commands bound to the Perl Connector feed a shell script to an embedded perl
    # interpreter, fail, and bottleneck the scheduler on max_concurrent_checks.
    Run
    ...    sed -i -E "s|^[[:space:]]*command_line[[:space:]]+.*| command_line ${command_line}|; /^[[:space:]]*connector[[:space:]]/d" ${EtcRoot}/centreon-engine/config0/commands.cfg
    # check_interval is 1 in the generated services, so interval_length=1 turns it into
    # one second instead of one minute. max_concurrent_checks stays at its default 400,
    # which is what caps the fork rate.
    Ctn Engine Config Set Value    ${0}    interval_length    1
    # The default configuration logs every scheduler call (functions=trace, 64% of the
    # lines) and every published event (neb=debug, 22%): 340 MB of log in 20 minutes,
    # which throttles engine and drowns the allocation profile we are after.
    Ctn Engine Config Set Value    ${0}    log_level_functions    error
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    module0    neb    error
    Ctn Config BBDO3    1
    Ctn Config Broker Sql Output    central    unified_sql

    # The configuration is checked here rather than by querying engine once started:
    # under this load engine saturates max_concurrent_checks and its gRPC server stops
    # answering altogether (Deadline Exceeded), so any runtime check would fail on a
    # perfectly healthy run. The rewrite above is deterministic, checking the file is
    # just as conclusive and costs nothing.
    ${cmds}    Get File    ${EtcRoot}/centreon-engine/config0/commands.cfg
    Should Not Contain    ${cmds}    check.pl
    Should Not Contain    ${cmds}    connector
    # The whole command line, arguments included: what the variant is about is their
    # number, so a run that silently lost them would measure EALLOC2 twice.
    Should Contain    ${cmds}    ${command_line}

    Ctn Start Broker
    Ctn Start Engine
    # Not using Ctn Wait For Engine To Be Ready: it greps check_for_external_commands(),
    # a functions/trace message, and that logger is precisely the one turned off above.
    # This is just enough for the scheduler to reach its cruising rate.
    Sleep    15s

    Ctn Alloc Bench Wait For Attach    traced=${traced}

    Log To Console    Running active checks for ${duration}...
    Sleep    ${duration}

    Ctn Alloc Bench Wait For Detach    Done, ${duration} of active checks.
    ...    traced=${traced}

Ctn Alloc Bench Wait For Attach
    [Documentation]    Publish the pid of the daemon being traced, then wait until heaptrack
    ...    has been attached to it. The pid goes into the ready sentinel and not only to the
    ...    console, which is what lets a driver do the attaching.
    ...
    ...    b1 is the central cbd, the one carrying unified_sql; b2 is the rrd one and parses
    ...    nothing.
    [Arguments]    ${traced}=engine
    # Asking robot for the pid of the process it started: "pgrep -f /usr/sbin/centengine"
    # would also match the shell robot forks to run it.
    IF    "${traced}" == "broker"
        ${pid}    Get Process Id    b1
        ${what}    Set Variable    the central cbd
    ELSE
        ${pid}    Ctn Get Engine Pid    e0
        ${what}    Set Variable    centengine
    END
    Create File    ${ready_file}    ${pid}
    Log To Console    \n\n=========================================================
    Log To Console    ${what} is ready, pid ${pid}
    Log To Console    Attach heaptrack, then release the test:
    # -o has to come before -p: once heaptrack 1.5 has parsed -p it stops reading
    # the remaining options and silently writes to the current directory.
    Log To Console    \ \ heaptrack -o /root/.cache/heaptrack/<trace-name> -p ${pid}
    Log To Console    \ \ touch ${go_file}
    Log To Console    =========================================================\n
    Wait Until Created    ${go_file}    timeout=${attach_timeout}
    RETURN    ${pid}

Ctn Alloc Bench Wait For Detach
    [Documentation]    Announce that the workload is over, stop centengine so that heaptrack
    ...    closes its trace, then wait until whoever drives it is done reading that trace.
    ...
    ...    Stopping centengine here rather than letting the teardown do it is not a detail:
    ...    heaptrack 1.5 cannot be detached from a live process. Signalling the tracer kills
    ...    it without its cleanup, and its cleanup -- what Ctrl-C triggers -- calls
    ...    heaptrack_stop() through gdb, which trips an assertion in libheaptrack and aborts
    ...    the debuggee anyway. Letting the traced process exit is the only supported way to
    ...    get a complete trace, so the test does exactly that.
    [Arguments]    ${what_happened}    ${traced}=engine
    Create File    ${done_file}    ${what_happened}
    Log To Console    \n\n=========================================================
    Log To Console    ${what_happened}
    Log To Console    Stopping the traced daemon: heaptrack will flush its trace and exit.
    Log To Console    Once it has, let the test end:
    Log To Console    \ \ rm ${go_file}
    Log To Console    =========================================================\n
    # Engine goes down first in both cases. When cbd is the traced one that order also
    # matters for the measurement: the events already in flight have to be stored while
    # the tracer is still attached, or the tail of the workload would fall outside the
    # trace and the two runs would not cover the same work.
    Ctn Stop Engine
    # no_rrd_test for the same reason the suite teardown passes it: on this workload
    # the RRD log rightfully complains about metrics sent in the past, which has
    # nothing to do with what is measured. Without it the stop fails here, after the
    # trace is already complete, and reports a red test on a good measurement.
    IF    "${traced}" == "broker"    Ctn Kindly Stop Broker    no_rrd_test=True
    Wait Until Removed    ${go_file}    timeout=${attach_timeout}

Ctn Alloc Bench Setup
    [Documentation]    Stop whatever is running and clear the sentinels, so that a file left
    ...    behind by an interrupted run cannot release the next one before its time.
    Ctn Stop Processes
    Remove File    ${ready_file}
    Remove File    ${go_file}
    Remove File    ${done_file}

Ctn Alloc Bench Teardown
    [Documentation]    Stop everything without checking the rrd log: the check results are
    ...    all submitted with the very same timestamp, so rrd rightfully complains about
    ...    metrics sent in the past. That has nothing to do with what is measured here.
    Ctn Stop Engine Broker And Save Logs    no_rrd_test=True
    # no_rrd_test only skips the check, it does not clean the log: the next test running
    # the usual teardown would fail on errors it did not produce.
    Remove File    ${rrdLog}
    Remove File    ${ready_file}
    Remove File    ${go_file}
    Remove File    ${done_file}

Ctn Last Check Result Is Processed
    [Documentation]    Fail unless service_1 of host_1 carries the perf data of the last
    ...    submitted check result.
    [Arguments]    ${idx}
    ${svc}    Ctn Get Service Info Grpc    ${1}    ${1}
    Should End With    ${svc}[perfData]    _${idx}

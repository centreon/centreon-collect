*** Settings ***
Documentation       RRD retention buffer benchmark.
...
...    Measures the throughput and merge latency of the RRD stream retention buffer
...    introduced in broker/rrd.  The test:
...
...    1. Starts Engine + Broker (BBDO3) and waits for real service metrics to
...       accumulate in the DB with corresponding .rrd files.
...    2. Injects ${N_OLD_POINTS} old-timestamped pb_metric events per metric
...       plus one current-time event per metric to trigger the junction merge.
...    3. Waits for all ``RRD: merging … buffered points for metric`` messages.
...    4. Logs injection throughput and end-to-end latency.
...
...    The injected old timestamps run from (now - N_OLD_POINTS*STEP - STEP) to
...    (now - STEP - 1), i.e. all strictly older than one step.  Engine's own
...    check results fall in the last STEP seconds, so there is no timestamp
...    collision with the buffer.  The merge itself creates a fresh .rrd via a
...    temp file + atomic rename, so no ``rrd_update`` timeline errors occur.
...
...    To compare before / after the retention buffer change:
...    - Old binary: old data is rejected and the merge messages never appear → FAIL.
...    - New binary: merge completes and figures are printed → PASS.
...
...    Run from the tests/ directory (where resources/ lives):
...      robot benchmarks/rrd_retention_bench.robot
...    Override variables as needed:
...      robot -v N_METRICS:10 -v N_OLD_POINTS:1440 benchmarks/rrd_retention_bench.robot

Resource    ../resources/import.resource
Library     ../resources/bbdo_injector.py
Library     robot_bench.py

Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Stop Processes
# no_rrd_test=True: the merge rewrites the file atomically so no update-error
# is produced by the benchmark itself, but Engine may race with the injector
# on the same step boundary; skip the strict RRD duplicate check.
Test Teardown    Ctn Stop Engine Broker And Save Logs    no_rrd_test=True


*** Variables ***
# Number of metrics to benchmark (each gets its own set of old points).
${N_METRICS}        5
# Old points per metric: 720 × 60 s = 12 hours of back-fill history.
${N_OLD_POINTS}     720
# Step in seconds — must match Engine's check_interval (default 60 s).
${STEP}             60
# RRD file length in seconds (6 months = 15 552 000 s).
${RRD_LEN}          15552000
# RRD broker input port (direct BROKER-to-BROKER connection, bypasses the
# central broker's BBDO3 Engine config handshake).
${BROKER_PORT}      5670
# Max seconds to wait for Engine to produce N_METRICS metrics with .rrd files.
${SETUP_TIMEOUT}    120
# Max seconds to wait for all merges to complete after injection.
${MERGE_TIMEOUT}    120
# Campaign the result is filed under. Empty means the current git branch, which is what
# ./bench.py does when it is not given a --label either.
${label}            ${EMPTY}


*** Test Cases ***
BENCH_RRD_METRIC_RETENTION
    [Documentation]    Benchmark: inject 12 h of back-fill data through the
    ...    retention buffer and measure merge latency.
    ...
    ...    Injects ${N_OLD_POINTS} old-timestamped pb_metric events per metric
    ...    (${N_METRICS} metrics) via BBDO v3 directly to the central broker, then
    ...    one current-time event per metric to trigger the junction merge.
    ...    Reports injection throughput and end-to-end merge latency.
    [Tags]    rrd    retention    benchmark    bbdo3    performance

    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    # INFO level is sufficient: "merging N buffered points" is logged at INFO.
    Ctn Broker Config Log    rrd    rrd    info
    Ctn Broker Config Flush Log    rrd    0
    # Reduce central-broker SQL noise to errors only.
    Ctn Broker Config Log    central    sql    error

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${connected}    Ctn Check Connections
    Should Be True    ${connected}    Engine and Broker not connected

    # Wait until at least N_METRICS metrics have both DB entries and .rrd files.
    # Engine generates these as it processes service check results.
    ${metrics}    Create List
    FOR    ${i}    IN RANGE    ${SETUP_TIMEOUT}
        ${metrics}    Ctn Get Metrics To Delete    ${N_METRICS}
        IF    len(${metrics}) >= ${N_METRICS}    BREAK
        Sleep    1s
    END
    ${n_found}    Get Length    ${metrics}
    Skip If    ${n_found} < ${N_METRICS}
    ...    Only ${n_found}/${N_METRICS} metrics available after ${SETUP_TIMEOUT} s — check Engine config.
    ${metrics}    Get Slice From List    ${metrics}    0    ${N_METRICS}
    Log To Console    \nBenchmark metric IDs: ${metrics}

    # -----------------------------------------------------------------------
    # Injection phase
    # -----------------------------------------------------------------------
    # The injector sends N_OLD_POINTS events whose timestamps range from
    # (now - N_OLD_POINTS*STEP - STEP) to (now - STEP - 1), i.e. all older
    # than one step.  A final current-time event then triggers the junction
    # merge for each metric.
    ${inject_start}    Get Current Date

    ${inject_result}    Ctn Inject Metric Events
    ...    host=127.0.0.1
    ...    port=${BROKER_PORT}
    ...    metric_ids=${metrics}
    ...    rrd_len=${RRD_LEN}
    ...    step=${STEP}
    ...    n_old_points=${N_OLD_POINTS}

    ${n_injected}    Set Variable    ${inject_result}[n_injected]
    ${inject_s}    Set Variable    ${inject_result}[inject_s]
    ${ev_per_s}    Evaluate    round(${inject_result}[events_per_second], 0)
    Log To Console    Injected ${n_injected} events in ${inject_s} s (${ev_per_s} events/s)

    # -----------------------------------------------------------------------
    # Merge completion phase
    # -----------------------------------------------------------------------
    # The RRD stream logs at INFO level when it starts merging:
    #   "RRD: merging <N> buffered points for metric <id> into '<path>'"
    # We match the prefix (substring match) so the trailing path is ignored.
    FOR    ${m}    IN    @{metrics}
        Log To Console    Step ${m}
        ${content}    Create List
        ...    RRD: merging ${N_OLD_POINTS} buffered points for metric ${m}
        ${found}    Ctn Find In Log With Timeout
        ...    ${rrdLog}    ${inject_start}    ${content}    ${MERGE_TIMEOUT}
        Should Be True    ${found}
        ...    Merge did not complete for metric ${m} within ${MERGE_TIMEOUT} s
        Log To Console    Metric ${m}: merge done
    END

    ${merge_end}    Get Current Date
    ${total_s}    Subtract Date From Date    ${merge_end}    ${inject_start}

    # -----------------------------------------------------------------------
    # Results summary
    # -----------------------------------------------------------------------
    ${total_points}    Evaluate    ${N_OLD_POINTS} * ${N_METRICS}
    ${history_s}    Evaluate    ${N_OLD_POINTS} * ${STEP}
    ${history_h}    Evaluate    round(${history_s} / 3600, 1)
    ${merge_tp}    Evaluate    round(${total_points} / ${total_s}, 0)

    Log To Console    \n=== RRD Retention Buffer Benchmark Results ===
    Log To Console    Metrics\ \ \ \ \ \ \ \ \ \ \ : ${N_METRICS}
    Log To Console    Old points/metric : ${N_OLD_POINTS}
    Log To Console    Step\ \ \ \ \ \ \ \ \ \ \ \ \ \ : ${STEP} s
    Log To Console    History depth\ \ \ \ \ : ${history_s} s (${history_h} h)
    Log To Console    Total old points\ \ : ${total_points}
    Log To Console    ---
    Log To Console    Injection events\ \ : ${n_injected}
    Log To Console    Injection time\ \ \ \ : ${inject_s} s
    Log To Console    Injection speed\ \ \ : ${ev_per_s} events/s
    Log To Console    ---
    Log To Console    Total latency\ \ \ \ \ : ${total_s} s
    Log To Console    Merge throughput\ \ : ${merge_tp} points/s
    Log To Console    ===============================================

    # Console output does not survive the terminal, and comparing two versions six months
    # apart is the whole point of a benchmark, so the same figures go to results/bench.db
    # like every other benchmark of this directory. The sizes go into the parameters and
    # not into the metrics: they are what identifies the measured point, and what pairs
    # two runs when ./bench.py compare puts two campaigns side by side.
    ${campaign}    Set Variable If    "${label}" == "${EMPTY}"    ${None}    ${label}
    IF    $campaign is None
        ${campaign}    Ctn Bench Git Branch
    END
    ${figures}    Create Dictionary
    ...    injection_events=${n_injected}
    ...    injection_s=${inject_s}
    ...    injection_events_per_s=${ev_per_s}
    ...    merge_latency_s=${total_s}
    ...    merge_points_per_s=${merge_tp}
    ...    buffered_points=${total_points}
    ${params}    Create Dictionary
    ...    metrics=${N_METRICS}
    ...    old_points=${N_OLD_POINTS}
    ...    step=${STEP}
    ${run}    Ctn Bench Record Run    ${campaign}    rrd-retention    merge
    ...    ${figures}    ${params}
    Log To Console    Filed as run ${run} of campaign '${campaign}'

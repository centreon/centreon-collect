*** Settings ***
Documentation       There tests are about big metric values

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Test Clean


*** Variables ***
# Five metrics with plain values, deliberately not the 3.4e39 ones EBBM1 needs:
# those overflow a float and would fill the log with SQL range errors that have
# nothing to do with a cap. Written out rather than generated in a loop, because
# which metrics survive the cap is the whole point and has to be readable here.
#
# The two tests use disjoint metric names on purpose. EBBM2 asserts that a metric
# past the cap is *absent* from the metrics table, and that table is not emptied
# between runs: sharing names with EBBM3, which stores all five of them, would
# make EBBM2 fail on what an earlier campaign left behind.
${capped_metrics}       metrics for the cap | cap0=1 cap1=2 cap2=3 cap3=4 cap4=5
${uncapped_metrics}     metrics uncapped | free0=1 free1=2 free2=3 free3=4 free4=5

# What parse_perfdata says when it stops early. The announced count comes from
# the number of '=' it counted, so it is 5 here.
${truncation_log}       perfdata truncated to 3 metrics, the output announces about 5


*** Test Cases ***
EBBM1
    [Documentation]    A service status contains metrics that do not fit in a float number.
    [Tags]    broker    engine    services    unified_sql
    Ctn Config Engine    ${1}    ${1}    ${1}
    # We want all the services to be passive to avoid parasite checks during our test.
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    tcp    error
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    perfdata    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    ${start}    Get Current Date
    ${start_broker}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    FOR    ${i}    IN RANGE    ${10}
        Ctn Process Service Check Result With Big Metrics
        ...    host_1    service_1    1
        ...    Big Metrics    ${10}
        Sleep    1s
    END
    ${content}    Create List
    ...    Out of range value for column 'current_value'
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    not ${result}    It shouldn't be forbidden to store big metrics in the database.

EBBM2
    [Documentation]    Scenario: an output carrying more metrics than the cap allows is truncated
    ...    Given a central broker whose unified_sql output carries max_perfdata=3
    ...    And its perfdata logger low enough to let a warning through
    ...    When a passive result carrying the five metrics m0 to m4 is submitted
    ...    Then the central log announces the truncation to three of about five
    ...    And the service carries m0, m1 and m2 in the metrics table
    ...    And m4 never reaches it
    [Tags]    broker    engine    services    unified_sql
    ${start}    Ctn Start With Metric Cap    3

    Ctn Process Service Check Result    host_1    service_1    1    ${capped_metrics}

    ${content}    Create List    ${truncation_log}
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should have said it truncated the perf data to 3 metrics.

    # The leading metrics, and those specifically: a graph fed by a capped output
    # stays continuous only if the same ones survive from a check to the next.
    ${kept}    Create List    cap0    cap1    cap2
    ${result}    Ctn Compare Metrics Of Service    ${1}    ${kept}    60
    Should Be True    ${result}    The service should carry m0, m1 and m2.

    # Asked only once the three above are in, so that what is observed is really
    # an absence and not the insertion latency.
    ${dropped}    Create List    cap4
    ${result}    Ctn Compare Metrics Of Service    ${1}    ${dropped}    20
    Should Not Be True    ${result}    m4 is past the cap and should never have been stored.

EBBM3
    [Documentation]    Scenario: without the option, every metric of the output is kept
    ...    Given a central broker whose unified_sql output carries no max_perfdata
    ...    When a passive result carrying the five metrics m0 to m4 is submitted
    ...    Then the service carries all five of them
    ...    And nothing is said about any truncation
    ...
    ...    The witness of EBBM2, and not a formality: a run where the metrics never
    ...    arrive at all, or where the submitted output holds three of them, would
    ...    satisfy EBBM2 just as well. This is what ties the truncation to the
    ...    option rather than to a broken pipeline.
    [Tags]    broker    engine    services    unified_sql
    ${start}    Ctn Start With Metric Cap

    Ctn Process Service Check Result    host_1    service_1    1    ${uncapped_metrics}

    ${all}    Create List    free0    free1    free2    free3    free4
    ${result}    Ctn Compare Metrics Of Service    ${1}    ${all}    60
    Should Be True    ${result}    Uncapped, the service should carry the five metrics.

    ${content}    Create List    perfdata truncated
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    10
    Should Be True    not ${result}    Nothing should have been truncated without the option.


*** Keywords ***
Ctn Start With Metric Cap
    [Documentation]    Configure and start engine and broker for the cap tests. The cap is
    ...    written into the unified_sql output when one is given, and left out entirely
    ...    otherwise, so that the two tests differ by that option and by nothing else.
    ...
    ...    Returns the date the daemons were started, for the log searches.
    [Arguments]    ${cap}=${EMPTY}
    Ctn Config Engine    ${1}    ${1}    ${1}
    # Passive, so that no scheduled check races with the result the test submits.
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    tcp    error
    Ctn Broker Config Log    central    sql    error
    # perfdata has to let a warning through, or the truncation message is never
    # emitted: that logger is the one parse_perfdata complains to, and the test
    # configurations leave it at error.
    Ctn Broker Config Log    central    perfdata    warning
    Ctn Config Broker Sql Output    central    unified_sql
    # After Ctn Config Broker Sql Output, which rewrites that whole output.
    IF    "${cap}" != "${EMPTY}"
        Ctn Broker Config Output Set    central    central-broker-unified-sql    max_perfdata    ${cap}
    END
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    RETURN    ${start}

Ctn Test Clean
    [Documentation]    Stop engine and broker, then save logs if the test failed.
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Ctn Save Logs If Failed

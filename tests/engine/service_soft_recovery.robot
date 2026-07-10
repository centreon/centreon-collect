*** Settings ***
Documentation       Centreon Engine service soft/hard state transition tests.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
ESVC_OK_SOFT_RECOVERY
    [Documentation]    Scenario: a service in OK HARD goes to WARNING SOFT (one check,
    ...    max_check_attempts=3), then receives OK check results.
    ...
    ...    This validates the Nagios state-type model:
    ...    - Recovering from a SOFT problem is a "soft recovery": the first OK check leaves the
    ...      service in an OK SOFT state (event handlers run, but NO recovery notification).
    ...    - The service only stabilizes to OK HARD on the following OK check.
    [Tags]    engine
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_events    info
    Ctn Engine Config Set Value    ${0}    log_flush_period    ${0}

    # Drive the service state deterministically through passive check results
    # (active checks disabled so nothing competes with the results we push).
    Ctn Set Services Passive    ${0}    service_1

    Ctn Clear Retention
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Start Broker
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Establish a clean baseline: host_1 UP and service_1 in OK HARD. An initial OK check is
    # always HARD, so this puts service_1 in a well-defined OK HARD starting state. The
    # WARNING;SOFT;1 alert below then confirms that baseline: a soft problem restarts the attempt
    # counter at 1 only from a clean (HARD) state.
    Ctn Process Host Check Result    host_1    0    host_1 UP
    Ctn Process Service Check Result    host_1    service_1    0    output ok for service_1

    # One WARNING check => service_1 enters WARNING SOFT at attempt 1 of 3 (this "attempt 1" is
    # the proof of the OK HARD baseline).
    Ctn Process Service Check Result    host_1    service_1    1    output warning for service_1
    ${content}    Create List    SERVICE ALERT: host_1;service_1;WARNING;SOFT;1
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    service_1 did not reach WARNING SOFT at attempt 1 (unexpected initial state)

    # First OK check => SOFT RECOVERY. Per the Nagios state-type model, a service recovering from
    # a soft problem returns to OK but stays in a SOFT state (no recovery notification is sent).
    # The SERVICE ALERT line carries the state type, so the engine log is the ground truth here.
    # Use a fresh timestamp so the OK searches match only the recovery, never the baseline OK.
    ${soft_recovery}    Ctn Get Round Current Date
    Ctn Process Service Check Result    host_1    service_1    0    output ok for service_1
    ${content}    Create List    SERVICE ALERT: host_1;service_1;OK;SOFT
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${soft_recovery}    ${content}    30
    Should Be True    ${result}    service_1 soft recovery must be logged OK SOFT (Nagios statetypes)

    # Second OK check => the service stabilizes to OK HARD.
    ${hard_recovery}    Ctn Get Round Current Date
    Ctn Process Service Check Result    host_1    service_1    0    output ok for service_1
    ${content}    Create List    SERVICE ALERT: host_1;service_1;OK;HARD
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${hard_recovery}    ${content}    30
    Should Be True    ${result}    service_1 did not stabilize to OK HARD after a second OK check

    Ctn Stop Engine
    Ctn Kindly Stop Broker

*** Settings ***
Documentation       Engine/Broker tests verifying severity presence in the broker cache
...                 via gRPC, using the centralized (BBDO3) configuration mode.
...
...                 These tests are the severity counterpart of the tag cache tests
...                 (centralized-tags-cache.robot).  They verify that the broker cache
...                 correctly tracks severities across multiple pollers: a severity shared
...                 by N pollers must remain in the cache as long as at least one poller
...                 defines it, and must disappear only when all pollers remove it.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
BECSEV4
    [Documentation]    Feature: Severity presence in Broker gRPC cache with centralized configuration
    ...
    ...    Background:
    ...        Given 4 pollers are configured with 5 hosts each (20 total) and 20 services per host
    ...        And each poller defines 2 severities: id=1/SERVICE/level=1 and id=2/HOST/level=2
    ...        And severity 1 is assigned to all services, severity 2 to all hosts
    ...        And Broker and Engine are started in centralized (BBDO3) mode
    ...
    ...    Scenario: Phase 1 — All pollers active
    ...        Then the broker gRPC cache contains severity (1, SERVICE, level=1)
    ...        And the broker gRPC cache contains severity (2, HOST, level=2)
    ...
    ...    Scenario: Phase 2 — Severity removed from poller 3
    ...        When severities are removed from poller 3 and broker is notified
    ...        Then the broker cache STILL contains severity (1, SERVICE) (pollers 0-2 have it)
    ...        And the broker cache STILL contains severity (2, HOST)
    ...
    ...    Scenario: Phase 3 — Severity removed from poller 2
    ...        When severities are removed from poller 2 and broker is notified
    ...        Then the broker cache STILL contains severity (1, SERVICE) (pollers 0-1 have it)
    ...        And the broker cache STILL contains severity (2, HOST)
    ...
    ...    Scenario: Phase 4 — Severity removed from all remaining pollers
    ...        When severities are removed from pollers 0 and 1 and broker is notified
    ...        Then the broker cache contains 0 severities
    ...
    ...    Scenario: Phase 5 — Severity cache is empty (no orphan entries)
    ...        Then GetSeverities returns an empty list
    [Tags]    broker    engine    cache    severities

    Ctn Config Centralized Engine    ${4}    ${20}    ${20}

    FOR    ${i}    IN RANGE    4
        Ctn Create Severities File    ${i}    ${2}
        Ctn Config Engine Add Cfg File    ${i}    severities.cfg
        Ctn Add Severity To All Services    ${i}    1
        Ctn Add Severity To All Hosts    ${i}    2
    END

    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${4}
    Ctn Config BBDO3    ${4}
    Ctn Broker Config Log    central    cache    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Db    severities

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine Configuration To Be Applied    ${start}    ${0}    ${4}

    # Phase 1: verify both severities are present in the broker cache
    Log To Console    Phase 1: verifying severities in broker cache
    ${result}    Ctn Check Severity In Cache With Timeout    51001    1    0    1    60
    Should Be True    ${result}    Phase 1: severity (1, SERVICE, level=1) should be in broker cache
    ${result}    Ctn Check Severity In Cache With Timeout    51001    2    1    2    60
    Should Be True    ${result}    Phase 1: severity (2, HOST, level=2) should be in broker cache

    # Phase 2: remove severities from poller 3 — both must still be in cache
    Log To Console    Phase 2: removing severities from poller 3
    Ctn Remove Severities From Services    ${3}
    Ctn Remove Severities From Hosts    ${3}
    Ctn Create Severities File    ${3}    ${0}
    ${start2}    Ctn Get Round Current Date
    Ctn Wait For Engine Configuration To Be Applied    ${start2}    ${3}    ${4}

    ${result}    Ctn Check Severity In Cache With Timeout    51001    1    0    1    60
    Should Be True    ${result}    Phase 2: severity (1, SERVICE) should persist after removing poller 3
    ${result}    Ctn Check Severity In Cache With Timeout    51001    2    1    2    60
    Should Be True    ${result}    Phase 2: severity (2, HOST) should persist after removing poller 3

    # Phase 3: remove severities from poller 2 — both must still be in cache
    Log To Console    Phase 3: removing severities from poller 2
    Ctn Remove Severities From Services    ${2}
    Ctn Remove Severities From Hosts    ${2}
    Ctn Create Severities File    ${2}    ${0}
    ${start3}    Ctn Get Round Current Date
    Ctn Wait For Engine Configuration To Be Applied    ${start3}    ${2}    ${3}

    ${result}    Ctn Check Severity In Cache With Timeout    51001    1    0    1    60
    Should Be True    ${result}    Phase 3: severity (1, SERVICE) should persist after removing poller 2
    ${result}    Ctn Check Severity In Cache With Timeout    51001    2    1    2    60
    Should Be True    ${result}    Phase 3: severity (2, HOST) should persist after removing poller 2

    # Phase 4: remove from pollers 0 and 1 — both must now be gone
    Log To Console    Phase 4: removing severities from all remaining pollers
    Ctn Remove Severities From Services    ${0}
    Ctn Remove Severities From Hosts    ${0}
    Ctn Create Severities File    ${0}    ${0}
    ${start4}    Ctn Get Round Current Date
    Ctn Wait For Engine Configuration To Be Applied    ${start4}    ${0}    ${1}
    Ctn Remove Severities From Services    ${1}
    Ctn Remove Severities From Hosts    ${1}
    Ctn Create Severities File    ${1}    ${0}
    ${start5}    Ctn Get Round Current Date
    Ctn Wait For Engine Configuration To Be Applied    ${start5}    ${1}    ${2}

    ${result}    Ctn Check Severity In Cache With Timeout    51001    1    0    ${None}    60
    Should Be True    ${result}    Phase 4: severity (1, SERVICE) should be gone after removing all pollers
    ${result}    Ctn Check Severity In Cache With Timeout    51001    2    1    ${None}    60
    Should Be True    ${result}    Phase 4: severity (2, HOST) should be gone after removing all pollers

    # Phase 5: the severity cache must be completely empty
    Log To Console    Phase 5: verifying severity cache is empty via GetSeverities
    ${result}    Ctn Check Severities Empty With Timeout    51001    60
    Should Be True    ${result}    Phase 5: broker severity cache is not empty after all severities were removed

    Ctn Stop Engine
    Ctn Kindly Stop Broker


BECSEV5
    [Documentation]    Feature: Severity level change is reflected in the Broker gRPC cache
    ...
    ...    Background:
    ...        Given 4 pollers configured with 5 hosts each (20 total) and 20 services per host
    ...        And each poller defines severity id=1 (SERVICE, level=1) and id=2 (HOST, level=2)
    ...        And severities assigned to all hosts and services
    ...
    ...    Scenario: Severity levels are updated in the broker cache after modification
    ...        Given the initial state has severity (1, SERVICE, level=1) and (2, HOST, level=2)
    ...        When all 4 pollers update their severities to level=4 (SERVICE) and level=5 (HOST)
    ...        Then broker GetSeverities returns (1, SERVICE, level=4) and (2, HOST, level=5)
    [Tags]    broker    engine    cache    severities

    Ctn Config Centralized Engine    ${4}    ${20}    ${20}

    FOR    ${i}    IN RANGE    4
        Ctn Create Severities File    ${i}    ${2}
        Ctn Config Engine Add Cfg File    ${i}    severities.cfg
        Ctn Add Severity To All Services    ${i}    1
        Ctn Add Severity To All Hosts    ${i}    2
    END

    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${4}
    Ctn Config BBDO3    ${4}
    Ctn Broker Config Log    central    cache    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Db    severities

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine Configuration To Be Applied    ${start}    ${0}    ${4}

    # Verify initial state
    ${result}    Ctn Check Severity In Cache With Timeout    51001    1    0    1    60
    Should Be True    ${result}    Initial state: severity (1, SERVICE, level=1) should be in cache

    # Update severity levels on all pollers (level_offset=3 → SERVICE level=4, HOST level=5)
    Log To Console    Updating severity levels to 4 (SERVICE) and 5 (HOST) on all pollers
    FOR    ${i}    IN RANGE    4
        Ctn Create Severities File    ${i}    ${2}    ${1}    ${3}
        Ctn Notify Broker Of Engine Config Change    ${i}
    END

    # Broker cache must reflect the new levels
    ${result}    Ctn Check Severity In Cache With Timeout    51001    1    0    4    60
    Should Be True    ${result}    Updated level: severity (1, SERVICE) should have level=4 in broker cache
    ${result}    Ctn Check Severity In Cache With Timeout    51001    2    1    5    60
    Should Be True    ${result}    Updated level: severity (2, HOST) should have level=5 in broker cache

    Ctn Stop Engine
    Ctn Kindly Stop Broker


BECSEV6
    [Documentation]    Feature: GetSeverities gRPC returns correct content while severities are active
    ...
    ...    Background:
    ...        Given 4 pollers configured with 5 hosts each (20 total) and 20 services per host
    ...        And each poller defines severity id=1 (SERVICE, level=1) and id=2 (HOST, level=2)
    ...        And severities assigned to all hosts and services
    ...
    ...    Scenario: GetSeverities returns 2 entries with correct metadata when all pollers are active
    ...        When Broker and Engine are started and synchronized
    ...        Then GetSeverities returns exactly 2 entries
    ...        And severity (1, SERVICE, level=1) is present
    ...        And severity (2, HOST, level=2) is present
    [Tags]    broker    engine    cache    severities

    Ctn Config Centralized Engine    ${4}    ${20}    ${20}

    FOR    ${i}    IN RANGE    4
        Ctn Create Severities File    ${i}    ${2}
        Ctn Config Engine Add Cfg File    ${i}    severities.cfg
        Ctn Add Severity To All Services    ${i}    1
        Ctn Add Severity To All Hosts    ${i}    2
    END

    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${4}
    Ctn Config BBDO3    ${4}
    Ctn Broker Config Log    central    cache    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Db    severities

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine Configuration To Be Applied    ${start}    ${0}    ${4}

    # GetSeverities must return exactly 2 entries with correct (id, type, level)
    ${result}    Ctn Check Severities Count With Timeout    51001    2    60
    Should Be True    ${result}    GetSeverities should return exactly 2 entries when all pollers have severities

    ${result}    Ctn Check Severity In Cache With Timeout    51001    1    0    1    60
    Should Be True    ${result}    GetSeverities should contain (1, SERVICE, level=1)
    ${result}    Ctn Check Severity In Cache With Timeout    51001    2    1    2    60
    Should Be True    ${result}    GetSeverities should contain (2, HOST, level=2)

    Ctn Stop Engine
    Ctn Kindly Stop Broker


BECSEV7
    [Documentation]    Feature: Broker cache is repopulated after broker restart with severities active
    ...
    ...    Background:
    ...        Given 4 pollers configured with 5 hosts each (20 total) and 20 services per host
    ...        And each poller defines severity id=1 (SERVICE, level=1) and id=2 (HOST, level=2)
    ...        And severities assigned to all hosts and services
    ...
    ...    Scenario: After broker restart, GetSeverities returns correct data
    ...        Given broker and engine are started and synchronized
    ...        And GetSeverities returns 2 entries before broker stops
    ...        When broker is stopped and restarted (engine keeps running)
    ...        Then GetSeverities returns the same 2 entries after restart
    [Tags]    broker    engine    cache    severities

    Ctn Config Centralized Engine    ${4}    ${20}    ${20}

    FOR    ${i}    IN RANGE    4
        Ctn Create Severities File    ${i}    ${2}
        Ctn Config Engine Add Cfg File    ${i}    severities.cfg
        Ctn Add Severity To All Services    ${i}    1
        Ctn Add Severity To All Hosts    ${i}    2
    END

    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${4}
    Ctn Config BBDO3    ${4}
    Ctn Broker Config Log    central    cache    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Db    severities

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine Configuration To Be Applied    ${start}    ${0}    ${4}

    # Verify cache is correct before restart
    ${result}    Ctn Check Severities Count With Timeout    51001    2    60
    Should Be True    ${result}    Pre-restart: GetSeverities should return exactly 2 entries
    ${result}    Ctn Check Severity In Cache With Timeout    51001    1    0    1    60
    Should Be True    ${result}    Pre-restart: severity (1, SERVICE, level=1) should be in cache
    ${result}    Ctn Check Severity In Cache With Timeout    51001    2    1    2    60
    Should Be True    ${result}    Pre-restart: severity (2, HOST, level=2) should be in cache

    # Stop broker only; engine keeps running
    Log To Console    Stopping broker
    Ctn Kindly Stop Broker

    # Restart broker; engine reconnects and re-sends configuration
    Log To Console    Restarting broker
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Wait For Engine Configuration To Be Applied    ${start}    ${0}    ${4}

    # Cache must be fully repopulated after restart
    ${result}    Ctn Check Severities Count With Timeout    51001    2    60
    Should Be True    ${result}    Post-restart: GetSeverities should return exactly 2 entries
    ${result}    Ctn Check Severity In Cache With Timeout    51001    1    0    1    60
    Should Be True    ${result}    Post-restart: severity (1, SERVICE, level=1) should be in cache
    ${result}    Ctn Check Severity In Cache With Timeout    51001    2    1    2    60
    Should Be True    ${result}    Post-restart: severity (2, HOST, level=2) should be in cache

    Ctn Stop Engine
    Ctn Kindly Stop Broker

*** Settings ***
Documentation       Centreon Engine restarts in centralized configuration with the runtime state Broker sends instead of retention.dat

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CERS1
    [Documentation]    Scenario: an Engine restarted without retention.dat gets its resources state back from Broker
    ...    Given Broker and Engine are started in centralized configuration, with passive services
    ...    And service_1 of host_1 is CRITICAL HARD
    ...    When Engine is stopped, its retention.dat is deleted and Engine is started again
    ...    Then Broker, finding the poller up to date, sends it its runtime state alone
    ...    And Engine restores the state of its hosts and services from that snapshot
    ...    And an OK result on service_1 is logged as a recovery from CRITICAL, which only a restored state allows
    [Tags]    engine    broker    centralized    retention    MON-187019
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_config    info
    Ctn Set Services Passive    ${0}    service_[0-9]+
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    cache    info
    Ctn Broker Config Log    central    sql    info

    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Db    hosts
    Ctn Clear Db    services
    Ctn Clear Db    resources
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Process Service Result Hard    host_1    service_1    2    output critical for service_1
    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    2    60    HARD
    Should Be True    ${result}    The service (host_1,service_1) should be CRITICAL HARD

    # Engine restarts with no retention file: the only state it can get back
    # is the one Broker sends at its connection.
    Ctn Stop Engine
    Remove File    ${VarRoot}/log/centreon-engine/config0/retention.dat
    ${restart}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${restart}    ${1}

    ${content}    Create List    is up to date, sending it its runtime state alone
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    60
    Should Be True    ${result}    Broker should send the runtime state to the poller it finds up to date
    # The retention phase runs before the log file is opened: its lines go to
    # the standard output of centengine.
    ${content}    Create List    retention.dat is not read: the runtime state comes from Broker
    ${result}    Ctn Find In Log With Timeout    ${VarRoot}/log/centreon-engine/config0/centengine-stdout.log    ${restart}    ${content}    30
    Should Be True    ${result}    Engine should say it does not read retention.dat in centralized configuration
    ${content}    Create List    Parsing of retention file failed
    ${result}    Ctn Find In Log With Timeout    ${VarRoot}/log/centreon-engine/config0/centengine-stdout.log    ${restart}    ${content}    5
    Should Not Be True    ${result}    Engine should not even try to read retention.dat
    ${content}    Create List    runtime state: 0 hosts and 0 services restored
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${restart}    ${content}    5
    Should Not Be True    ${result}    The snapshot should restore at least one resource
    ${content}    Create List    services restored from the Broker snapshot
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${restart}    ${content}    60
    Should Be True    ${result}    Engine should restore its resources state from the Broker snapshot

    # A recovery can only be logged from a restored CRITICAL: a fresh Engine
    # would see an OK result on a PENDING service and log nothing of the kind.
    ${ok}    Ctn Get Round Current Date
    Ctn Process Service Result Hard    host_1    service_1    0    output ok for service_1
    ${content}    Create List    SERVICE ALERT: host_1;service_1;OK;HARD;1;output ok for service_1
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${ok}    ${content}    60
    Should Be True    ${result}    The OK result should be logged as a recovery from the restored CRITICAL state

    [Teardown]    Ctn Stop Engine Broker And Save Logs

CERS2
    [Documentation]    Scenario: a Broker that lost a poller's configuration rebuilds it from Engine and still hands the poller its runtime state
    ...    Given Broker and Engine are started in centralized configuration, with passive services
    ...    And service_1 of host_1 is CRITICAL HARD
    ...    When Broker is stopped, its stored poller configuration (1.prot) is deleted and Broker is started again, Engine still running
    ...    Then Broker asks Engine for its configuration, stores it, rebuilds the poller's cache entries and restores their runtime from the database
    ...    And only then sends the poller its runtime state alone, which Engine applies
    [Tags]    engine    broker    centralized    retention    MON-187019
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Set Services Passive    ${0}    service_[0-9]+
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    cache    info
    Ctn Broker Config Log    central    sql    info

    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Db    hosts
    Ctn Clear Db    services
    Ctn Clear Db    resources
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Process Service Result Hard    host_1    service_1    2    output critical for service_1
    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    2    60    HARD
    Should Be True    ${result}    The service (host_1,service_1) should be CRITICAL HARD

    # Broker loses the stored configuration of the poller and restarts.
    Ctn Kindly Stop Broker
    Ctn Clear Prot Files    broker_only=${True}
    ${restart}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True

    ${content}    Create List    No known configuration for the poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    60
    Should Be True    ${result}    Broker should find no stored configuration for the poller and ask Engine for it
    ${content}    Create List    unified_sql: processing engine state
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    60
    Should Be True    ${result}    The configuration Engine sent back should reach the cache through the reference output
    ${content}    Create List    is up to date, sending it its runtime state alone
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    60
    Should Be True    ${result}    Broker should send the runtime state once the rebuilt configuration reached the cache
    ${content}    Create List    runtime state: 0 hosts and 0 services restored
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${restart}    ${content}    5
    Should Not Be True    ${result}    The snapshot should describe the resources the database knew
    ${content}    Create List    services restored from the Broker snapshot
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${restart}    ${content}    60
    Should Be True    ${result}    Engine should apply the snapshot sent after the configuration was rebuilt

    [Teardown]    Ctn Stop Engine Broker And Save Logs

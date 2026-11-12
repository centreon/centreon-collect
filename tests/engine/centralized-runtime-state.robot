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
    ...    When Engine is stopped, without writing any retention.dat but only its downtimes file, and started again
    ...    Then Broker, finding the poller up to date, sends it its runtime state alone
    ...    And Engine restores the state of its hosts and services from that snapshot
    ...    And service_1 gets back its check output, but not its long output
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

    # A long output after the escaped newline: the snapshot must carry the
    # first line only.
    Ctn Process Service Result Hard    host_1    service_1    2    output critical for service_1\\nlong output of service_1
    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    2    60    HARD
    Should Be True    ${result}    The service (host_1,service_1) should be CRITICAL HARD
    ${output}    Ctn Get Service Info Grpc    ${1}    ${1}
    Should Be Equal As Strings    ${output}[longPluginOutput]    long output of service_1    Engine should split the long output from the output

    # Engine restarts with no retention file: in centralized configuration it
    # does not write one, so the only state it can get back is the one Broker
    # sends at its connection.
    Ctn Stop Engine
    File Should Not Exist    ${VarRoot}/log/centreon-engine/config0/retention.dat
    ...    Engine should not write retention.dat in centralized configuration
    File Should Exist    ${VarRoot}/lib/centreon-engine/config0/downtimes.prot
    ...    Engine should save the downtimes it owns apart, in downtimes.prot
    ${restart}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${restart}    ${1}

    ${content}    Create List    is up to date, sending it its runtime state alone
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    60
    Should Be True    ${result}    Broker should send the runtime state to the poller it finds up to date
    # The retention phase runs before the log file is opened: its lines go to
    # the standard output of centengine.
    ${content}    Create List    no retention file to read: the runtime state comes from Broker
    ${result}    Ctn Find In Log With Timeout    ${VarRoot}/log/centreon-engine/config0/centengine-stdout.log    ${restart}    ${content}    30
    Should Be True    ${result}    Engine should say it has no retention.dat to read in centralized configuration
    ${content}    Create List    Parsing of retention file failed
    ${result}    Ctn Find In Log With Timeout    ${VarRoot}/log/centreon-engine/config0/centengine-stdout.log    ${restart}    ${content}    5
    Should Not Be True    ${result}    Engine should not even try to read retention.dat
    ${content}    Create List    runtime state: 0 hosts and 0 services restored
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${restart}    ${content}    5
    Should Not Be True    ${result}    The snapshot should restore at least one resource
    ${content}    Create List    services restored from the Broker snapshot
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${restart}    ${content}    60
    Should Be True    ${result}    Engine should restore its resources state from the Broker snapshot
    # The output is in the snapshot so that $SERVICEOUTPUT$ is not empty
    # before the next check; the long output is not.
    ${output}    Ctn Get Service Info Grpc    ${1}    ${1}
    Should Be Equal As Strings    ${output}[pluginOutput]    output critical for service_1    service_1 should get its check output back from the snapshot
    Should Be Empty    ${output}[longPluginOutput]    the long output is not part of the snapshot

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

CERS3
    [Documentation]    Scenario: after a Broker restart, the runtime restored from the resources table is completed from the hosts and services tables
    ...    Given Broker and Engine are started in centralized configuration, with passive services, Broker writing both the resources and the hosts/services tables
    ...    And service_1 of host_1 is CRITICAL HARD
    ...    When Engine is stopped, its retention.dat is deleted, Broker is stopped and both are started again
    ...    Then unified_sql restores the runtime from the resources table, then completes it from the hosts/services tables
    ...    And Engine gets back from the Broker snapshot the last time service_1 was CRITICAL, a date the resources table does not hold
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
    ${before}    Ctn Get Service Info Grpc    ${1}    ${1}

    # Engine first, so that no status reaches Broker after it restarts: the
    # database is then the only source of the runtime.
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Remove File    ${VarRoot}/log/centreon-engine/config0/retention.dat
    ${restart}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True

    ${content}    Create List    restored into the global cache from the resources table
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    60
    Should Be True    ${result}    unified_sql should restore the runtime from the resources table
    ${content}    Create List    completed in the global cache from the hosts/services tables
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    60
    Should Be True    ${result}    unified_sql should complete the runtime from the hosts/services tables
    ${content}    Create List    runtime of 0 hosts and 0 services completed
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    5
    Should Not Be True    ${result}    At least one resource should have been completed

    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${restart}    ${1}
    ${content}    Create List    services restored from the Broker snapshot
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${restart}    ${content}    60
    Should Be True    ${result}    Engine should restore its resources state from the Broker snapshot

    # last_time_critical is not in the resources table: only the complement
    # brings it back.
    ${after}    Ctn Get Service Info Grpc    ${1}    ${1}
    Should Be Equal As Strings    ${after}[lastTimeCritical]    ${before}[lastTimeCritical]    service_1 should get back the last time it was CRITICAL

    [Teardown]    Ctn Stop Engine Broker And Save Logs

CERS4
    [Documentation]    Scenario: an Engine restarted without retention.dat gets its event and problem ids back from Broker
    ...    Given Broker and Engine are started in centralized configuration, with passive services
    ...    And service_1 of host_1 is CRITICAL HARD, with an event id and a problem id
    ...    When Engine is stopped, its retention.dat is deleted and Engine is started again
    ...    Then service_1 gets back its event and problem ids from the Broker snapshot
    ...    And the recovery of service_1 closes that same problem
    ...    And a new problem on service_1 gets a new problem id, the counter having been moved past the restored ids
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
    ${before}    Ctn Get Service Info Grpc    ${1}    ${1}
    Should Not Be Equal As Strings    ${before}[currentEventId]    0    service_1 should have an event id
    Should Not Be Equal As Strings    ${before}[currentProblemId]    0    service_1 should have a problem id

    Ctn Stop Engine
    Remove File    ${VarRoot}/log/centreon-engine/config0/retention.dat
    ${restart}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${restart}    ${1}
    ${content}    Create List    services restored from the Broker snapshot
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${restart}    ${content}    60
    Should Be True    ${result}    Engine should restore its resources state from the Broker snapshot

    ${after}    Ctn Get Service Info Grpc    ${1}    ${1}
    Should Be Equal As Strings    ${after}[currentEventId]    ${before}[currentEventId]    service_1 should get its event id back
    Should Be Equal As Strings    ${after}[lastEventId]    ${before}[lastEventId]    service_1 should get its last event id back
    Should Be Equal As Strings    ${after}[currentProblemId]    ${before}[currentProblemId]    service_1 should get its problem id back
    Should Be Equal As Strings    ${after}[lastProblemId]    ${before}[lastProblemId]    service_1 should get its last problem id back

    # The recovery closes the restored problem.
    Ctn Process Service Result Hard    host_1    service_1    0    output ok for service_1
    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    0    60    HARD
    Should Be True    ${result}    The service (host_1,service_1) should be OK HARD
    ${ok}    Ctn Get Service Info Grpc    ${1}    ${1}
    Should Be Equal As Strings    ${ok}[currentProblemId]    0    service_1 has no problem anymore
    Should Be Equal As Strings    ${ok}[lastProblemId]    ${before}[currentProblemId]    The recovery should close the restored problem

    # A new problem gets an id the restored counter never gave.
    Ctn Process Service Result Hard    host_1    service_1    2    output critical again for service_1
    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    2    60    HARD
    Should Be True    ${result}    The service (host_1,service_1) should be CRITICAL HARD again
    ${again}    Ctn Get Service Info Grpc    ${1}    ${1}
    Should Be True    ${again}[currentProblemId] > ${before}[currentProblemId]    The new problem should get a new id
    Should Be True    ${again}[currentEventId] > ${ok}[currentEventId]    The new event should get a new id

    [Teardown]    Ctn Stop Engine Broker And Save Logs

CERS5
    [Documentation]    Scenario: the event and problem ids survive a restart of both Broker and Engine
    ...    Given Broker and Engine are started in centralized configuration, with passive services
    ...    And service_1 of host_1 is CRITICAL HARD, with an event id and a problem id
    ...    When Engine and Broker are stopped, retention.dat is deleted and both are started again
    ...    Then Broker reads the ids of service_1 back from its cache file
    ...    And Engine gets them from the Broker snapshot
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
    ${before}    Ctn Get Service Info Grpc    ${1}    ${1}
    Should Not Be Equal As Strings    ${before}[currentProblemId]    0    service_1 should have a problem id

    # Engine first, so that no status reaches Broker after it restarts: its
    # cache file is then the only source of the ids.
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Remove File    ${VarRoot}/log/centreon-engine/config0/retention.dat
    ${restart}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${restart}    ${1}
    ${content}    Create List    services restored from the Broker snapshot
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${restart}    ${content}    60
    Should Be True    ${result}    Engine should restore its resources state from the Broker snapshot

    ${after}    Ctn Get Service Info Grpc    ${1}    ${1}
    Should Be Equal As Strings    ${after}[currentEventId]    ${before}[currentEventId]    service_1 should get its event id back
    Should Be Equal As Strings    ${after}[currentProblemId]    ${before}[currentProblemId]    service_1 should get its problem id back

    [Teardown]    Ctn Stop Engine Broker And Save Logs

CERS6
    [Documentation]    Scenario: an Engine restarted without retention.dat gets its notification id back from Broker
    ...    Given Broker and Engine are started in centralized configuration, notifications handled by Engine
    ...    And service_1 of host_1 is CRITICAL HARD and notified
    ...    And a custom notification is then sent for service_1, giving it a new notification id without any state change
    ...    When Engine is stopped, its retention.dat is deleted and Engine is started again
    ...    Then service_1 gets back the id of the custom notification from the Broker snapshot
    ...    And a new custom notification gets a greater id, the generator having been moved past the restored one
    [Tags]    engine    broker    centralized    retention    notification    MON-187019
    Ctn Config Centralized Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_config    info
    Ctn Engine Config Set Value    ${0}    enable_notifications    ${1}    True
    Ctn Engine Config Set Value    ${0}    log_notifications    ${1}    True
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Engine Config Add Command    ${0}    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Services    ${0}    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    ${0}    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    ${0}    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    ${0}    service_1    notification_period    24x7
    Ctn Set Services Passive    ${0}    service_[0-9]+
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    cache    info

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
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    service_1 should be notified when it becomes CRITICAL HARD
    ${problem}    Wait Until Keyword Succeeds    30s    1s    Service 1 Notification Id Should Be Above    0

    # No state change: only the adaptive status sent after the notification
    # tells Broker about the new id.
    Ctn Send External Command    SEND_CUSTOM_SVC_NOTIFICATION;host_1;service_1;2;admin;custom notification
    ${custom}    Wait Until Keyword Succeeds    30s    1s    Service 1 Notification Id Should Be Above    ${problem}

    Ctn Stop Engine
    Remove File    ${VarRoot}/log/centreon-engine/config0/retention.dat
    ${restart}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${restart}    ${1}
    ${content}    Create List    services restored from the Broker snapshot
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${restart}    ${content}    60
    Should Be True    ${result}    Engine should restore its resources state from the Broker snapshot

    ${after}    Ctn Get Service Info Grpc    ${1}    ${1}
    Should Be Equal As Strings    ${after}[currentNotificationId]    ${custom}    service_1 should get back the id of the custom notification

    Ctn Send External Command    SEND_CUSTOM_SVC_NOTIFICATION;host_1;service_1;2;admin;custom notification after restart
    Wait Until Keyword Succeeds    30s    1s    Service 1 Notification Id Should Be Above    ${custom}

    [Teardown]    Ctn Stop Engine Broker And Save Logs


CERS7
    [Documentation]    Scenario: in notification_mode=broker, Engine keeps the notification id Broker attributed and gets it back after a restart
    ...    Given Broker and Engine are started in centralized configuration, notifications decided by Broker
    ...    When service_1 of host_1 becomes CRITICAL HARD and Broker dispatches its notification to the poller
    ...    Then Engine keeps the notification id Broker attributed on service_1, for the macros read later
    ...    When Engine is stopped, its retention.dat is deleted and Engine is started again
    ...    Then service_1 gets that notification id back from the Broker snapshot
    [Tags]    engine    broker    centralized    retention    notification    MON-187019
    Ctn Config Centralized Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_config    info
    Ctn Engine Config Set Value    ${0}    enable_notifications    ${1}    True
    Ctn Engine Config Set Value    ${0}    log_notifications    ${1}    True
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Engine Config Add Command    ${0}    command_notif    /usr/bin/true
    Ctn Engine Config Set Value In Services    ${0}    service_1    contacts    John_Doe
    Ctn Engine Config Set Value In Services    ${0}    service_1    notification_options    w,c,r
    Ctn Engine Config Set Value In Services    ${0}    service_1    notifications_enabled    1
    Ctn Engine Config Set Value In Services    ${0}    service_1    notification_period    24x7
    Ctn Set Services Passive    ${0}    service_[0-9]+
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    cache    info
    Ctn Broker Config Log    central    notifications    info

    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Db    hosts
    Ctn Clear Db    services
    Ctn Clear Db    resources
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    # The contact must be in the Broker cache before the first decision.
    Wait Until Created    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/1.prot    timeout=30s

    Ctn Process Service Result Hard    host_1    service_1    2    output critical for service_1
    ${content}    Create List    dispatched notification execution for resource (1, 1)
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should dispatch the notification of service_1 to the poller
    ${content}    Create List    SERVICE NOTIFICATION: John_Doe;host_1;service_1;CRITICAL;command_notif;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The poller should run the notification Broker dispatched
    ${notified}    Wait Until Keyword Succeeds    30s    1s    Service 1 Notification Id Should Be Above    0

    Ctn Stop Engine
    Remove File    ${VarRoot}/log/centreon-engine/config0/retention.dat
    ${restart}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${restart}    ${1}
    ${content}    Create List    services restored from the Broker snapshot
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${restart}    ${content}    60
    Should Be True    ${result}    Engine should restore its resources state from the Broker snapshot

    ${after}    Ctn Get Service Info Grpc    ${1}    ${1}
    Should Be Equal As Strings    ${after}[currentNotificationId]    ${notified}    service_1 should get back the notification id Broker attributed

    [Teardown]    Ctn Stop Engine Broker And Save Logs

CERS8
    [Documentation]    Scenario: a poller switching from the legacy configuration to the centralized one uses its retention.dat one last time
    ...    Given Broker and Engine are started in legacy configuration, with passive services
    ...    And service_1 of host_1 is in a downtime held by Engine
    ...    And Engine is stopped, writing its retention.dat
    ...    When the poller switches to the centralized configuration and Engine starts with its centengine.cfg, as the packaged service does
    ...    Then Engine reads retention.dat once and renames it retention.dat.migrated
    ...    And service_1 is still in downtime, which only retention.dat could tell
    ...    And SAVE_STATE_INFORMATION, as stopping Engine, saves the downtime in downtimes.prot but writes no retention.dat
    ...    And at its next start, Engine has no retention file to read but gets the downtime of service_1 back from downtimes.prot
    [Tags]    engine    broker    centralized    retention    MON-187019
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Set Services Passive    ${0}    service_[0-9]+

    Ctn Clear Retention
    Remove File    ${VarRoot}/log/centreon-engine/config0/retention.dat.migrated
    Ctn Clear Prot Files
    Ctn Clear Db    downtimes
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # The downtime is held by Engine: notification_mode is not broker. It is
    # fixed so that it starts at once (a flexible one waits for a problem).
    ${now}    Evaluate    int(time.time())    modules=time
    ${end}    Evaluate    ${now} + 3600
    Ctn Send External Command    SCHEDULE_SVC_DOWNTIME;host_1;service_1;${now};${end};1;0;3600;admin;fixed downtime
    Wait Until Keyword Succeeds    60s    2s    Service 1 Downtime Depth Should Be    ${1}
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    ${downtime}    Ctn Grep Retention    ${0}    servicedowntime
    Should Not Be Empty    ${downtime}    The legacy Engine should have written its downtime in retention.dat

    # The poller switches to the centralized configuration. Neither Engine
    # (no state.prot) nor Broker knows its runtime state yet. On a real poller,
    # the legacy centengine.cfg is still there; Ctn Config Centralized Engine
    # wipes it, so it is kept aside and put back.
    Remove Directory    ${VarRoot}/legacy-config0    recursive=True
    Copy Directory    ${EtcRoot}/centreon-engine/config0    ${VarRoot}/legacy-config0
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Directory Should Not Exist    ${EtcRoot}/centreon-engine/config0
    Copy Directory    ${VarRoot}/legacy-config0    ${EtcRoot}/centreon-engine/config0
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_config    info
    Ctn Set Services Passive    ${0}    service_[0-9]+
    ${switch}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True    with_legacy_cfg=True
    Ctn Wait For Engine To Be Ready    ${switch}    ${1}

    # The retention phase runs before the log file is opened: its lines go to
    # the standard output of centengine.
    ${content}    Create List    read once and renamed to
    ${result}    Ctn Find In Log With Timeout    ${VarRoot}/log/centreon-engine/config0/centengine-stdout.log    ${switch}    ${content}    30
    Should Be True    ${result}    Engine should read retention.dat once and rename it
    File Should Not Exist    ${VarRoot}/log/centreon-engine/config0/retention.dat
    File Should Exist    ${VarRoot}/log/centreon-engine/config0/retention.dat.migrated
    # The downtime must survive the first configuration Broker sends, applied
    # over the one parsed from centengine.cfg.
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${switch}    ${content}    60
    Should Be True    ${result}    Broker should receive the acknowledgement of the first diff state
    Service 1 Downtime Depth Should Be    ${1}

    # In centralized configuration, Engine never writes retention.dat: only
    # the downtimes it owns are saved, in downtimes.prot.
    File Should Not Exist    ${VarRoot}/lib/centreon-engine/config0/downtimes.prot
    Ctn Send External Command    SAVE_STATE_INFORMATION
    Wait Until Created    ${VarRoot}/lib/centreon-engine/config0/downtimes.prot    30s
    File Should Not Exist    ${VarRoot}/log/centreon-engine/config0/retention.dat
    Ctn Stop Engine
    File Should Not Exist    ${VarRoot}/log/centreon-engine/config0/retention.dat
    ...    Engine should not write retention.dat when stopped in centralized configuration

    # The renamed file is never read again, the downtime comes from
    # downtimes.prot.
    ${restart}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True    with_legacy_cfg=True
    Ctn Wait For Engine To Be Ready    ${restart}    ${1}
    ${content}    Create List    no retention file to read: the runtime state comes from Broker
    ${result}    Ctn Find In Log With Timeout    ${VarRoot}/log/centreon-engine/config0/centengine-stdout.log    ${restart}    ${content}    30
    Should Be True    ${result}    Engine should not read retention.dat.migrated again
    ${content}    Create List    Startup: 1 downtime(s) restored from
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${restart}    ${content}    30
    Should Be True    ${result}    Engine should restore the downtime from downtimes.prot
    Wait Until Keyword Succeeds    30s    2s    Service 1 Downtime Depth Should Be    ${1}

    [Teardown]    Ctn Stop Engine Broker And Save Logs

CERS9
    [Documentation]    Scenario: in centralized configuration, the downtimes Engine owns survive its restarts
    ...    Given Broker and Engine are started in centralized configuration, notification_mode being engine
    ...    And service_1 of host_1 is in a fixed downtime held by Engine
    ...    When Engine is stopped and started again, twice
    ...    Then each time Engine restores the downtime from downtimes.prot
    ...    And the downtime depth of service_1 is still 1, the depth being rebuilt and not counted twice
    ...    And once the downtime is deleted, a restart restores no downtime anymore
    [Tags]    engine    broker    centralized    retention    downtime    MON-187019
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_config    info
    Ctn Set Services Passive    ${0}    service_[0-9]+

    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Db    hosts
    Ctn Clear Db    services
    Ctn Clear Db    resources
    Ctn Clear Db    downtimes
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Fixed, so that it starts at once (a flexible one waits for a problem).
    ${now}    Evaluate    int(time.time())    modules=time
    ${end}    Evaluate    ${now} + 3600
    Ctn Send External Command    SCHEDULE_SVC_DOWNTIME;host_1;service_1;${now};${end};1;0;3600;admin;fixed downtime
    Wait Until Keyword Succeeds    60s    2s    Service 1 Downtime Depth Should Be    ${1}

    FOR    ${i}    IN RANGE    2
        Ctn Stop Engine
        File Should Exist    ${VarRoot}/lib/centreon-engine/config0/downtimes.prot
        ...    Engine should save its downtimes in downtimes.prot when stopped
        ${restart}    Ctn Get Round Current Date
        Ctn Start Engine    newGeneration=True
        Ctn Wait For Engine To Be Ready    ${restart}    ${1}
        ${content}    Create List    Startup: 1 downtime(s) restored from
        ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${restart}    ${content}    30
        Should Be True    ${result}    Engine should restore the downtime from downtimes.prot
        # The snapshot Broker sends must not change the depth.
        ${content}    Create List    services restored from the Broker snapshot
        ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${restart}    ${content}    60
        Should Be True    ${result}    Engine should apply the Broker snapshot
        Wait Until Keyword Succeeds    30s    2s    Service 1 Downtime Depth Should Be    ${1}
    END

    # Once deleted, the downtime is not restored anymore.
    Ctn Delete Service Downtime Full    ${0}    host_1    service_1
    Wait Until Keyword Succeeds    60s    2s    Service 1 Downtime Depth Should Be    ${0}
    Ctn Stop Engine
    ${restart}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${restart}    ${1}
    ${content}    Create List    Startup: 0 downtime(s) restored from
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${restart}    ${content}    30
    Should Be True    ${result}    Engine should restore no downtime once it is deleted
    Service 1 Downtime Depth Should Be    ${0}

    [Teardown]    Ctn Stop Engine Broker And Save Logs

CERS10
    [Documentation]    Scenario: a host moved from a running poller to another running poller keeps its state
    ...    Given Broker and two Engines are started in centralized configuration, with passive services
    ...    And service_1 of host_1, on poller 1, is CRITICAL HARD
    ...    And the notifications of service_2 of host_1 are disabled
    ...    When host_1 and its services are moved to poller 2, both Engines still running
    ...    Then poller 1 removes host_1 and poller 2 creates it
    ...    And poller 2 gets from Broker the state of service_1 and the disabled notifications of service_2
    ...    And the database still shows service_1 CRITICAL HARD and service_2 without notifications
    [Tags]    engine    broker    centralized    retention    MON-187019
    Ctn Config Centralized Engine    ${2}    ${4}    ${3}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${2}
    FOR    ${i}    IN RANGE    2
        Ctn Engine Config Set Value    ${i}    log_legacy_enabled    ${0}
        Ctn Engine Config Set Value    ${i}    log_v2_enabled    ${1}
        Ctn Engine Config Set Value    ${i}    log_level_config    debug
        Ctn Set Services Passive    ${i}    service_[0-9]+
    END
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    cache    info
    Ctn Broker Config Log    central    config    info

    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Db    hosts
    Ctn Clear Db    services
    Ctn Clear Db    resources
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${2}

    # Two pieces of runtime a check does not give back: a HARD state, and an
    # adaptive change.
    Ctn Process Service Result Hard    host_1    service_1    2    output critical for service_1
    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    2    60    HARD
    Should Be True    ${result}    The service (host_1,service_1) should be CRITICAL HARD
    Ctn Send External Command    DISABLE_SVC_NOTIFICATIONS;host_1;service_2
    Wait Until Keyword Succeeds    60s    2s    Service Notify In Db Should Be    host_1    service_2    ${0}

    # The user attaches host_1 to the other poller: one export for both.
    Ctn Engine Config Move Host    ${0}    ${1}    host_1
    ${move}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    ${0}    ${1}

    ${content}    Create List    Removing host 'host_1'.
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${move}    ${content}    60
    Should Be True    ${result}    Poller 1 should remove host_1
    ${content}    Create List    Creating new host 'host_1'.
    ${result}    Ctn Find In Log With Timeout    ${engineLog1}    ${move}    ${content}    60
    Should Be True    ${result}    Poller 2 should create host_1
    Wait Until Keyword Succeeds    60s    2s    Host Instance In Db Should Be    host_1    ${2}

    # Poller 2 is described what it receives, poller 1 nothing: it runs, and
    # knows its resources better than Broker.
    ${content}    Create List    the diff adds to poller 2 added to its DiffState
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${move}    ${content}    30
    Should Be True    ${result}    Broker should send poller 2 the runtime of the resources it receives
    FOR    ${line}    IN    added to the DiffState of poller 1    the diff adds to poller 1
        ${content}    Create List    ${line}
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${move}    ${content}    1
        Should Not Be True    ${result}    Broker should send poller 1 no runtime state with the diff
    END

    # What poller 2 knows of the resources it received.
    ${info}    Ctn Get Service Info Grpc    ${1}    ${1}    config=${1}
    Should Be Equal As Strings    ${info}[currentState]    CRITICAL
    ...    Poller 2 should get the CRITICAL state of service_1 from Broker
    # Not stateType: the gRPC message types it as a State, HARD reads WARNING.
    Should Be Equal As Strings    ${info}[lastHardState]    CRITICAL
    ...    Poller 2 should get the last hard state of service_1 from Broker
    Should Be Equal As Strings    ${info}[pluginOutput]    output critical for service_1
    ...    Poller 2 should get the output of service_1 from Broker
    ${info}    Ctn Get Service Info Grpc    ${1}    ${2}    config=${1}
    Should Not Be True    ${info}[notificationsEnabled]
    ...    Poller 2 should get the disabled notifications of service_2 from Broker

    # Whatever poller 2 sends about them must not undo the database. Full
    # statuses reach it through the bulk binds, some 30 s later.
    Sleep    40s
    ${result}    Ctn Check Service Status With Timeout    host_1    service_1    2    5    HARD
    Should Be True    ${result}    The database should still show service_1 CRITICAL HARD
    Service Notify In Db Should Be    host_1    service_2    ${0}

    [Teardown]    Ctn Stop Engine Broker And Save Logs

CERS11
    [Documentation]    Scenario: the attributes overridden by external commands survive a change of the configuration
    ...    Given Broker and one Engine are started in centralized configuration, with passive services
    ...    And the notifications of service_1 of host_1 are disabled and its check interval changed to 10
    ...    And the active checks of host_1 are disabled
    ...    When the configuration changes the check interval of service_1 to 7, its max_check_attempts to 4, and the alias of host_1
    ...    Then Engine applies the new max_check_attempts and alias, and keeps the overridden notifications, check interval and active checks
    ...    And the hosts, services and resources tables do the same, with the modified_attributes bits
    [Tags]    engine    broker    centralized    retention    MON-187019
    Ctn Config Centralized Engine    ${1}    ${4}    ${3}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Set Services Passive    ${0}    service_[0-9]+
    Ctn Broker Config Log    central    bbdo    info
    Ctn Broker Config Log    central    cache    info
    Ctn Broker Config Log    central    config    info
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

    Ctn Send External Command
    ...    DISABLE_SVC_NOTIFICATIONS;host_1;service_1
    ...    CHANGE_NORMAL_SVC_CHECK_INTERVAL;host_1;service_1;10
    ...    DISABLE_HOST_CHECK;host_1
    # service_1: MOD_NOTIFICATIONS_ENABLED (1) | MOD_NORMAL_CHECK_INTERVAL (1024);
    # host_1: MOD_ACTIVE_CHECKS_ENABLED (2).
    Wait Until Keyword Succeeds    60s    2s    Service Overrides In Db Should Be    host_1    service_1
    ...    ((0, 10.0, 3, 1025, 0, 3),)
    Wait Until Keyword Succeeds    60s    2s    Host Overrides In Db Should Be    host_1    ((0, 'host_1', 2, 0),)

    # The user changes service_1 and host_1: one attribute an override holds,
    # two it does not.
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    7
    Ctn Engine Config Replace Value In Services    ${0}    service_1    max_check_attempts    4
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    alias    host_1_renamed
    ${change}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    ${0}
    ${content}    Create List    Modifying service 'service_1' of host 'host_1'.    Modifying host 'host_1' (id 1).
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${change}    ${content}    60
    Should Be True    ${result}    Engine should modify service_1 and host_1

    ${info}    Ctn Get Service Info Grpc    ${1}    ${1}
    Should Be Equal As Integers    ${info}[maxCheckAttempts]    4
    ...    Engine should apply the new max_check_attempts of service_1
    Should Be Equal As Integers    ${info}[checkInterval]    10
    ...    Engine should keep the overridden check interval of service_1
    Should Not Be True    ${info}[notificationsEnabled]
    ...    Engine should keep the disabled notifications of service_1
    Should Be Equal As Integers    ${info}[modifiedAttributes]    1025
    ...    service_1 should keep its modified_attributes bits
    ${info}    Ctn Get Host Info Grpc    ${1}
    Should Be Equal As Strings    ${info}[alias]    host_1_renamed    Engine should apply the new alias of host_1
    Should Not Be True    ${info}[checksEnabled]    Engine should keep the disabled active checks of host_1
    Should Be Equal As Integers    ${info}[modifiedAttributes]    2    host_1 should keep its modified_attributes bit

    Wait Until Keyword Succeeds    60s    2s    Service Overrides In Db Should Be    host_1    service_1
    ...    ((0, 10.0, 4, 1025, 0, 4),)
    Wait Until Keyword Succeeds    60s    2s    Host Overrides In Db Should Be    host_1
    ...    ((0, 'host_1_renamed', 2, 0),)

    [Teardown]    Ctn Stop Engine Broker And Save Logs


*** Keywords ***
Service Notify In Db Should Be
    [Documentation]    Fails unless the notify column of the service ${svc} of ${host} is ${expected} in the services table.
    [Arguments]    ${host}    ${svc}    ${expected}
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Query
    ...    SELECT s.notify FROM services s JOIN hosts h ON h.host_id = s.host_id WHERE h.name = '${host}' AND s.description = '${svc}'
    Disconnect From Database
    Should Not Be Empty    ${output}    The service ${svc} of ${host} should be in the database
    Should Be Equal As Integers    ${output}[0][0]    ${expected}

Service Overrides In Db Should Be
    [Documentation]    Fails unless the rows of the service ${svc} of ${host} read ${expected}, as Python prints them:
    ...    services.notify, check_interval, max_check_attempts, modified_attributes, then
    ...    resources.notifications_enabled, max_check_attempts.
    [Arguments]    ${host}    ${svc}    ${expected}
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Query
    ...    SELECT s.notify, s.check_interval, s.max_check_attempts, s.modified_attributes, r.notifications_enabled, r.max_check_attempts FROM services s JOIN hosts h ON h.host_id = s.host_id JOIN resources r ON r.id = s.service_id AND r.parent_id = s.host_id WHERE h.name = '${host}' AND s.description = '${svc}'
    Disconnect From Database
    Should Be Equal As Strings    ${output}    ${expected}

Host Overrides In Db Should Be
    [Documentation]    Fails unless the rows of the host ${host} read ${expected}, as Python prints them:
    ...    hosts.active_checks, alias, modified_attributes, then resources.active_checks_enabled.
    [Arguments]    ${host}    ${expected}
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Query
    ...    SELECT h.active_checks, h.alias, h.modified_attributes, r.active_checks_enabled FROM hosts h JOIN resources r ON r.id = h.host_id AND r.parent_id = 0 WHERE h.name = '${host}'
    Disconnect From Database
    Should Be Equal As Strings    ${output}    ${expected}

Host Instance In Db Should Be
    [Documentation]    Fails unless the host ${host} belongs to the instance ${expected} in the hosts table.
    [Arguments]    ${host}    ${expected}
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Query    SELECT instance_id FROM hosts WHERE name = '${host}'
    Disconnect From Database
    Should Not Be Empty    ${output}    The host ${host} should be in the database
    Should Be Equal As Integers    ${output}[0][0]    ${expected}
Service 1 Downtime Depth Should Be
    [Documentation]    Fails unless the scheduled downtime depth of (host_1, service_1) in Engine is ${expected}.
    [Arguments]    ${expected}
    ${info}    Ctn Get Service Info Grpc    ${1}    ${1}
    ${depth}    Evaluate    $info.get('scheduledDowntimeDepth', 0)
    Should Be Equal As Integers    ${depth}    ${expected}

Service 1 Notification Id Should Be Above
    [Documentation]    Fails unless the current notification id of (host_1, service_1) is greater than ${min}, returns it otherwise.
    [Arguments]    ${min}
    ${info}    Ctn Get Service Info Grpc    ${1}    ${1}
    Should Be True    ${info}[currentNotificationId] > ${min}
    RETURN    ${info}[currentNotificationId]

*** Settings ***
Documentation     Centreon Broker and Engine start/stop tests

Resource          ../resources/import.resource

Suite Setup       Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup        Ctn Stop Processes
Test Teardown     Ctn Save Logs If Failed


*** Test Cases ***
BECSS1
    [Documentation]     Scenario: Broker sends configuration to engine in new generation
    ...    Given an engine configuration is provided to the broker
    ...    And the broker and engine are started in new generation (broker first)
    ...    And the protocol is bbdo3
    ...    When the broker detects the configuration for the engine
    ...    Then the broker sends the configuration to the engine
    ...    Then both broker and engine are stopped (engine first)
    [Tags]    broker    engine    start-stop    MON-153802
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Flush Log    central    0

    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    ${start}    Ctn Get Round Current Date
    ${result}    Ctn In Bbdo2
    Should Not Be True    ${result}    We should be in BBDO3 in this test (forced by newGeneration=True).
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established

    ${content}    Create List    sending DiffState to poller 1    BBDO: received diff state ack
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No new Engine configuration found in central cbd log

    Ctn Kindly Stop Broker
    Ctn Stop Engine

BECSS2
    [Documentation]     Scenario: Broker sends configuration to engine in new generation
    ...    Given an engine configuration is provided to the broker
    ...    And the broker and engine are started in new generation (broker first)
    ...    And the protocol is bbdo3
    ...    When the broker detects the configuration for the engine
    ...    Then the broker sends the configuration to the engine
    ...    Then both broker and engine are stopped (engine first)
    [Tags]    broker    engine    start-stop    MON-153802
    Ctn Clear Retention
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Flush Log    central    0

    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    ${start}    Ctn Get Round Current Date
    ${result}    Ctn In Bbdo2
    Should Not Be True    ${result}    We should be in BBDO3 in this test (newGeneration=True).

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established

    ${content}    Create List    sending DiffState to poller 1    BBDO: received diff state ack
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No new Engine configuration found in central cbd log

    Ctn Stop Engine
    ${content}    Create List    unified_sql: Disabling poller
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No stop event processed by central cbd
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker

BECSS3
    [Documentation]     Scenario: Broker sends configuration to engine in new generation
    ...    Given an engine configuration is provided to the broker
    ...    And the broker and engine are started in new generation (engine first)
    ...    And the protocol is bbdo3
    ...    When the broker detects the configuration for the engine
    ...    Then the broker sends the configuration to the engine
    ...    Then both broker and engine are stopped (engine first)
    [Tags]    broker    engine    start-stop    MON-153802
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug

    Ctn Start Engine    newGeneration=True
    Ctn Start Broker    newGeneration=True

    ${start}    Ctn Get Round Current Date
    ${result}    Ctn In Bbdo2
    Should Not Be True    ${result}    We should be in BBDO3 in this test (newGeneration=True).

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established

    ${content}    Create List    sending DiffState to poller 1    BBDO: received diff state ack
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No new Engine configuration found in central cbd log

    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database

    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker

BECSS4
    [Documentation]     Scenario: Broker sends configuration to engine in new generation
    ...    Given an engine configuration is provided to the broker
    ...    And the broker and engine are started in new generation (engine first)
    ...    And the protocol is bbdo3
    ...    When the broker detects the configuration for the engine
    ...    Then the broker sends the configuration to the engine
    ...    Then both broker and engine are stopped (broker first)
    [Tags]    broker    engine    start-stop
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug

    Ctn Start Engine    newGeneration=True
    Ctn Start Broker    newGeneration=True

    ${start}    Ctn Get Round Current Date
    ${result}    Ctn In Bbdo2
    Should Not Be True    ${result}    We should be in BBDO3 in this test (newGeneration=True).

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established

    ${content}    Create List    sending DiffState to poller 1    BBDO: received diff state ack
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No new Engine configuration found in central cbd log

    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database

    Ctn Kindly Stop Broker
    Ctn Stop Engine

BECSS5
    [Documentation]    Scenario: Broker stopped between an acknowledgement and the end of the round
    ...    Given one poller running the configuration Broker pushed to it
    ...    And Engine frozen, so that the next configuration it is sent stays in flight
    ...    When a new version is exported, and sent to Engine
    ...    And Broker is stopped
    ...    And new-1.prot is renamed 1.prot and new-diff-1.prot diff-1.prot, as the acknowledgement does
    ...    Then the database does not hold the change
    ...    When Broker is started again, Engine still frozen and without any new export
    ...    Then Broker finds diff-1.prot, closes the round and publishes the global diff
    ...    And diff-1.prot is removed
    ...    And the database holds the change.
    [Tags]    broker    engine    config    centralized    start-stop
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Logs
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Push Configuration And Wait    ${start}    ${0}

    VAR    ${dir}    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration
    VAR    ${query}
    ...    SELECT s.check_interval FROM services s JOIN hosts h ON h.host_id = s.host_id WHERE h.name = 'host_1' AND s.description = 'service_1'

    # Frozen, Engine receives the new version but cannot apply it: Broker has
    # new-1.prot and new-diff-1.prot, and no acknowledgement comes.
    Run Process    pkill    -STOP    centengine
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    7
    ${start}    Ctn Get Round Current Date
    Ctn Announce Poller Configurations    batch    ${0}
    ${content}    Create List    sending DiffState to poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The new version was not sent to the poller
    File Should Exist    ${dir}/new-1.prot
    File Should Exist    ${dir}/new-diff-1.prot

    # What an acknowledgement followed by a stop of Broker before the end of
    # the round leaves: 1.prot is the new version, its difference waits in
    # diff-1.prot, the database has not received it.
    Ctn Kindly Stop Broker
    Move File    ${dir}/new-1.prot    ${dir}/1.prot
    Move File    ${dir}/new-diff-1.prot    ${dir}/diff-1.prot
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Query    ${query}
    Should Not Be Equal As Strings
    ...    ${output}
    ...    ((7.0,),)
    ...    The database holds the change before the round is closed

    # Engine stays frozen: nothing but the difference left behind can carry
    # the change to the database.
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    ${content}    Create List
    ...    did not reach the database before Broker stopped, and no configuration is left to deliver
    ...    all engine peers have acknowledged their configuration
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker did not close at startup the round it had left open
    Wait Until Removed    ${dir}/diff-1.prot    10s

    FOR    ${i}    IN RANGE    30
        ${output}    Query    ${query}
        IF    "${output}" == "((7.0,),)"    BREAK
        Sleep    1s
    END
    Disconnect From Database
    Should Be Equal As Strings    ${output}    ((7.0,),)    The database did not receive the difference left behind
    [Teardown]    Run Keywords
    ...    Run Process    pkill    -CONT    centengine
    ...    AND    Ctn Stop Engine Broker And Save Logs

BECSS6
    [Documentation]    Scenario: an Engine that lost its state.prot is sent the stored configuration again
    ...    Given one poller running the configuration Broker pushed to it
    ...    When Engine is stopped and its state.prot removed
    ...    And Engine is started again, with no new export
    ...    Then Engine announces no version, and Broker stores one for it with nothing newer
    ...    And Broker sends it the stored configuration, whole
    ...    And Engine acknowledges it and writes its state.prot again
    ...    And no file was prepared for that in the pollers configuration directory.
    [Tags]    broker    engine    config    centralized    start-stop
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Logs
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Push Configuration And Wait    ${start}    ${0}

    VAR    ${dir}    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration
    VAR    ${state_prot}    ${VarRoot}/lib/centreon-engine/config0/state.prot
    File Should Exist    ${dir}/1.prot
    File Should Exist    ${state_prot}

    Ctn Stop Engine
    Remove File    ${state_prot}
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True

    ${content}    Create List
    ...    it is owed the stored one, whole
    ...    poller 1 does not run the stored configuration
    ...    sending DiffState to poller 1
    ...    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker did not send the stored configuration to the Engine that lost its state.prot
    Wait Until Created    ${state_prot}    30s
    File Should Exist    ${dir}/1.prot
    File Should Not Exist    ${dir}/new-1.prot
    File Should Not Exist    ${dir}/new-diff-1.prot
    [Teardown]    Ctn Stop Engine Broker And Save Logs

BECSS_GRPC1
    [Documentation]     Scenario: Broker sends configuration to engine in new generation
    ...    Given an engine configuration is provided to the broker
    ...    And the broker and engine are started in new generation (broker first)
    ...    And the protocol is bbdo3
    ...    When the broker detects the configuration for the engine
    ...    Then the broker sends the configuration to the engine
    ...    Then both broker and engine are stopped (engine first)
    [Tags]    broker    engine    start-stop    MON-153802
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Flush Log    central    0
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Clear Broker Logs

    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    ${start}    Ctn Get Round Current Date
    ${result}    Ctn In Bbdo2
    Should Not Be True    ${result}    We should be in BBDO3 in this test (forced by newGeneration=True).
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established

    ${content}    Create List    sending DiffState to poller 1    BBDO: received diff state ack
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No new Engine configuration found in central cbd log

    Ctn Kindly Stop Broker
    Ctn Stop Engine

BECSS_GRPC2
    [Documentation]     Scenario: Broker sends configuration to engine in new generation
    ...    Given an engine configuration is provided to the broker
    ...    And the broker and engine are started in new generation (broker first)
    ...    And the protocol is bbdo3
    ...    When the broker detects the configuration for the engine
    ...    Then the broker sends the configuration to the engine
    ...    Then both broker and engine are stopped (engine first)
    [Tags]    broker    engine    start-stop    MON-153802
    Ctn Clear Retention
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Flush Log    central    0
    Ctn Clear Broker Logs

    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    ${start}    Ctn Get Round Current Date
    ${result}    Ctn In Bbdo2
    Should Not Be True    ${result}    We should be in BBDO3 in this test (newGeneration=True).

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established

    ${content}    Create List    sending DiffState to poller 1    BBDO: received diff state ack
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No new Engine configuration found in central cbd log

    Ctn Stop Engine
    ${content}    Create List    unified_sql: Disabling poller
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No stop event processed by central cbd
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker

BECSS_GRPC3
    [Documentation]     Scenario: Broker sends configuration to engine in new generation
    ...    Given an engine configuration is provided to the broker
    ...    And the broker and engine are started in new generation (engine first)
    ...    And the protocol is bbdo3
    ...    When the broker detects the configuration for the engine
    ...    Then the broker sends the configuration to the engine
    ...    Then both broker and engine are stopped (engine first)
    [Tags]    broker    engine    start-stop    MON-153802
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Clear Broker Logs

    Ctn Start Engine    newGeneration=True
    Ctn Start Broker    newGeneration=True

    ${start}    Ctn Get Round Current Date
    ${result}    Ctn In Bbdo2
    Should Not Be True    ${result}    We should be in BBDO3 in this test (newGeneration=True).

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established

    ${content}    Create List    sending DiffState to poller 1    BBDO: received diff state ack
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No new Engine configuration found in central cbd log

    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database

    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker

BECSS_GRPC4
    [Documentation]     Scenario: Broker sends configuration to engine in new generation
    ...    Given an engine configuration is provided to the broker
    ...    And the broker and engine are started in new generation (engine first)
    ...    And the protocol is bbdo3
    ...    When the broker detects the configuration for the engine
    ...    Then the broker sends the configuration to the engine
    ...    Then both broker and engine are stopped (broker first)
    [Tags]    broker    engine    start-stop
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Clear Broker Logs

    Ctn Start Engine    newGeneration=True
    Ctn Start Broker    newGeneration=True

    ${start}    Ctn Get Round Current Date
    ${result}    Ctn In Bbdo2
    Should Not Be True    ${result}    We should be in BBDO3 in this test (newGeneration=True).

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established

    ${content}    Create List    sending DiffState to poller 1    BBDO: received diff state ack
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No new Engine configuration found in central cbd log

    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database

    Ctn Kindly Stop Broker
    Ctn Stop Engine

BECSS_GRPC_COMPRESS1
    [Documentation]    Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
    [Tags]    broker    engine    start-stop
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Change Broker Compression Output    module0    central-module-master-output    yes
    Ctn Change Broker Compression Input    central    centreon-broker-master-input    yes
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Clear Broker Logs

    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    ${start}    Ctn Get Round Current Date
    ${result}    Ctn In Bbdo2
    Should Not Be True    ${result}    We should be in BBDO3 in this test.

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection not established between Engine and Broker

    ${content}    Create List    sending DiffState to poller 1    BBDO: received diff state ack
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No new Engine configuration found in central cbd log

    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker

BECSS_CRYPTED_GRPC1
    [Documentation]    Scenario: Repeated start/stop cycles with gRPC and mutual TLS in centralized configuration mode
    ...
    ...    Given a centralized Engine configuration with gRPC and server-side TLS encryption
    ...    When Broker and Engine are started for the first time
    ...    Then Broker detects the lock file, sends the configuration to Engine and receives the ack
    ...    And the database shows 50 enabled hosts and 1000 enabled services for poller 1
    ...    When Engine is stopped
    ...    Then all hosts for poller 1 are disabled in the database
    ...    When Broker and Engine are restarted (4 additional times)
    ...    Then both reload from their cached configuration files (.prot for Broker, state.prot for Engine)
    ...    And no new configuration is exchanged
    ...    And the database consistently shows 50 enabled hosts and 1000 enabled services
    [Tags]    broker    engine    start-stop
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Add Broker Tcp Output Grpc Crypto    module0    True    False
    Ctn Add Broker Tcp Input Grpc Crypto    central    True    False
    Ctn Remove Host From Broker Output    module0    central-module-master-output
    Ctn Add Host To Broker Output    module0    central-module-master-output    localhost
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Broker Logs
    Ctn Clear Prot Files

    FOR    ${i}    IN RANGE    0    5
        ${start}    Ctn Get Round Current Date
        Ctn Start Broker    newGeneration=True
        Ctn Start Engine    newGeneration=True
        ${result}    Ctn In Bbdo2
        Should Not Be True    ${result}    We should be in BBDO3 in this test.
        ${result}    Ctn Check Connections
        Should Be True    ${result}    Connection between Engine and Broker not established

        # On the first iteration an announcement is present, so Broker reads it
        # and sends the configuration to Engine.    On subsequent iterations the
        # announcement is gone -- Broker consumed it as soon as it had prepared
        # the configuration -- and both sides already know it (Broker from its
        # .prot file, Engine from state.prot).
        IF    ${i} == 0
            ${content}    Create List
            ...    sending DiffState to poller 1
            ...    BBDO: received diff state ack
            ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
            Should Be True    ${result}    No new Engine configuration found in central cbd log
        END

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Check Query Result
        ...    SELECT COUNT(*) FROM hosts WHERE instance_id=1 AND enabled=1
        ...    ==
        ...    ${50}
        ...    retry_timeout=30s
        ...    retry_pause=1s
        Check Query Result
        ...    SELECT COUNT(*) FROM resources WHERE parent_id=0 AND poller_id=1 AND enabled=1
        ...    ==
        ...    ${50}
        ...    retry_timeout=30s
        ...    retry_pause=1s
        Check Query Result
        ...    SELECT COUNT(*) FROM services s LEFT JOIN hosts h ON h.host_id=s.host_id AND s.enabled=1 WHERE h.instance_id=1
        ...    ==
        ...    ${1000}
        ...    retry_timeout=30s
        ...    retry_pause=1s
        Check Query Result
        ...    SELECT COUNT(*) FROM resources WHERE parent_id<>0 AND poller_id=1 AND enabled=1
        ...    ==
        ...    ${1000}
        ...    retry_timeout=30s
        ...    retry_pause=1s
        Disconnect From Database

        Ctn Stop Engine
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Check Query Result
        ...    SELECT COUNT(*) FROM hosts WHERE instance_id=1 AND enabled>0
        ...    ==
        ...    ${0}
        ...    retry_timeout=30s
        ...    retry_pause=1s
        Disconnect From Database

        Ctn Kindly Stop Broker
    END

BECSS_CRYPTED_GRPC2
    [Documentation]    Start-Stop grpc version Broker/Engine only server crypted
    [Tags]    broker    engine    start-stop
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Add Broker Tcp Input Grpc Crypto    central    True    False
    Ctn Clear Broker Logs
    Ctn Clear Prot Files

    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker    newGeneration=True
        Ctn Start Engine    newGeneration=True
        ${result}    Ctn In Bbdo2
        Should Not Be True    ${result}    We should be in BBDO3 in this test.
        Sleep    2s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

BECSS_CRYPTED_REVERSED_GRPC1
    [Documentation]    Start-Stop grpc version Broker/Engine - well configured
    [Tags]    broker    engine    start-stop
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Add Broker Tcp Output Grpc Crypto    module0    True    True
    Ctn Add Broker Tcp Input Grpc Crypto    central    True    True
    Ctn Add Host To Broker Input    central    central-broker-master-input    localhost
    Ctn Remove Host From Broker Output    module0    central-module-master-output
    Ctn Clear Broker Logs
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker    newGeneration=True
        Ctn Start Engine    newGeneration=True
        ${result}    Ctn In Bbdo2
        Should Not Be True    ${result}    We should be in BBDO3 in this test.
        ${result}    Ctn Check Connections
        Should Be True    ${result}    Connection between Engine and Broker not established
        Sleep    2s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

BECSS_CRYPTED_REVERSED_GRPC2
    [Documentation]    Start-Stop grpc version Broker/Engine only engine server crypted
    [Tags]    broker    engine    start-stop
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Add Broker Tcp Output Grpc Crypto    module0    True    True
    Ctn Add Host To Broker Input    central    central-broker-master-input    localhost
    Ctn Remove Host From Broker Output    module0    central-module-master-output
    Ctn Clear Broker Logs
    Log To Console    Waiting 2s to be sure configurations are written
    Sleep    2s
    #Ctn Stop Processes
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker    newGeneration=True
        Ctn Start Engine    newGeneration=True
        ${result}    Ctn In Bbdo2
        Should Not Be True    ${result}    We should be in BBDO3 in this test.
        Sleep    5s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

BECSS_CRYPTED_REVERSED_GRPC3
    [Documentation]    Start-Stop grpc version Broker/Engine only engine crypted
    [Tags]    broker    engine    start-stop
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Add Broker Tcp Input Grpc Crypto    central    True    True
    Ctn Add Host To Broker Input    central    central-broker-master-input    localhost
    Ctn Remove Host From Broker Output    module0    central-module-master-output
    Ctn Clear Broker Logs
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker    newGeneration=True
        Ctn Start Engine    newGeneration=True
        ${result}    Ctn In Bbdo2
        Should Not Be True    ${result}    We should be in BBDO3 in this test.
        Sleep    5s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

BECSS_ENGINE_DELETE_HOST
    [Documentation]    once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
    [Tags]    broker    engine    start-stop
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    module0    config    debug
    Ctn Broker Config Log    module0    core    off
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    module0    0
    Ctn Engine Config Set Value    ${0}    log_level_functions    trace
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Clear Retention
    Ctn Clear Broker Logs
    ${start}    Get Current Date
    Ctn Start Broker    True    True
    Ctn Start Engine    newGeneration=True
    ${result}    Ctn In Bbdo2
    Should Not Be True    ${result}    We should be in BBDO3 in this test.

    Ctn Wait For Engine To Be Ready    ${start}

    Ctn Kindly Stop Broker    True
    Ctn Start Broker    True    True

    ${start}    Ctn Get Round Current Date

    Ctn Engine Config Remove All Services From Host    ${0}    host_16
    Ctn Engine Config Remove Host    ${0}    host_16
    Ctn Notify Broker Of Engine Config Change    ${0}

    # What is checked is the delivery and its acknowledgement. Looking for the
    # announcement being read would be a race: it is consumed as soon as the
    # configuration is prepared, which happens without waiting for the poller, so
    # by the time Engine reconnects there is nothing left to find -- what waits
    # for it then is new-1.prot.
    ${content}    Create List    sending DiffState to poller 1    BBDO: received diff state ack
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No new Engine configuration found in central cbd log

    ${content}    Create List    Removing host 'host_16'.
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Host removal not found in engine log

    Ctn Kindly Stop Broker    True
    Ctn Stop Engine

BECSSBQ1
    [Documentation]    A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
    [Tags]    broker    engine    start-stop    queue
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    core    debug
    Ctn Clear Retention
    Ctn Clear Broker Logs
    ${start}    Ctn Get Round Current Date
    Ctn Create Bad Queue    central-broker-master.queue.central-broker-unified-sql
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    ${result}    Ctn In Bbdo2
    Should Not Be True    ${result}    We should be in BBDO3 in this test.

    ${content}    Create List
    ...    stream got corrupted compressed data, skipping next byte
    ...    processing pb service status
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should raise an error about the bad queue file and the process service events

    Ctn Stop Engine
    Ctn Kindly Stop Broker

Centralized_Start_Stop_Engine_Broker_${id}
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Centralized Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Log    central    core    debug
    Ctn Broker Config Log    central    processing    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Broker Logs
    IF    ${grpc}
        Ctn Change Broker Tcp Output To Grpc    central
        Ctn Change Broker Tcp Output To Grpc    module0
        Ctn Change Broker Tcp Input To Grpc    central
        Ctn Change Broker Tcp Input To Grpc    rrd
    END
    ${start}    Get Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    ${result}    Ctn In Bbdo2
    Should Not Be True    ${result}    We should be in BBDO3 in this test.
    ${content}    Create List    create feeder central-broker-master-input
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    create feeder not found
    ${result}    Ctn Check Connections
    Should Be True    ${result}    no connection between engine and cbd
    Sleep    5s
    ${start_stop}    Get Current Date
    Ctn Stop Engine
    ${content}    Create List    feeder 'central-broker-master-input-1', connection closed
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start_stop}    ${content}    60
    Should Be True    ${result}    connection closed not found

    Examples:    id    grpc    --
    ...    1    False
    ...    2    True
    Ctn Kindly Stop Broker

Centralized_Start_Stop_Broker_Engine_${id}
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Centralized Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Log    central    core    debug
    Ctn Broker Config Log    central    processing    debug
    IF    ${grpc}
        Ctn Change Broker Tcp Output To Grpc    central
        Ctn Change Broker Tcp Output To Grpc    module0
        Ctn Change Broker Tcp Input To Grpc    central
        Ctn Change Broker Tcp Input To Grpc    rrd
    END
    Ctn Clear Broker Logs
    ${start}    Ctn Get Round Current Date

    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    ${result}    Ctn In Bbdo2
    Should Not Be True    ${result}    We should be in BBDO3 in this test.
    ${content}    Create List    create feeder central-broker-master-input
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    create feeder not found
    ${result}    Ctn Check Connections
    Should Be True    ${result}    no connection between engine and cbd
    Sleep    5s
    ${stop_broker}    Get Current Date
    Ctn Kindly Stop Broker
    ${content}    Create List    failover central-module-master-output: connection closed
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${stop_broker}    ${content}    60
    Should Be True    ${result}    connection closed not found
    Examples:    id    grpc    --
    ...    1    False
    ...    2    True
    Ctn Stop Engine

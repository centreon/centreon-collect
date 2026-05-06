*** Settings ***
Documentation       Centreon Broker and Engine start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


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

    ${content}    Create List    Found lock file '/tmp/var/lib/centreon/config/1.lck' for poller id 1    sending DiffState to poller 1    BBDO: received diff state ack
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

    ${content}    Create List    Found lock file '/tmp/var/lib/centreon/config/1.lck' for poller id 1    sending DiffState to poller 1    BBDO: received diff state ack
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

    ${content}    Create List    Found lock file '/tmp/var/lib/centreon/config/1.lck' for poller id 1    sending DiffState to poller 1    BBDO: received diff state ack
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

    ${content}    Create List    Found lock file '/tmp/var/lib/centreon/config/1.lck' for poller id 1    sending DiffState to poller 1    BBDO: received diff state ack
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No new Engine configuration found in central cbd log

    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database

    Ctn Kindly Stop Broker
    Ctn Stop Engine

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

    ${content}    Create List    Found lock file '/tmp/var/lib/centreon/config/1.lck' for poller id 1    sending DiffState to poller 1    BBDO: received diff state ack
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

    ${content}    Create List    Found lock file '/tmp/var/lib/centreon/config/1.lck' for poller id 1    sending DiffState to poller 1    BBDO: received diff state ack
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

    ${content}    Create List    Found lock file '/tmp/var/lib/centreon/config/1.lck' for poller id 1    sending DiffState to poller 1    BBDO: received diff state ack
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

    ${content}    Create List    Found lock file '/tmp/var/lib/centreon/config/1.lck' for poller id 1    sending DiffState to poller 1    BBDO: received diff state ack
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

    ${content}    Create List    Found lock file '/tmp/var/lib/centreon/config/1.lck' for poller id 1    sending DiffState to poller 1    BBDO: received diff state ack
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

        # On the first iteration the .lck file is present so Broker processes
        # the new configuration and sends it to Engine.  On subsequent iterations
        # the .lck file has been deleted by Broker; both sides already know the
        # configuration (Broker from its .prot file, Engine from state.prot).
        IF    ${i} == 0
            ${content}    Create List
            ...    Found lock file '/tmp/var/lib/centreon/config/1.lck' for poller id 1
            ...    sending DiffState to poller 1
            ...    BBDO: received diff state ack
            ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
            Should Be True    ${result}    No new Engine configuration found in central cbd log
        END

        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Check Query Result    SELECT COUNT(*) FROM hosts WHERE instance_id=1 AND enabled=1    ==    ${50}    retry_timeout=30s    retry_pause=1s
        Check Query Result    SELECT COUNT(*) FROM resources WHERE parent_id=0 AND poller_id=1 AND enabled=1    ==    ${50}    retry_timeout=30s    retry_pause=1s
        Check Query Result    SELECT COUNT(*) FROM services s LEFT JOIN hosts h ON h.host_id=s.host_id AND s.enabled=1 WHERE h.instance_id=1    ==    ${1000}    retry_timeout=30s    retry_pause=1s
        Check Query Result    SELECT COUNT(*) FROM resources WHERE parent_id<>0 AND poller_id=1 AND enabled=1    ==    ${1000}    retry_timeout=30s    retry_pause=1s
        Disconnect From Database

        Ctn Stop Engine
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Check Query Result    SELECT COUNT(*) FROM hosts WHERE instance_id=1 AND enabled>0    ==    ${0}    retry_timeout=30s    retry_pause=1s
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

    ${content}    Create List    Found lock file '/tmp/var/lib/centreon/config/1.lck' for poller id 1    sending DiffState to poller 1    BBDO: received diff state ack
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

    ${content}    Create List    stream got corrupted compressed data, skipping next byte    processing pb service status
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
    Ctn Start Engine	newGeneration=True
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

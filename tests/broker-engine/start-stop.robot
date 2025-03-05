*** Settings ***
Documentation       Centreon Broker and Engine start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BESS1
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}
    Ctn Kindly Stop Broker
    Ctn Stop Engine

BESS2
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    info
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${content}    Create List    SQL: Disabling poller
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No stop event processed by central cbd
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker

BESS2U
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Engine stopped first.
    ...    Unified_sql is used.
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    info
    Ctn Broker Config Log    central    bbdo    info
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${content}    Create List    unified_sql: Disabling poller
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No stop event processed by central cbd
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker

BESS3
    [Documentation]    Start-Stop Broker/Engine - Engine started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Start Engine
    Ctn Start Broker
    ${result}    Ctn Check Connections
    Should Be True    ${result}
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}
    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}
    Ctn Kindly Stop Broker

BESS4
    [Documentation]    Start-Stop Broker/Engine - Engine started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Start Engine
    Ctn Start Broker
    ${result}    Ctn Check Connections
    Should Be True    ${result}
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}
    Ctn Kindly Stop Broker
    Ctn Stop Engine

BESS5
    [Documentation]    Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Engine Config Set Value    ${0}    debug_level    ${-1}
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Broker and Engine seem not connected
    [Teardown]    Ctn Stop Engine Broker And Save Logs

BESS_GRPC1
    [Documentation]    Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}
    Ctn Kindly Stop Broker
    Ctn Stop Engine

BESS_GRPC2
    [Documentation]    Start-Stop grpc version Broker/Engine - Broker started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connections between Engine and Broker not established
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker

BESS_GRPC3
    [Documentation]    Start-Stop grpc version Broker/Engine - Engine started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Start Engine
    Ctn Start Broker
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connections between Engine and Broker not established
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker

BESS_GRPC4
    [Documentation]    Start-Stop grpc version Broker/Engine - Engine started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Start Engine
    Ctn Start Broker
    ${result}    Ctn Check Connections
    Should Be True    ${result}
    Ctn Kindly Stop Broker
    Ctn Stop Engine

BESS_GRPC5
    [Documentation]    Start-Stop grpc version Broker/engine - Engine debug level is set to all, it should not hang
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Engine Config Set Value    ${0}    debug_level    ${-1}
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connections between Engine and Broker not established
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker

BESS_GRPC_COMPRESS1
    [Documentation]    Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Change Broker Tcp Output To Grpc    central
    Ctn Change Broker Tcp Output To Grpc    module0
    Ctn Change Broker Tcp Input To Grpc    central
    Ctn Change Broker Tcp Input To Grpc    rrd
    Ctn Change Broker Compression Output    module0    central-module-master-output    yes
    Ctn Change Broker Compression Input    central    centreon-broker-master-input    yes
    Ctn Start Broker
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection not established between Engine and Broker
    ${result}    Ctn Check Poller Enabled In Database    1    10
    Should Be True    ${result}    Poller not visible in database
    Ctn Stop Engine
    ${result}    Ctn Check Poller Disabled In Database    1    10
    Should Be True    ${result}    Poller still visible in database
    Ctn Kindly Stop Broker

BESS_CRYPTED_GRPC1
    [Documentation]    Start-Stop grpc version Broker/Engine - well configured
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
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
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        ${result}    Ctn Check Connections
        Should Be True    ${result}
        ${result}    Ctn Check Poller Enabled In Database    1    10
        Should Be True    ${result}
        Ctn Stop Engine
        ${result}    Ctn Check Poller Disabled In Database    1    10
        Should Be True    ${result}
        Ctn Kindly Stop Broker
    END

BESS_CRYPTED_GRPC2
    [Documentation]    Start-Stop grpc version Broker/Engine only server crypted
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
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
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        Sleep    2s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

BESS_CRYPTED_GRPC3
    [Documentation]    Start-Stop grpc version Broker/Engine only engine crypted
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
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
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        Sleep    2s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

BESS_CRYPTED_REVERSED_GRPC1
    [Documentation]    Start-Stop grpc version Broker/Engine - well configured
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
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
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        ${result}    Ctn Check Connections
        Should Be True    ${result}
        Sleep    2s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

BESS_CRYPTED_REVERSED_GRPC2
    [Documentation]    Start-Stop grpc version Broker/Engine only engine server crypted
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
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
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        Sleep    5s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

BESS_CRYPTED_REVERSED_GRPC3
    [Documentation]    Start-Stop grpc version Broker/Engine only engine crypted
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
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
    FOR    ${i}    IN RANGE    0    5
        Ctn Start Broker
        Ctn Start Engine
        Sleep    5s
        Ctn Kindly Stop Broker
        Ctn Stop Engine
    END

BESS_ENGINE_DELETE_HOST
    [Documentation]    once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker    True
    Ctn Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    Ctn Kindly Stop Broker    True
    Ctn Start Broker    True
    Ctn Engine Config Remove Service Host    ${0}    host_16
    Ctn Engine Config Remove Host    ${0}    host_16
    Ctn Reload Engine
    Sleep    2s
    Ctn Kindly Stop Broker    True
    Ctn Stop Engine

BESSBQ1
    [Documentation]    A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
    [Tags]    broker    engine    start-stop    queue
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    core    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    Ctn Create Bad Queue    central-broker-master.queue.central-broker-master-sql
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    execute statement 1245300e

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    120
    Should Be True    ${result}    Services should be updated after the ingestion of the queue file
    Ctn Stop Engine
    Ctn Kindly Stop Broker

Start_Stop_Engine_Broker_${id}
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Log    central    core    debug
    Ctn Broker Config Log    central    processing    debug
    Ctn Config Broker Sql Output    central    unified_sql
    IF    ${grpc}
        Ctn Change Broker Tcp Output To Grpc    central
        Ctn Change Broker Tcp Output To Grpc    module0
        Ctn Change Broker Tcp Input To Grpc    central
        Ctn Change Broker Tcp Input To Grpc    rrd
    END
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
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

Start_Stop_Broker_Engine_${id}
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    Ctn Config Engine    ${1}    ${1}    ${1}
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
    ${start}    Ctn Get Round Current Date

    Ctn Start Broker
    Ctn Start Engine
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

BESSG
    [Documentation]    Scenario: Broker handles connection and disconnection with Engine
    ...    Given Broker is configured with only one output that is Graphite
    ...    When the Engine starts and connects to the Broker
    ...    Then the Broker must be able to handle the connection
    ...    When the Engine stops
    ...    Then the Broker must be able to handle the disconnection

    [Tags]    broker    engine    start-stop    MON-161611
    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1    3.0.1    True
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Remove Output    central    central-broker-unified-sql
    Ctn Broker Config Remove Output    central    centreon-broker-master-rrd
    Ctn Broker Config Add Output    central    { "name": "graphite-output", "db_host": "localhost", "db_port": "2003", "type": "graphite", "db_password": "", "queries_per_transaction": "1000", "metric_naming": "nagios.host.$HOST$.service.$SERVICE$.perfdata.$METRIC$", "status_naming": "nagios.host.$HOST$.service.$SERVICE$.metadata.state" }
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    ${True}
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}    1
    Ctn Stop Engine
    Ctn Kindly Stop Broker    ${True}

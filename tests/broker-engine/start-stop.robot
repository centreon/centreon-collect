*** Settings ***
Documentation       Centreon Broker and Engine start/stop tests

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save logs If Failed


*** Test Cases ***
BESS1
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}
    Kindly Stop Broker
    Stop Engine

BESS2
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}
    ${result}    Check Poller Enabled In Database    1    10
    Should Be True    ${result}
    Stop Engine
    ${result}    Check Poller Disabled In Database    1    10
    Should Be True    ${result}
    Kindly Stop Broker

BESS3
    [Documentation]    Start-Stop Broker/Engine - Engine started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Start Engine
    Start Broker
    ${result}    Check Connections
    Should Be True    ${result}
    ${result}    Check Poller Enabled In Database    1    10
    Should Be True    ${result}
    Stop Engine
    ${result}    Check Poller Disabled In Database    1    10
    Should Be True    ${result}
    Kindly Stop Broker

BESS4
    [Documentation]    Start-Stop Broker/Engine - Engine started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Start Engine
    Start Broker
    ${result}    Check Connections
    Should Be True    ${result}
    ${result}    Check Poller Enabled In Database    1    10
    Should Be True    ${result}
    Kindly Stop Broker
    Stop Engine

BESS5
    [Documentation]    Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Engine Config Set Value    ${0}    debug_level    ${-1}
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Broker and Engine seem not connected
    [Teardown]    Stop Engine Broker And Save Logs

BESS_GRPC1
    [Documentation]    Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Change Broker Tcp Output To Grpc    central
    Change Broker Tcp Output To Grpc    module0
    Change Broker Tcp Input To Grpc    central
    Change Broker Tcp Input To Grpc    rrd
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}
    Kindly Stop Broker
    Stop Engine

BESS_GRPC2
    [Documentation]    Start-Stop grpc version Broker/Engine - Broker started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Change Broker Tcp Output To Grpc    central
    Change Broker Tcp Output To Grpc    module0
    Change Broker Tcp Input To Grpc    central
    Change Broker Tcp Input To Grpc    rrd
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}
    ${result}    Check Poller Enabled In Database    1    10
    Should Be True    ${result}
    Stop Engine
    ${result}    Check Poller Disabled In Database    1    10
    Should Be True    ${result}
    Kindly Stop Broker

BESS_GRPC3
    [Documentation]    Start-Stop grpc version Broker/Engine - Engine started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Change Broker Tcp Output To Grpc    central
    Change Broker Tcp Output To Grpc    module0
    Change Broker Tcp Input To Grpc    central
    Change Broker Tcp Input To Grpc    rrd
    Start Engine
    Start Broker
    ${result}    Check Connections
    Should Be True    ${result}
    ${result}    Check Poller Enabled In Database    1    10
    Should Be True    ${result}
    Stop Engine
    ${result}    Check Poller Disabled In Database    1    10
    Should Be True    ${result}
    Kindly Stop Broker

BESS_GRPC4
    [Documentation]    Start-Stop grpc version Broker/Engine - Engine started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Change Broker Tcp Output To Grpc    central
    Change Broker Tcp Output To Grpc    module0
    Change Broker Tcp Input To Grpc    central
    Change Broker Tcp Input To Grpc    rrd
    Start Engine
    Start Broker
    ${result}    Check Connections
    Should Be True    ${result}
    Kindly Stop Broker
    Stop Engine

BESS_GRPC5
    [Documentation]    Start-Stop grpc version Broker/engine - Engine debug level is set to all, it should not hang
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Engine Config Set Value    ${0}    debug_level    ${-1}
    Change Broker Tcp Output To Grpc    central
    Change Broker Tcp Output To Grpc    module0
    Change Broker Tcp Input To Grpc    central
    Change Broker Tcp Input To Grpc    rrd
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}
    ${result}    Check Poller Enabled In Database    1    10
    Should Be True    ${result}
    Stop Engine
    ${result}    Check Poller Disabled In Database    1    10
    Should Be True    ${result}
    Kindly Stop Broker

BESS_GRPC_COMPRESS1
    [Documentation]    Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Change Broker Tcp Output To Grpc    central
    Change Broker Tcp Output To Grpc    module0
    Change Broker Tcp Input To Grpc    central
    Change Broker Tcp Input To Grpc    rrd
    Change Broker Compression Output    module0    central-module-master-output    yes
    Change Broker Compression Input    central    centreon-broker-master-input    yes
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}
    ${result}    Check Poller Enabled In Database    1    10
    Should Be True    ${result}
    Stop Engine
    ${result}    Check Poller Disabled In Database    1    10
    Should Be True    ${result}
    Kindly Stop Broker

BESS_CRYPTED_GRPC1
    [Documentation]    Start-Stop grpc version Broker/Engine - well configured
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Change Broker Tcp Output To Grpc    central
    Change Broker Tcp Output To Grpc    module0
    Change Broker Tcp Input To Grpc    central
    Change Broker Tcp Input To Grpc    rrd
    Add Broker Tcp Output Grpc Crypto    module0    True    False
    Add Broker Tcp Input Grpc Crypto    central    True    False
    Remove Host From Broker Output    module0    central-module-master-output
    Add Host To Broker Output    module0    central-module-master-output    localhost
    FOR    ${i}    IN RANGE    0    5
        Start Broker
        Start Engine
        ${result}    Check Connections
        Should Be True    ${result}
        ${result}    Check Poller Enabled In Database    1    10
        Should Be True    ${result}
        Stop Engine
        ${result}    Check Poller Disabled In Database    1    10
        Should Be True    ${result}
        Kindly Stop Broker
    END

BESS_CRYPTED_GRPC2
    [Documentation]    Start-Stop grpc version Broker/Engine only server crypted
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Change Broker Tcp Output To Grpc    central
    Change Broker Tcp Output To Grpc    module0
    Change Broker Tcp Input To Grpc    central
    Change Broker Tcp Input To Grpc    rrd
    Add Broker Tcp Input Grpc Crypto    central    True    False
    FOR    ${i}    IN RANGE    0    5
        Start Broker
        Start Engine
        Sleep    2s
        Kindly Stop Broker
        Stop Engine
    END

BESS_CRYPTED_GRPC3
    [Documentation]    Start-Stop grpc version Broker/Engine only engine crypted
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Change Broker Tcp Output To Grpc    central
    Change Broker Tcp Output To Grpc    module0
    Change Broker Tcp Input To Grpc    central
    Change Broker Tcp Input To Grpc    rrd
    Add Broker Tcp Output Grpc Crypto    module0    True    False
    FOR    ${i}    IN RANGE    0    5
        Start Broker
        Start Engine
        Sleep    2s
        Kindly Stop Broker
        Stop Engine
    END

BESS_CRYPTED_REVERSED_GRPC1
    [Documentation]    Start-Stop grpc version Broker/Engine - well configured
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Change Broker Tcp Output To Grpc    central
    Change Broker Tcp Output To Grpc    module0
    Change Broker Tcp Input To Grpc    central
    Change Broker Tcp Input To Grpc    rrd
    Add Broker Tcp Output Grpc Crypto    module0    True    True
    Add Broker Tcp Input Grpc Crypto    central    True    True
    Add Host To Broker Input    central    central-broker-master-input    localhost
    Remove Host From Broker Output    module0    central-module-master-output
    FOR    ${i}    IN RANGE    0    5
        Start Broker
        Start Engine
        ${result}    Check Connections
        Should Be True    ${result}
        Sleep    2s
        Kindly Stop Broker
        Stop Engine
    END

BESS_CRYPTED_REVERSED_GRPC2
    [Documentation]    Start-Stop grpc version Broker/Engine only engine server crypted
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.key    /tmp/
    Copy File    ../broker/grpc/test/grpc_test_keys/server_1234.crt    /tmp/
    Change Broker Tcp Output To Grpc    central
    Change Broker Tcp Output To Grpc    module0
    Change Broker Tcp Input To Grpc    central
    Change Broker Tcp Input To Grpc    rrd
    Add Broker Tcp Output Grpc Crypto    module0    True    True
    Add Host To Broker Input    central    central-broker-master-input    localhost
    Remove Host From Broker Output    module0    central-module-master-output
    FOR    ${i}    IN RANGE    0    5
        Start Broker
        Start Engine
        Sleep    5s
        Kindly Stop Broker
        Stop Engine
    END

BESS_CRYPTED_REVERSED_GRPC3
    [Documentation]    Start-Stop grpc version Broker/Engine only engine crypted
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Copy File    ../broker/grpc/test/grpc_test_keys/ca_1234.crt    /tmp/
    Change Broker Tcp Output To Grpc    central
    Change Broker Tcp Output To Grpc    module0
    Change Broker Tcp Input To Grpc    central
    Change Broker Tcp Input To Grpc    rrd
    Add Broker Tcp Input Grpc Crypto    central    True    True
    Add Host To Broker Input    central    central-broker-master-input    localhost
    Remove Host From Broker Output    module0    central-module-master-output
    FOR    ${i}    IN RANGE    0    5
        Start Broker
        Start Engine
        Sleep    5s
        Kindly Stop Broker
        Stop Engine
    END

BESS_ENGINE_DELETE_HOST
    [Documentation]    once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}
    Config Broker    central
    Config Broker    module
    Clear Retention
    ${start}    Get Current Date
    Start Broker    True
    Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    Kindly Stop Broker    True
    Start Broker    True
    Engine Config Remove Service Host    ${0}    host_16
    Engine Config Remove Host    ${0}    host_16
    Reload Engine
    Sleep    2s
    Kindly Stop Broker    True
    Stop Engine

BESSBQ1
    [Documentation]    A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
    [Tags]    broker    engine    start-stop    queue
    Config Engine    ${1}
    Config Broker    central
    Config Broker    rrd
    Config Broker    module
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    bbdo    debug
    Broker Config Log    central    sql    trace
    Broker Config Log    central    core    debug
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    Create Bad Queue    central-broker-master.queue.central-broker-master-sql
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List    execute statement 306524174

    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    120
    Should Be True    ${result}    Services should be updated after the ingestion of the queue file
    Stop Engine
    Kindly Stop Broker

Start_Stop_Engine_Broker_${id}
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Broker stopped first
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}    ${1}    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    debug
    Broker Config Log    central    processing    info
    Config Broker Sql Output    central    unified_sql
    IF    ${grpc}
        Change Broker Tcp Output To Grpc    central
        Change Broker Tcp Output To Grpc    module0
        Change Broker Tcp Input To Grpc    central
        Change Broker Tcp Input To Grpc    rrd
    END
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List    create feeder central-broker-master-input
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    create feeder not found
    ${result}    Check Connections
    Should Be True    ${result}    no connection between engine and cbd
    Sleep    5s
    ${start_stop}    Get Current Date
    Stop Engine
    ${content}    Create List    feeder 'central-broker-master-input-1', connection closed
    ${result}    Find In Log With Timeout    ${centralLog}    ${start_stop}    ${content}    60
    Should Be True    ${result}    connection closed not found

    Examples:    id    grpc    --
    ...    1    False
    ...    2    True
    Kindly Stop Broker

Start_Stop_Broker_Engine_${id}
    [Documentation]    Start-Stop Broker/Engine - Broker started first - Engine stopped first
    [Tags]    broker    engine    start-stop
    Config Engine    ${1}    ${1}    ${1}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    debug
    IF    ${grpc}
        Change Broker Tcp Output To Grpc    central
        Change Broker Tcp Output To Grpc    module0
        Change Broker Tcp Input To Grpc    central
        Change Broker Tcp Input To Grpc    rrd
    END
    ${start}    Get Current Date


    Start Broker
    Start Engine
    ${content}    Create List    create feeder central-broker-master-input
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    create feeder not found
    ${result}    Check Connections
    Should Be True    ${result}    no connection between engine and cbd
    Sleep    5s
    ${stop_broker}    Get Current Date
    Kindly Stop Broker
    ${content}    Create List    failover central-module-master-output: connection closed
    ${result}    Find In Log With Timeout    ${moduleLog0}    ${stop_broker}    ${content}    60
    Should Be True    ${result}    connection closed not found
    Examples:    id    grpc    --
    ...    1    False
    ...    2    True
    Stop Engine

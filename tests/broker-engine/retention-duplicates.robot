*** Settings ***
Documentation       Centreon Broker tests on dublicated data that could come from retention when centengine or cbd are restarted

Resource            ../resources/resources.robot
Library             Process
Library             DateTime
Library             OperatingSystem
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py
Library             ../resources/specific-duplication.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Test Cases ***
BERD1
    [Documentation]    Starting/stopping Broker does not create duplicated events.
    [Tags]    broker    engine    start-stop    duplicate    retention
    Config Engine    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Config Broker    central
    Broker Config Clear Outputs Except    central    ["ipv4"]
    Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Broker Config Log    central    lua    debug
    Broker Config Flush Log    central    0
    Config Broker    module
    Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Broker Config Log    module0    lua    debug
    Config Broker    rrd
    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Find In Log With Timeout    ${moduleLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected.
    Sleep    5s
    Kindly Stop Broker
    Sleep    5s
    Clear Cache
    Start Broker
    Sleep    25s
    Stop Engine
    Kindly Stop Broker
    ${result}    Files Contain Same Json    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    Contents of /tmp/lua.log and /tmp/lua-engine.log do not match.
    ${result}    Check Multiplicity When Broker Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERD2
    [Documentation]    Starting/stopping Engine does not create duplicated events.
    [Tags]    broker    engine    start-stop    duplicate    retention
    Config Engine    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Engine Config Set Value    ${0}    log_level_runtime    info
    Config Broker    central
    Broker Config Clear Outputs Except    central    ["ipv4"]
    Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Broker Config Log    central    lua    debug
    Broker Config Flush Log    central    0
    Config Broker    module
    Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Broker Config Log    module0    lua    debug
    Broker Config Log    module0    neb    debug
    Config Broker    rrd
    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Find In Log With Timeout    ${moduleLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected.
    Sleep    15s
    Stop Engine
    Start Engine
    Sleep    25s
    Stop Engine
    Kindly Stop Broker
    ${result}    Files Contain Same Json    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    Contents of /tmp/lua.log and /tmp/lua-engine.log do not match.
    ${result}    Check Multiplicity When Engine Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERDUC1
    [Documentation]    Starting/stopping Broker does not create duplicated events in usual cases
    [Tags]    broker    engine    start-stop    duplicate    retention
    Config Engine    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Config Broker    central
    Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Broker Config Log    central    lua    debug
    Broker Config Log    central    perfdata    debug
    Broker Config Log    central    sql    debug
    Broker Config Flush Log    central    0

    Config Broker    module
    Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Broker Config Log    module0    lua    debug
    Broker Config Log    module0    neb    debug
    Config Broker    rrd
    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Find In Log With Timeout    ${moduleLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected.
    Sleep    5s
    Kindly Stop Broker
    Sleep    5s
    Clear Cache
    Start Broker
    Sleep    25s
    Stop Engine
    Kindly Stop Broker
    ${result}    Check Multiplicity When Broker Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERDUCU1
    [Documentation]    Starting/stopping Broker does not create duplicated events in usual cases with unified_sql
    [Tags]    broker    engine    start-stop    duplicate    retention
    Config Engine    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Config Broker    central
    Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Broker Config Log    central    lua    debug
    Config Broker Sql Output    central    unified_sql
    Config Broker    module
    Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Broker Config Log    module0    lua    debug
    Broker Config Flush Log    central    0
    Broker Config Flush Log    rrd    0
    Broker Config Flush Log    module0    0
    Config Broker    rrd
    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Find In Log With Timeout    ${moduleLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine
    Sleep    5s
    Kindly Stop Broker
    Sleep    5s
    Clear Cache
    Start Broker
    Sleep    25s
    Stop Engine
    Kindly Stop Broker
    ${result}    Check Multiplicity When Broker Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERDUC2
    [Documentation]    Starting/stopping Engine does not create duplicated events in usual cases
    [Tags]    broker    engine    start-stop    duplicate    retention
    Clear Retention
    Config Engine    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Config Broker    central
    Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Broker Config Log    central    lua    debug
    Config Broker    module
    Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Broker Config Log    module0    lua    debug
    Broker Config Flush Log    central    0
    Broker Config Flush Log    module0    0
    Config Broker    rrd
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Find In Log With Timeout    ${moduleLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected.
    Sleep    5s
    Stop Engine
    Sleep    5s
    Clear Cache
    Start Engine
    Sleep    25s
    Stop Engine
    Kindly Stop Broker
    ${result}    Check Multiplicity When Engine Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERDUCU2
    [Documentation]    Starting/stopping Engine does not create duplicated events in usual cases with unified_sql
    [Tags]    broker    engine    start-stop    duplicate    retention
    Clear Retention
    Config Engine    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Broker Config Log    central    lua    debug
    Broker Config Log    central    sql    trace
    Config Broker    module
    Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Broker Config Log    module0    lua    debug
    Broker Config Flush Log    central    0
    Broker Config Flush Log    module0    0
    Config Broker    rrd
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Find In Log With Timeout    ${moduleLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected.
    Sleep    5s
    Stop Engine
    Sleep    5s
    Clear Cache
    Start Engine
    Sleep    25s
    Stop Engine
    Kindly Stop Broker
    ${result}    Check Multiplicity When Engine Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERDUC3U1
    [Documentation]    Starting/stopping Broker does not create duplicated events in usual cases with unified_sql and BBDO 3.0
    [Tags]    broker    engine    start-stop    duplicate    retention
    Config Engine    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Config Broker    central
    Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Broker Config Log    central    lua    debug
    Config Broker Sql Output    central    unified_sql
    Config Broker    module
    Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Broker Config Flush Log    central    0
    Broker Config Flush Log    module0    0
    Config Broker    rrd
    Config BBDO3    1
    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Find In Log With Timeout    ${moduleLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected.
    Sleep    5s
    Kindly Stop Broker
    Sleep    5s
    Clear Cache
    Start Broker
    Sleep    25s
    Stop Engine
    Kindly Stop Broker
    ${result}    Check Multiplicity When Broker Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERDUC3U2
    [Documentation]    Starting/stopping Engine does not create duplicated events in usual cases with unified_sql and BBDO 3.0
    [Tags]    broker    engine    start-stop    duplicate    retention
    Clear Retention
    Config Engine    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Broker Config Log    central    lua    debug
    Broker Config Log    central    sql    trace
    Config Broker    module
    Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Broker Config Log    module0    lua    debug
    Broker Config Flush Log    central    0
    Broker Config Flush Log    module0    0
    Config Broker    rrd
    Config BBDO3    1
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the lua to be correctly initialized
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Find In Log With Timeout    ${moduleLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine

    # Let's wait for all the services configuration.
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60

    ${start}    Get Round Current Date
    # Let's wait for a first service status.
    ${content}    Create List    SQL: pb service .* status .* type .* check result output
    ${result}    Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result[0]}    We did not get any pb service status for 60s

    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected.
    Sleep    5s
    Stop Engine
    Sleep    5s
    Clear Cache
    Start Engine
    Sleep    25s
    Stop Engine
    Kindly Stop Broker
    ${result}    Check Multiplicity When Engine Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERDUCA300
    [Documentation]    Starting/stopping Engine is stopped ; it should emit a stop event and receive an ack event with events to clean from broker.
    [Tags]    broker    engine    start-stop    duplicate    retention    unified_sql
    Clear Retention
    Config Engine    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Broker Config Log    central    config    debug
    Broker Config Log    central    bbdo    trace
    Broker Config Log    central    tcp    trace
    Config Broker    module
    Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Broker Config Log    module0    config    debug
    Broker Config Log    module0    bbdo    trace
    Broker Config Flush Log    central    0
    Broker Config Flush Log    module0    0
    Config Broker    rrd
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    ${start}    Get Current Date
    Start Broker
    Start Engine

    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected.

    # Let's wait for all the services configuration.
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60

    Stop Engine
    ${content}    Create List    BBDO: sending pb stop packet to peer
    ${result}    Find In Log With Timeout    ${moduleLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Engine should send a pb stop message to cbd.

    ${content}    Create List    BBDO: received pb stop from peer
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker should receive a pb stop message from engine.

    ${content}    Create List    send acknowledgement for [0-9]+ events
    ${result}    Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result[0]}    Broker should send an ack for handled events.

    ${content}    Create List    BBDO: received acknowledgement for [0-9]+ events before finishing
    ${result}    Find Regex In Log With Timeout    ${moduleLog0}    ${start}    ${content}    30
    Should Be True    ${result[0]}    Engine should receive an ack for handled events from broker.

    Kindly Stop Broker

BERDUCA301
    [Documentation]    Starting/stopping Engine is stopped ; it should emit a stop event and receive an ack event with events to clean from broker with bbdo 3.0.1.
    [Tags]    broker    engine    start-stop    duplicate    retention    unified_sql
    Clear Retention
    Config Engine    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Config Broker    central
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Broker Config Log    central    config    debug
    Broker Config Log    central    bbdo    trace
    Broker Config Log    central    tcp    trace
    Config Broker    module
    Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Broker Config Log    module0    config    debug
    Broker Config Log    module0    bbdo    trace
    Broker Config Flush Log    central    0
    Broker Config Flush Log    module0    0
    Config Broker    rrd

    Config BBDO3    1
    ${start}    Get Current Date

    Start Broker
    Start Engine

    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected.

    # Let's wait for all the services configuration.
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60

    Stop Engine
    ${content}    Create List    BBDO: sending pb stop packet to peer
    ${result}    Find In Log With Timeout    ${moduleLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Engine should send a pb stop message to cbd.

    ${content}    Create List    BBDO: received pb stop from peer
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker should receive a pb stop message from engine.

    ${content}    Create List    send pb acknowledgement for [0-9]+ events
    ${result}    Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result[0]}    Broker should send an ack for handled events.

    ${content}    Create List    BBDO: received acknowledgement for [0-9]+ events before finishing
    ${result}    Find Regex In Log With Timeout    ${moduleLog0}    ${start}    ${content}    30
    Should Be True    ${result[0]}    Engine should receive an ack for handled events from broker.

    Kindly Stop Broker

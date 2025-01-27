*** Settings ***
Documentation       Centreon Broker tests on dublicated data that could come from retention when centengine or cbd are restarted

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BERD1
    [Documentation]    Starting/stopping Broker does not create duplicated events.
    [Tags]    broker    engine    start-stop    duplicate    retention
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Config Broker    central
    Ctn Broker Config Clear Outputs Except    central    ["ipv4"]
    Ctn Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Ctn Broker Config Log    central    lua    debug
    Ctn Broker Config Flush Log    central    0
    Ctn Config Broker    module
    Ctn Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Ctn Broker Config Log    module0    lua    debug
    Ctn Config Broker    rrd
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected.
    Sleep    5s
    Ctn Kindly Stop Broker
    Sleep    5s
    Ctn Clear Cache
    Ctn Start Broker
    Sleep    25s
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    ${result}    Ctn Files Contain Same Json    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    Contents of /tmp/lua.log and /tmp/lua-engine.log do not match.
    ${result}    Ctn Check Multiplicity When Broker Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERD2
    [Documentation]    Starting/stopping Engine does not create duplicated events.
    [Tags]    broker    engine    start-stop    duplicate    retention
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_runtime    info
    Ctn Config Broker    central
    Ctn Broker Config Clear Outputs Except    central    ["ipv4"]
    Ctn Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Ctn Broker Config Log    central    lua    debug
    Ctn Broker Config Flush Log    central    0
    Ctn Config Broker    module
    Ctn Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Ctn Broker Config Log    module0    lua    debug
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Config Broker    rrd
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected.
    Log To Console    Engine and Broker talk during 15s.
    Sleep    15s
    Ctn Stop Engine
    Ctn Start Engine
    Log To Console    Engine has been restart and now they talk during 25s.
    Sleep    25s
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    ${result}    Ctn Files Contain Same Json    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    Contents of /tmp/lua.log and /tmp/lua-engine.log do not match.
    ${result}    Ctn Check Multiplicity When Engine Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERDUC1
    [Documentation]    Starting/stopping Broker does not create duplicated events in usual cases
    [Tags]    broker    engine    start-stop    duplicate    retention
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Config Broker    central
    Ctn Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Ctn Broker Config Log    central    lua    debug
    Ctn Broker Config Log    central    perfdata    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Flush Log    central    0

    Ctn Config Broker    module
    Ctn Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Ctn Broker Config Log    module0    lua    debug
    Ctn Broker Config Log    module0    neb    debug
    Ctn Config Broker    rrd
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected.
    Sleep    5s
    Ctn Kindly Stop Broker
    Sleep    5s
    Ctn Clear Cache
    Ctn Start Broker
    Sleep    25s
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    ${result}    Ctn Check Multiplicity When Broker Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERDUCU1
    [Documentation]    Starting/stopping Broker does not create duplicated events in usual cases with unified_sql
    [Tags]    broker    engine    start-stop    duplicate    retention
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Config Broker    central
    Ctn Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Ctn Broker Config Log    central    lua    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker    module
    Ctn Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Ctn Broker Config Log    module0    lua    debug
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    rrd    0
    Ctn Broker Config Flush Log    module0    0
    Ctn Config Broker    rrd
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine
    Sleep    5s
    Ctn Kindly Stop Broker
    Sleep    5s
    Ctn Clear Cache
    Ctn Start Broker
    Sleep    25s
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    ${result}    Ctn Check Multiplicity When Broker Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERDUC2
    [Documentation]    Starting/stopping Engine does not create duplicated events in usual cases
    [Tags]    broker    engine    start-stop    duplicate    retention
    Ctn Clear Retention
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Config Broker    central
    Ctn Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Ctn Broker Config Log    central    lua    debug
    Ctn Config Broker    module
    Ctn Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Ctn Broker Config Log    module0    lua    debug
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    module0    0
    Ctn Config Broker    rrd
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected.
    Sleep    5s
    Ctn Stop Engine
    Sleep    5s
    Ctn Clear Cache
    Ctn Start Engine
    Sleep    25s
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    ${result}    Ctn Check Multiplicity When Engine Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERDUCU2
    [Documentation]    Starting/stopping Engine does not create duplicated events in usual cases with unified_sql
    [Tags]    broker    engine    start-stop    duplicate    retention
    Ctn Clear Retention
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Ctn Broker Config Log    central    lua    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker    module
    Ctn Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Ctn Broker Config Log    module0    lua    debug
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    module0    0
    Ctn Config Broker    rrd
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected.
    Sleep    5s
    Ctn Stop Engine
    Sleep    5s
    Ctn Clear Cache
    Ctn Start Engine
    Sleep    25s
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    ${result}    Ctn Check Multiplicity When Engine Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERDUC3U1
    [Documentation]    Starting/stopping Broker does not create duplicated events in usual cases with unified_sql and BBDO 3.0
    [Tags]    broker    engine    start-stop    duplicate    retention
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Config Broker    central
    Ctn Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Ctn Broker Config Log    central    lua    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker    module
    Ctn Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    module0    0
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected.
    Sleep    5s
    Ctn Kindly Stop Broker
    Sleep    5s
    Ctn Clear Cache
    Ctn Start Broker
    Sleep    25s
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    ${result}    Ctn Check Multiplicity When Broker Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERDUC3U2
    [Documentation]    Starting/stopping Engine does not create duplicated events in usual cases with unified_sql and BBDO 3.0
    [Tags]    broker    engine    start-stop    duplicate    retention
    Ctn Clear Retention
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Ctn Broker Config Log    central    lua    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Config Broker    module
    Ctn Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Ctn Broker Config Log    module0    lua    debug
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    module0    0
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Let's wait for the lua to be correctly initialized
    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in cbd
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Lua not started in centengine

    # Let's wait for all the services configuration.
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60

    ${start}    Ctn Get Round Current Date
    # Let's wait for a first service status.
    ${content}    Create List    unified_sql: processing pb service status
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result[0]}    We did not get any pb service status for 60s

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected.
    Sleep    5s
    Ctn Stop Engine
    Sleep    5s
    Ctn Clear Cache
    Ctn Start Engine
    Sleep    25s
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    ${result}    Ctn Check Multiplicity When Engine Restarted    /tmp/lua-engine.log    /tmp/lua.log
    Should Be True    ${result}    There are events sent several times, see /tmp/lua-engine.log and /tmp/lua.log

BERDUCA300
    [Documentation]    Starting/stopping Engine is stopped ; it should emit a stop event and receive an ack event with events to clean from broker.
    [Tags]    broker    engine    start-stop    duplicate    retention    unified_sql
    Ctn Clear Retention
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    trace
    Ctn Broker Config Log    central    tcp    trace
    Ctn Config Broker    module
    Ctn Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Ctn Broker Config Log    module0    config    debug
    Ctn Broker Config Log    module0    bbdo    trace
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    module0    0
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.1
    Ctn Broker Config Add Item    central    bbdo_version    3.0.1
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.1
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected.

    Ctn Wait For Engine To Be Ready    ${1}

    Ctn Stop Engine
    ${content}    Create List    BBDO: sending stop packet to peer
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Engine should send a pb stop message to cbd.

    ${content}    Create List    BBDO: received stop from peer
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker should receive a pb stop message from engine.

    ${content}    Create List    send acknowledgement for [0-9]+ events
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result[0]}    Broker should send an ack for handled events.

    ${content}    Create List    BBDO: received acknowledgement for [0-9]+ events before finishing
    ${result}    Ctn Find Regex In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result[0]}    Engine should receive an ack for handled events from broker.

    Ctn Kindly Stop Broker

BERDUCA301
    [Documentation]    Starting/stopping Engine is stopped ; it should emit a stop event and receive an ack event with events to clean from broker with bbdo 3.0.1.
    [Tags]    broker    engine    start-stop    duplicate    retention    unified_sql
    Ctn Clear Retention
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Config Broker    central
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Lua Output    central    test-doubles    ${SCRIPTS}test-doubles-c.lua
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    trace
    Ctn Broker Config Log    central    tcp    trace
    Ctn Config Broker    module
    Ctn Broker Config Add Lua Output    module0    test-doubles    ${SCRIPTS}test-doubles.lua
    Ctn Broker Config Log    module0    config    debug
    Ctn Broker Config Log    module0    bbdo    trace
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Flush Log    module0    0
    Ctn Config Broker    rrd

    Ctn Config BBDO3    1
    ${start}    Get Current Date

    Ctn Start Broker
    Ctn Start Engine

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected.
    Ctn Wait For Engine To Be Ready    ${1}

    Ctn Stop Engine
    ${content}    Create List    BBDO: sending stop packet to peer
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    Engine should send a pb stop message to cbd.

    ${content}    Create List    BBDO: received stop from peer
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker should receive a pb stop message from engine.

    ${content}    Create List    send acknowledgement for [0-9]+ events
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result[0]}    Broker should send an ack for handled events.

    ${content}    Create List    BBDO: received acknowledgement for [0-9]+ events before finishing
    ${result}    Ctn Find Regex In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result[0]}    Engine should receive an ack for handled events from broker.

    Ctn Kindly Stop Broker

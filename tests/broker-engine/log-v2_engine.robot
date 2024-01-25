*** Settings ***
Documentation       Centreon Broker and Engine log_v2

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Test Cases ***
LOGV2EB1
    [Documentation]    Checking broker sink when log-v2 is enabled and legacy logs are disabled.
    [Tags]    broker    engine    log-v2 sink broker
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module
    Broker Config Flush Log    module0    0
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Engine Config Set Value    ${0}    log_level_config    trace
    Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date    exclude_millis=yes
    ${time_stamp}    Convert Date    ${start}    epoch    exclude_millis=yes
    ${time_stamp2}    Evaluate    int(${time_stamp})
    Sleep    1s

    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${pid}    Get Process Id    e0
    ${content}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.

    ${result1}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result1}    No message telling configuration loaded.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Log To Console    after connection
    FOR    ${index}    IN RANGE    60
        Log To Console
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        ${output}    Query
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((1,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((1,),)
    Stop Engine
    Kindly Stop Broker

LOGV2EBU1
    [Documentation]    Checking broker sink when log-v2 is enabled and legacy logs are disabled with bbdo3.
    [Tags]    broker    engine    log-v2 sink broker    bbdo3    unified_sql
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module
    Config BBDO3    ${1}
    Broker Config Flush Log    module0    0
    Broker Config Flush Log    central    0
    Broker Config Log    central    sql    trace
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Engine Config Set Value    ${0}    log_level_config    trace
    Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date    exclude_millis=yes
    ${time_stamp}    Convert Date    ${start}    epoch    exclude_millis=yes
    ${time_stamp2}    Evaluate    int(${time_stamp})
    Sleep    1s

    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${pid}    Get Process Id    e0
    ${content}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.

    ${result1}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result1}    No message telling configuration loaded.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Log To Console    after connection
    FOR    ${index}    IN RANGE    60
        Log To Console
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        ${output}    Query
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((1,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((1,),)
    Stop Engine
    Kindly Stop Broker

LOGV2DB1
    [Documentation]    log-v2 disabled old log enabled check broker sink
    [Tags]    broker    engine    log-v2 sink broker
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module
    Broker Config Flush Log    module0    0
    Broker Config Log    central    sql    trace
    Engine Config Set Value    ${0}    log_legacy_enabled    ${1}
    Engine Config Set Value    ${0}    log_v2_enabled    ${0}
    Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date    exclude_millis=yes
    ${time_stamp}    Convert Date    ${start}    epoch    exclude_millis=yes
    ${time_stamp2}    Evaluate    int(${time_stamp})
    Sleep    1s

    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.
    ${content_old}    Create List    [${pid}] Configuration loaded, main loop starting.

    ${result1}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    15
    ${result2}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content_old}    15
    Should Not Be True    ${result1}
    Should Be True    ${result2}    Old logs should be enabled.

    Log To Console    after connection
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        Log To Console
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        ${output}    Query
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2};
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((1,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((1,),)
    Stop Engine
    Kindly Stop Broker

LOGV2DB2
    [Documentation]    log-v2 disabled old log disabled check broker sink
    [Tags]    broker    engine    log-v2 sink broker
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module
    Broker Config Flush Log    module0    0
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${0}
    Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date    exclude_millis=yes
    ${time_stamp}    Convert Date    ${start}    epoch    exclude_millis=yes
    ${time_stamp2}    Evaluate    int(${time_stamp})
    Sleep    1s
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.
    ${content_hold}    Create List    [${pid}] Configuration loaded, main loop starting.

    ${result1}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    30
    ${result2}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content_hold}    30
    Should Not Be True    ${result1}
    Should Not Be True    ${result2}

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Log To Console    after connection
    FOR    ${index}    IN RANGE    60
        Log To Console
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        ${output}    Query
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2};
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((0,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((0,),)
    Stop Engine
    Kindly Stop Broker

LOGV2EB2
    [Documentation]    log-v2 enabled old log enabled check broker sink
    [Tags]    broker    engine    log-v2    sinkbroker
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module
    Broker Config Flush Log    module0    0
    Engine Config Set Value    ${0}    log_legacy_enabled    ${1}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date    exclude_millis=yes
    ${time_stamp}    Convert Date    ${start}    epoch    exclude_millis=yes
    ${time_stamp2}    Evaluate    int(${time_stamp})
    Sleep    1s

    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.
    ${content_hold}    Create List    [${pid}] Configuration loaded, main loop starting.

    ${result1}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    30
    ${result2}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content_hold}    30
    Should Be True    ${result1}
    Should Be True    ${result2}

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Log To Console    after connection
    FOR    ${index}    IN RANGE    60
        Log To Console
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        ${output}    Query
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2};
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((2,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((2,),)

    Stop Engine
    Kindly Stop Broker

LOGV2EBU2
    [Documentation]    Check Broker sink with log-v2 enabled and legacy log enabled with BBDO3.
    [Tags]    broker    engine    log-v2    sinkbroker    unified_sql    bbdo3
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module
    Broker Config Flush Log    module0    0
    Config BBDO3    ${1}
    Engine Config Set Value    ${0}    log_legacy_enabled    ${1}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date    exclude_millis=yes
    ${time_stamp}    Convert Date    ${start}    epoch    exclude_millis=yes
    ${time_stamp2}    Evaluate    int(${time_stamp})
    Sleep    1s

    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.
    ${content_hold}    Create List    [${pid}] Configuration loaded, main loop starting.

    ${result1}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    30
    ${result2}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content_hold}    30
    Should Be True    ${result1}
    Should Be True    ${result2}

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Log To Console    after connection
    FOR    ${index}    IN RANGE    60
        Log To Console
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        ${output}    Query
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2};
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((2,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((2,),)

    Stop Engine
    Kindly Stop Broker

LOGV2EF1
    [Documentation]    log-v2 enabled    old log disabled check logfile sink
    [Tags]    broker    engine    log-v2
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module
    Broker Config Flush Log    module0    0
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.

    ${result1}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    30
    Should Be True    ${result1}
    Stop Engine
    Kindly Stop Broker

LOGV2DF1
    [Documentation]    log-v2 disabled old log enabled check logfile sink
    [Tags]    broker    engine    log-v2
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module
    Broker Config Flush Log    module0    0
    Engine Config Set Value    ${0}    log_legacy_enabled    ${1}
    Engine Config Set Value    ${0}    log_v2_enabled    ${0}
    Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    ${pid}    Get Process Id    e0
    ${content_hold}    Create List    [${pid}] Configuration loaded, main loop starting.
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.

    ${result1}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content_hold}    30
    ${result2}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    30
    Should Be True    ${result1}
    Should Not Be True    ${result2}
    Stop Engine
    Kindly Stop Broker

LOGV2DF2
    [Documentation]    log-v2 disabled old log disabled check logfile sink
    [Tags]    broker    engine    log-v2
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module
    Broker Config Flush Log    module0    0
    Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Engine Config Set Value    ${0}    log_v2_enabled    ${0}
    Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.
    ${content_hold}    Create List    [${pid}] Configuration loaded, main loop starting.

    ${result1}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    15
    ${result2}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content_hold}    15
    Should Not Be True    ${result1}
    Should Not Be True    ${result2}
    Stop Engine
    Kindly Stop Broker

LOGV2EF2
    [Documentation]    log-v2 enabled old log enabled check logfile sink
    [Tags]    broker    engine    log-v2
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module
    Broker Config Flush Log    module0    0
    Engine Config Set Value    ${0}    log_legacy_enabled    ${1}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.
    ${content_hold}    Create List    [${pid}] Configuration loaded, main loop starting.

    ${result1}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    15
    ${result2}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content_hold}    15
    Should Be True    ${result1}
    Should Be True    ${result2}
    Stop Engine
    Kindly Stop Broker

LOGV2FE2
    [Documentation]    log-v2 enabled old log enabled check logfile sink
    [Tags]    broker    engine    log-v2
    Config Engine    ${1}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module
    Broker Config Flush Log    module0    0
    Engine Config Set Value    ${0}    log_legacy_enabled    ${1}
    Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Engine Config Set Value    ${0}    log_flush_period    0    True

    Clear Engine Logs

    ${start}    Get Current Date
    Start Broker
    Start Engine
    ${result}    Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.
    ${content_hold}    Create List    [${pid}] Configuration loaded, main loop starting.

    Sleep    2m

    ${res}    Check Engine Logs Are Duplicated    ${engineLog0}    ${start}
    Should Be True    ${res}    one or other log are not duplicate in logsfile
    Stop Engine
    Kindly Stop Broker

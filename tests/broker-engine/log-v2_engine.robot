*** Settings ***
Documentation       Centreon Broker and Engine log_v2

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
LOGV2EB1
    [Documentation]    Checking broker sink when log-v2 is enabled and legacy logs are disabled.
    [Tags]    broker    engine    log-v2 sink broker
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Flush Log    module0    0
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_config    trace
    Ctn Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date    exclude_millis=yes
    ${time_stamp}    Convert Date    ${start}    epoch    exclude_millis=yes
    ${time_stamp2}    Evaluate    int(${time_stamp})
    Sleep    1s

    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${pid}    Get Process Id    e0
    ${content}    Create List    [process] [info] [:] [${pid}] Configuration loaded, main loop starting.

    ${result1}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result1}    No message telling configuration loaded.

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Log To Console
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        ${output}    Query
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((1,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((1,),)
    Ctn Stop engine
    Ctn Kindly Stop Broker

LOGV2EBU1
    [Documentation]    Checking broker sink when log-v2 is enabled and legacy logs are disabled with bbdo3.
    [Tags]    broker    engine    log-v2 sink broker    bbdo3    unified_sql
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Flush Log    module0    0
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Log    central    sql    trace
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_config    trace
    Ctn Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date    exclude_millis=yes
    ${time_stamp}    Convert Date    ${start}    epoch    exclude_millis=yes
    ${time_stamp2}    Evaluate    int(${time_stamp})
    Sleep    1s

    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${pid}    Get Process Id    e0
    ${content}    Create List    [process] [info] [:] [${pid}] Configuration loaded, main loop starting.

    ${result1}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result1}    No message telling configuration loaded.

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Log To Console
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        ${output}    Query
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((1,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((1,),)
    Ctn Stop engine
    Ctn Kindly Stop Broker

LOGV2DB1
    [Documentation]    log-v2 disabled old log enabled check broker sink
    [Tags]    broker    engine    log-v2 sink broker
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Flush Log    module0    0
    Ctn Broker Config Log    central    sql    trace
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date    exclude_millis=yes
    ${time_stamp}    Convert Date    ${start}    epoch    exclude_millis=yes
    ${time_stamp2}    Evaluate    int(${time_stamp})
    Sleep    1s

    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.
    ${content_old}    Create List    [${pid}] Configuration loaded, main loop starting.

    ${result1}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    15
    ${result2}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_old}    15
    Should Not Be True    ${result1}
    Should Be True    ${result2}    Old logs should be enabled.

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Log To Console
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        ${output}    Query
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2};
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((1,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((1,),)
    Ctn Stop engine
    Ctn Kindly Stop Broker

LOGV2DB2
    [Documentation]    log-v2 disabled old log disabled check broker sink
    [Tags]    broker    engine    log-v2 sink broker
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Flush Log    module0    0
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date    exclude_millis=yes
    ${time_stamp}    Convert Date    ${start}    epoch    exclude_millis=yes
    ${time_stamp2}    Evaluate    int(${time_stamp})
    Sleep    1s
    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.
    ${content_old}    Create List    [${pid}] Configuration loaded, main loop starting.

    ${result1}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    30
    ${result2}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_old}    30
    Should Not Be True    ${result1}
    Should Not Be True    ${result2}

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Log To Console
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        ${output}    Query
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2};
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((0,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((0,),)
    Ctn Stop engine
    Ctn Kindly Stop Broker

LOGV2EB2
    [Documentation]    log-v2 enabled old log enabled check broker sink
    [Tags]    broker    engine    log-v2    sinkbroker
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Flush Log    module0    0
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date    exclude_millis=yes
    ${time_stamp}    Convert Date    ${start}    epoch    exclude_millis=yes
    ${time_stamp2}    Evaluate    int(${time_stamp})
    Sleep    1s

    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [:] [${pid}] Configuration loaded, main loop starting.
    ${content_old}    Create List    [${pid}] Configuration loaded, main loop starting.

    ${result1}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    30
    ${result2}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_old}    30
    Should Be True    ${result1}
    Should Be True    ${result2}

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Log To Console
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        ${output}    Query
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2};
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((2,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((2,),)

    Ctn Stop engine
    Ctn Kindly Stop Broker

LOGV2EBU2
    [Documentation]    Check Broker sink with log-v2 enabled and legacy log enabled with BBDO3.
    [Tags]    broker    engine    log-v2    sinkbroker    unified_sql    bbdo3
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Flush Log    module0    0
    Ctn Config BBDO3    ${1}
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date    exclude_millis=yes
    ${time_stamp}    Convert Date    ${start}    epoch    exclude_millis=yes
    ${time_stamp2}    Evaluate    int(${time_stamp})
    Sleep    1s

    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [:] [${pid}] Configuration loaded, main loop starting.
    ${content_old}    Create List    [${pid}] Configuration loaded, main loop starting.

    ${result1}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    30
    ${result2}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_old}    30
    Should Be True    ${result1}
    Should Be True    ${result2}

    FOR    ${index}    IN RANGE    60
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Log To Console
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2}
        ${output}    Query
        ...    SELECT COUNT(*) FROM logs WHERE output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2};
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((2,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((2,),)

    Ctn Stop engine
    Ctn Kindly Stop Broker

LOGV2EF1
    [Documentation]    log-v2 enabled    old log disabled check logfile sink
    [Tags]    broker    engine    log-v2
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Flush Log    module0    0
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [:] [${pid}] Configuration loaded, main loop starting.

    ${result1}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    30
    Should Be True    ${result1}
    Ctn Stop engine
    Ctn Kindly Stop Broker

LOGV2DF1
    [Documentation]    log-v2 disabled old log enabled check logfile sink
    [Tags]    broker    engine    log-v2
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Flush Log    module0    0
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    ${pid}    Get Process Id    e0
    ${content_old}    Create List    [${pid}] Configuration loaded, main loop starting.
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.

    ${result1}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_old}    30
    ${result2}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    30
    Should Be True    ${result1}
    Should Not Be True    ${result2}
    Ctn Stop engine
    Ctn Kindly Stop Broker

LOGV2DF2
    [Documentation]    log-v2 disabled old log disabled check logfile sink
    [Tags]    broker    engine    log-v2
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Flush Log    module0    0
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${0}
    Ctn Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.
    ${content_old}    Create List    [${pid}] Configuration loaded, main loop starting.

    ${result1}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    15
    ${result2}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_old}    15
    Should Not Be True    ${result1}
    Should Not Be True    ${result2}
    Ctn Stop engine
    Ctn Kindly Stop Broker

LOGV2EF2
    [Documentation]    log-v2 enabled old log enabled check logfile sink
    [Tags]    broker    engine    log-v2
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Flush Log    module0    0
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_flush_period    0    True

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [:] [${pid}] Configuration loaded, main loop starting.
    ${content_old}    Create List    [${pid}] Configuration loaded, main loop starting.

    ${result1}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    15
    ${result2}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_old}    15
    Should Be True    ${result1}
    Should Be True    ${result2}
    Ctn Stop engine
    Ctn Kindly Stop Broker

LOGV2FE2
    [Documentation]    log-v2 enabled old log enabled check logfile sink
    [Tags]    broker    engine    log-v2
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Flush Log    module0    0
    Ctn Engine Config Set Value    ${0}    log_legacy_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_v2_enabled    ${1}
    Ctn Engine Config Set Value    ${0}    log_flush_period    0    True

    Ctn Clear Engine Logs

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.
    ${content_old}    Create List    [${pid}] Configuration loaded, main loop starting.

    Sleep    2m

    ${res}    Ctn Check Engine Logs Are Duplicated    ${engineLog0}    ${start}
    Should Be True    ${res}    one or other log are not duplicate in logsfile
    Ctn Stop engine
    Ctn Kindly Stop Broker

*** Settings ***
Documentation       Centreon Broker and Engine log_v2

Resource    ../resources/import.resource

Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Stop Processes
Test Teardown    Ctn Save Logs If Failed


*** Test Cases ***
LOGV2EB1
    [Documentation]    Checking broker sink when log-v2 is enabled and legacy logs are disabled.
    [Tags]    broker    engine    log-v2 sink broker
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Clear Db    logs

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
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${pid}    Get Process Id    e0
    ${content}    Create List    [process] [info] [:] [${pid}] Configuration loaded, main loop starting.

    ${result1}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
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
    Disconnect From Database
    Should Be Equal As Strings    ${output}    ((1,),)
    Ctn Stop Engine
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
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${pid}    Get Process Id    e0
    ${content}    Create List    [process] [info] [:] [${pid}] Configuration loaded, main loop starting.

    ${result1}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
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
    Disconnect From Database
    Should Be Equal As Strings    ${output}    ((1,),)
    Ctn Stop Engine
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
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected

    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.
    ${content_old}    Create List    [${pid}] Configuration loaded, main loop starting.

    ${result1}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    30
    ${result2}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_old}    30
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
    Disconnect From Database
    Should Be Equal As Strings    ${output}    ((0,),)
    Ctn Stop Engine
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
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [:] [${pid}] Configuration loaded, main loop starting.

    ${result1}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    30
    Should Be True    ${result1}
    Ctn Stop Engine
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
    Ctn Start Engine
    ${result}    Ctn Check Connections
    Should Be True    ${result}    Engine and Broker not connected
    ${pid}    Get Process Id    e0
    ${content_v2}    Create List    [process] [info] [${pid}] Configuration loaded, main loop starting.
    ${content_old}    Create List    [${pid}] Configuration loaded, main loop starting.

    ${result1}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_v2}    15
    ${result2}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content_old}    15
    Should Not Be True    ${result1}
    Should Not Be True    ${result2}
    Ctn Stop Engine
    Ctn Kindly Stop Broker

LOGV2CENTRALIZEDRELOAD
    [Documentation]    Given a Centreon platform with 1 poller in centralized configuration mode
    ...    And Engine already connected to Broker and logging into its initial log file
    ...    When a new configuration changing the log file path and log_file_line is pushed to Broker
    ...    And Broker forwards this new configuration to Engine
    ...    Then Engine must start writing its logs into the new log file.
    [Tags]    broker    engine    log-v2    centralized-configuration    MON-209200
    Ctn Config Centralized Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established

    ${content}    Create List    \\[functions\\] \\[trace\\] \\[host.cc:[0-9]+\\]
    ${result}    Ctn Find Regex In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result[0]}    Engine did not start logging into its initial log file

    # Build a new configuration changing the log file path and log_file_line, while Engine
    # is already running with the previous one.
    ${newEngineLog}    Set Variable    ${ENGINE_LOG}/config0/centengine_new.log
    Ctn Engine Config Set Value    ${0}    log_file    ${newEngineLog}    True
    Ctn Engine Config Set Value    ${0}    log_file_line    0    True

    # Push this new configuration to Broker, which must forward it to the already connected Engine.
    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker did not acknowledge the new configuration for poller 1

    # Engine must now be writing its logs into the new log file.
    ${content}    Create List    [functions] [trace]
    ${result}    Ctn Find In Log With Timeout    ${newEngineLog}    ${start}    ${content}    30
    Should Be True    ${result}
    ...    Engine did not start writing into the new log file after the configuration reload

    Ctn Stop Engine
    Ctn Kindly Stop Broker



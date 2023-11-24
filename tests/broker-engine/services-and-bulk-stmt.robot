*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/resources.robot
Library             Process
Library             OperatingSystem
Library             DateTime
Library             Collections
Library             DatabaseLibrary
Library             String
Library             Examples
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Test Clean


*** Test Cases ***
EBBPS1
    [Documentation]    1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table resources.
    [Tags]    broker    engine    services    unified_sql
    Config Engine    ${1}    ${1}    ${1000}
    # We want all the services to be passive to avoid parasite checks during our test.
    Set Services Passive    ${0}    service_.*
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    core    info
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    trace
    Broker Config Log    central    perfdata    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    ${start}    Get Current Date
    ${start_broker}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True
    ...    ${result}
    ...    A message telling check_for_external_commands() is ready should be available.
    FOR    ${i}    IN RANGE    ${1000}
        Process Service Check Result    host_1    service_${i+1}    1    warning${i}
    END
    ${content}    Create List
    ...    connected to 'MariaDB' Server
    ...    it supports column-wise binding in prepared statements
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Prepared statements should be supported with this version of MariaDB.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${date}    Get Round Current Date
    Log To Console    date=${date}
    FOR    ${index}    IN RANGE    60
        ${output}    Query
        ...    SELECT count(*) FROM resources WHERE name like 'service\_%%' and parent_name='host_1' and status <> 1
        Sleep    1s
        IF    "${output}" == "((0,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((0,),)

    FOR    ${i}    IN RANGE    ${1000}
        Process Service Check Result    host_1    service_${i+1}    2    warning${i}
        IF    ${i} % 200 == 0
            ${first_service_status_content}    Create List    unified_sql: processing pb service status
            ${result}    Find In Log With Timeout
            ...    ${centralLog}
            ...    ${start_broker}
            ...    ${first_service_status_content}
            ...    30
            Should Be True    ${result}    No service_status processing found.
            Log To Console    Stopping Broker
            Kindly Stop Broker
            Log To Console    Waiting for 5s
            Sleep    5s
            Log To Console    Restarting Broker
            ${start_broker}    Get Current Date
            Start Broker
        END
    END
    ${content}    Create List
    ...    connected to 'MariaDB' Server
    ...    it supports column-wise binding in prepared statements
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Prepared statements should be supported with this version of MariaDB.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${date}    Get Current Date    result_format=epoch
    Log To Console    date=${date}
    FOR    ${index}    IN RANGE    120
        ${output}    Query
        ...    SELECT count(*) FROM resources WHERE name like 'service\_%%' and parent_name='host_1' and status <> 2
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((0,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((0,),)

EBBPS2
    [Documentation]    1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table services.
    [Tags]    broker    engine    services    unified_sql
    Config Engine    ${1}    ${1}    ${1000}
    # We want all the services to be passive to avoid parasite checks during our test.
    Set Services Passive    ${0}    service_.*
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    core    info
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    trace
    Broker Config Log    central    perfdata    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    ${start}    Get Current Date
    ${start_broker}    Get Current Date
    Start Broker
    Start Engine
    ${content}    Create List    INITIAL SERVICE STATE: host_1;service_1000;
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True
    ...    ${result}
    ...    An Initial service state on host_1:service_1000 should be raised before we can start external commands.
    FOR    ${i}    IN RANGE    ${1000}
        Process Service Check Result    host_1    service_${i+1}    1    warning${i}
    END
    ${content}    Create List
    ...    connected to 'MariaDB' Server
    ...    it supports column-wise binding in prepared statements
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Prepared statements should be supported with this version of MariaDB.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${date}    Get Round Current Date
    Log To Console    date=${date}
    FOR    ${index}    IN RANGE    120
        ${output}    Query
        ...    SELECT count(*) FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description LIKE 'service\_%%' AND s.state <> 1
        Sleep    1s
        IF    "${output}" == "((0,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((0,),)

    FOR    ${i}    IN RANGE    ${1000}
        Process Service Check Result    host_1    service_${i+1}    2    critical${i}
        IF    ${i} % 200 == 0
            ${first_service_status_content}    Create List    unified_sql: processing pb service status
            ${result}    Find In Log With Timeout
            ...    ${centralLog}
            ...    ${start_broker}
            ...    ${first_service_status_content}
            ...    30
            Should Be True    ${result}    No service_status processing found.
            Kindly Stop Broker
            Log To Console    Waiting for 5s
            Sleep    5s
            Log To Console    Restarting Broker
            ${start_broker}    Get Current Date
            Start Broker
        END
    END
    ${content}    Create List
    ...    connected to 'MariaDB' Server
    ...    it supports column-wise binding in prepared statements
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Prepared statements should be supported with this version of MariaDB.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${date}    Get Current Date    result_format=epoch
    Log To Console    date=${date}
    FOR    ${index}    IN RANGE    60
        ${output}    Query
        ...    SELECT count(*) FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description LIKE 'service\_%%' AND s.state <> 2
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((0,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((0,),)

EBMSSM
    [Documentation]    1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. GetSqlManagerStats is called to measure writes into data_bin.
    [Tags]    broker    engine    services    unified_sql    benchmark
    Clear Metrics
    Config Engine    ${1}    ${1}    ${1000}
    # We want all the services to be passive to avoid parasite checks during our test.
    Set Services Passive    ${0}    service_.*
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    ${start}    Get Current Date
    Start Broker
    Start Engine
    Broker Set Sql Manager Stats    51001    5    5

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${start}    Get Round Current Date
    Log To Console    STEP1
    # Let's wait for one "INSERT INTO data_bin" to appear in stats.
    Log To Console    STEP2
    FOR    ${i}    IN RANGE    ${1000}
    Log To Console    STEP3
        Process Service Check Result With Metrics    host_1    service_${i+1}    1    warning${i}    ${100}
    Log To Console    STEP4
    END

    Log To Console    STEP5
    ${duration}    Broker Get Sql Manager Stats    51001    INSERT INTO data_bin    300
    Should Be True    ${duration} > 0

    # Let's wait for all force checks to be in the storage database.
    Log To Console    STEP6
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Log To Console    STEP7
    FOR    ${i}    IN RANGE    ${500}
    Log To Console    STEP8
        ${output}    Query
        ...    SELECT COUNT(s.last_check) FROM metrics m LEFT JOIN index_data i ON m.index_id = i.id LEFT JOIN services s ON s.host_id = i.host_id AND s.service_id = i.service_id WHERE metric_name LIKE "metric_%%" AND s.last_check >= ${start}
        IF    ${output[0][0]} >= 100000    BREAK
        Sleep    1s
    Log To Console    STEP10
    END
    Should Be True    ${output[0][0]} >= 100000
    Log To Console    STEP11

EBPS2
    [Documentation]    1000 services are configured with 20 metrics each. The rrd output is removed from the broker configuration to avoid to write too many rrd files. While metrics are written in bulk, the database is stopped. This must not crash broker.
    [Tags]    broker    engine    services    unified_sql    benchmark
    Clear Metrics
    Config Engine    ${1}    ${1}    ${1000}
    # We want all the services to be passive to avoid parasite checks during our test.
    Set Services Passive    ${0}    service_.*
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    trace
    Broker Config Log    central    perfdata    debug
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention

    ${start}    Get Current Date
    Start Broker
    Start Engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # Let's wait for one "INSERT INTO data_bin" to appear in stats.
    FOR    ${i}    IN RANGE    ${1000}
        Process Service Check Result With Metrics    host_1    service_${i+1}    1    warning${i}    ${20}
    END
    ${start}    Get Current Date
    ${content}    Create List    Check if some statements are ready,    sscr_bind connections
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that statements are available should be displayed
    Stop mysql
    Stop Engine
    Start mysql

RLCode
    [Documentation]    Test if reloading LUA code in a stream connector applies the changes
    [Tags]    lua    stream connector
    Clear Commands Status
    Clear Retention

    Remove File    /tmp/toto.lua
    Config Engine    ${1}    ${1}    ${10}
    Config Broker    central
    Config Broker    module
    Config Broker    rrd
    Config BBDO3    1
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    error
    Broker Config Log    central    lua    debug
    Config Broker Sql Output    central    unified_sql

    ${INITIAL_SCRIPT_CONTENT}    Catenate
    ...    function init(params)
    ...    broker_log:set_parameters(2, '/tmp/toto.log')
    ...    end
    ...
    ...    function write(d)
    ...    broker_log:info(0, "toto")
    ...    return true
    ...    end

    # Create the initial LUA script file
    Create File    /tmp/toto.lua    ${INITIAL_SCRIPT_CONTENT}

    Broker Config Add Lua Output    central    test-toto    /tmp/toto.lua

    # Start the engine/broker
    ${start}    Get Current Date

    Start Broker
    Start Engine

    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The lua virtual machine is not correctly initialized

    # Define the new content to take place of the first one
    ${new_content}    Catenate
    ...    function init(params)
    ...    broker_log:set_parameters(2, '/tmp/titi.log')
    ...    end
    ...
    ...    function write(d)
    ...    broker_log:info(0, "titi")
    ...    return true
    ...    end

    # Create the LUA script file from the content
    Create File    /tmp/toto.lua    ${new_content}
    ${start}    Get Current Date

    Reload Broker

    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The Lua virtual machine is not correctly initialized

    Stop Engine
    Kindly Stop Broker

metric_mapping
    [Documentation]    Check if metric name exists using a stream connector
    [Tags]    broker    engine    bbdo    unified_sql    metric
    Clear Commands Status
    Clear Retention

    Remove File    /tmp/test.log
    Config Engine    ${1}    ${1}    ${10}
    Config Broker    central
    Config Broker    module
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Log    central    lua    debug
    Broker Config Log    module0    neb    debug
    Config Broker Sql Output    central    unified_sql

    ${new_content}    Catenate
    ...    function init(params)
    ...    broker_log:set_parameters(1, "/tmp/test.log")
    ...    end
    ...
    ...    function write(d)
    ...    if d._type == 196617 then
    ...    broker_log:info(0, "name: " .. tostring(d.name) .. " corresponds to metric id " .. tostring(d.metric_id))
    ...    end
    ...    return true
    ...    end

    # Create the initial LUA script file
    Create File    /tmp/test-metric.lua    ${new_content}

    Broker Config Add Lua Output    central    test-metric    /tmp/test-metric.lua

    ${start}    Get Current Date

    Start Broker
    Start Engine

    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message about check_for_external_commands() should be available.

    # We force several checks with metrics
    FOR    ${i}    IN RANGE    ${10}
        Process Service Check Result With Metrics    host_1    service_${i+1}    1    warning${i}    ${20}
    END

    Wait Until Created    /tmp/test.log    30s
    ${grep_res}    Grep File    /tmp/test.log    name: metric1 corresponds to metric id
    Should Not Be Empty    ${grep_res}    metric name "metric1" not found

Services_and_bulks_${id}
    [Documentation]    One service is configured with one metric with a name of 150 to 1021 characters.
    [Tags]    broker    engine    services    unified_sql    benchmark
    Clear Metrics
    Config Engine    ${1}    ${1}    ${1}
    # We want all the services to be passive to avoid parasite checks during our test.
    ${random_string}    Generate Random String    ${metric_num_char}    [LOWER]
    Set Services passive    ${0}    service_.*
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Broker Config Source Log    central    1

    Config Broker Remove Rrd Output    central
    Clear Retention
    Clear Db    metrics

    ${start}    Get Current Date
    Start Broker
    Start Engine
    Broker Set Sql Manager Stats    51001    5    5

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${start_1}    Get Round Current Date

    Process Service Check result with metrics
    ...    host_1
    ...    service_${1}
    ...    ${1}
    ...    warning${0}
    ...    1
    ...    config0
    ...    ${random_string}

    ${content}    Create List    new perfdata inserted
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}    Find In Log With Timeout    ${log}    ${start_1}    ${content}    60
    Should Be True    ${result}    A message fail to handle a metric with ${metric_num_char} characters.

    ${metrics}    Get Metrics For Service    1    ${random_string}0
    Should Not Be Equal    ${metrics}    ${None}    no metric found for service

    Examples:    id    metric_num_char    --
    ...    1    1020
    ...    2    150


*** Keywords ***
Test Clean
    Stop Engine
    Kindly Stop Broker
    Save Logs If Failed

*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Test Clean


*** Test Cases ***
EBBPS1
    [Documentation]    1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table resources.
    [Tags]    broker    engine    services    unified_sql
    Ctn Config Engine    ${1}    ${1}    ${1000}
    # We want all the services to be passive to avoid parasite checks during our test.
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    tcp    error
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    perfdata    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    ${start}    Get Current Date
    ${start_broker}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    Ctn Wait For Engine To Be Ready    ${start}

    FOR    ${i}    IN RANGE    ${1000}
        Ctn Process Service Check Result    host_1    service_${i+1}    1    warning${i}
    END
    ${content}    Create List
    ...    connected to 'MariaDB' Server
    ...    it supports column-wise binding in prepared statements
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Prepared statements should be supported with this version of MariaDB.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${date}    Get Current Date    result_format=epoch
    Log To Console    date=${date}
    FOR    ${index}    IN RANGE    60
        ${output}    Query
        ...    SELECT count(*) FROM resources WHERE name like 'service\_%%' and parent_name='host_1' and status <> 1
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((0,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((0,),)

    FOR    ${i}    IN RANGE    ${1000}
        Ctn Process Service Check Result    host_1    service_${i+1}    2    warning${i}
        IF    ${i} % 200 == 0
            ${first_service_status_content}    Create List    unified_sql service_status processing
            ${result}    Ctn Find In Log With Timeout
            ...    ${centralLog}
            ...    ${start_broker}
            ...    ${first_service_status_content}
            ...    30
            Should Be True    ${result}    No service_status processing found.
            Log To Console    Stopping Broker
            Ctn Kindly Stop Broker
            Log To Console    Waiting for 5s
            Sleep    5s
            Log To Console    Restarting Broker
            ${start_broker}    Get Current Date
            Ctn Start Broker
        END
    END
    ${content}    Create List
    ...    connected to 'MariaDB' Server
    ...    it supports column-wise binding in prepared statements
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
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
    Ctn Config Engine    ${1}    ${1}    ${1000}
    # We want all the services to be passive to avoid parasite checks during our test.
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    central    tcp    error
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    perfdata    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention
    ${start}    Get Current Date
    ${start_broker}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    ${content}    Create List    INITIAL SERVICE STATE: host_1;service_1000;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True
    ...    ${result}
    ...    An Initial service state on host_1:service_1000 should be raised before we can start external commands.
    FOR    ${i}    IN RANGE    ${1000}
        Ctn Process Service Check Result    host_1    service_${i+1}    1    warning${i}
    END
    ${content}    Create List
    ...    connected to 'MariaDB' Server
    ...    it supports column-wise binding in prepared statements
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Prepared statements should be supported with this version of MariaDB.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${date}    Get Current Date    result_format=epoch
    Log To Console    date=${date}
    FOR    ${index}    IN RANGE    120
        ${output}    Query
        ...    SELECT count(*) FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description LIKE 'service\_%%' AND s.state <> 1
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((0,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((0,),)

    FOR    ${i}    IN RANGE    ${1000}
        Ctn Process Service Check Result    host_1    service_${i+1}    2    critical${i}
        IF    ${i} % 200 == 0
            ${first_service_status_content}    Create List    unified_sql service_status processing
            ${result}    Ctn Find In Log With Timeout
            ...    ${centralLog}
            ...    ${start_broker}
            ...    ${first_service_status_content}
            ...    30
            Should Be True    ${result}    No service_status processing found.
            Ctn Kindly Stop Broker
            Log To Console    Waiting for 5s
            Sleep    5s
            Log To Console    Restarting Broker
            ${start_broker}    Get Current Date
            Ctn Start Broker
        END
    END
    ${content}    Create List
    ...    connected to 'MariaDB' Server
    ...    it supports column-wise binding in prepared statements
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
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
    Ctn Clear Metrics
    Ctn Config Engine    ${1}    ${1}    ${1000}
    # We want all the services to be passive to avoid parasite checks during our test.
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    tcp    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker Remove Rrd Output    central
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    Ctn Broker Set Sql Manager Stats    51001    5    5

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${start}    Ctn Get Round Current Date
    # Let's wait for one "INSERT INTO data_bin" to appear in stats.
    FOR    ${i}    IN RANGE    ${1000}
        Ctn Process Service Check Result With Metrics    host_1    service_${i+1}    1    warning${i}    100
    END

    ${duration}    Ctn Broker Get Sql Manager Stats    51001    INSERT INTO data_bin    300
    Should Be True    ${duration} > 0

    # Let's wait for all force checks to be in the storage database.
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${i}    IN RANGE    ${500}
        ${output}    Query
        ...    SELECT COUNT(s.last_check) FROM metrics m LEFT JOIN index_data i ON m.index_id = i.id LEFT JOIN services s ON s.host_id = i.host_id AND s.service_id = i.service_id WHERE metric_name LIKE "metric_%%" AND s.last_check >= ${start}
        IF    ${output[0][0]} >= 100000    BREAK
        Sleep    1s
    END
    Should Be True    ${output[0][0]} >= 100000

EBPS2
    [Documentation]    1000 services are configured with 20 metrics each. The rrd output is removed from the broker configuration to avoid to write too many rrd files. While metrics are written in bulk, the database is stopped. This must not crash broker.
    [Tags]    broker    engine    services    unified_sql    benchmark
    Ctn Clear Metrics
    Ctn Config Engine    ${1}    ${1}    ${1000}
    # We want all the services to be passive to avoid parasite checks during our test.
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    tcp    error
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    perfdata    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker Remove Rrd Output    central
    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # Let's wait for one "INSERT INTO data_bin" to appear in stats.
    FOR    ${i}    IN RANGE    ${1000}
        Ctn Process Service Check Result With Metrics    host_1    service_${i+1}    1    warning${i}    20
    END
    ${start}    Get Current Date
    ${content}    Create List    Check if some statements are ready,    sscr_bind connections
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that statements are available should be displayed
    Ctn Stop Mysql
    Ctn Stop engine
    Ctn Start Mysql

RLCode
    [Documentation]    Test if reloading LUA code in a stream connector applies the changes
    [Tags]    lua    stream connector
    Ctn Clear Commands Status
    Ctn Clear Retention

    Remove File    /tmp/toto.lua
    Ctn Config Engine    ${1}    ${1}    ${10}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    tcp    error
    Ctn Broker Config Log    central    sql    error
    Ctn Broker Config Log    central    lua    debug
    Ctn Config Broker Sql Output    central    unified_sql

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

    Ctn Broker Config Add Lua Output    central    test-toto    /tmp/toto.lua

    # Start the engine/broker
    ${start}    Get Current Date

    Ctn Start Broker
    Ctn Start engine

    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
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

    Ctn Reload Broker

    ${content}    Create List    lua: initializing the Lua virtual machine
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The Lua virtual machine is not correctly initialized

    Ctn Stop engine
    Ctn Kindly Stop Broker

metric_mapping
    [Documentation]    Check if metric name exists using a stream connector
    [Tags]    broker    engine    bbdo    unified_sql    metric
    Ctn Clear Commands Status
    Ctn Clear Retention

    Remove File    /tmp/test.log
    Ctn Config Engine    ${1}    ${1}    ${10}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Add Item    central    bbdo_version    3.0.1
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.1
    Ctn Broker Config Log    central    lua    debug
    Ctn Broker Config Log    module0    neb    debug
    Ctn Config Broker Sql Output    central    unified_sql

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

    Ctn Broker Config Add Lua Output    central    test-metric    /tmp/test-metric.lua

    ${start}    Get Current Date

    Ctn Start Broker
    Ctn Start engine

    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message about check_for_external_commands() should be available.

    # We force several checks with metrics
    FOR    ${i}    IN RANGE    ${10}
        Ctn Process Service Check Result With Metrics    host_1    service_${i+1}    1    warning${i}    20
    END

    Wait Until Created    /tmp/test.log    30s
    ${grep_res}    Grep File    /tmp/test.log    name: metric1 corresponds to metric id
    Should Not Be Empty    ${grep_res}    metric name "metric1" not found

Services_and_bulks_${id}
    [Documentation]    One service is configured with one metric with a name of 150 to 1021 characters.
    [Tags]    broker    engine    services    unified_sql    benchmark
    Ctn Clear Metrics
    Ctn Config Engine    ${1}    ${1}    ${1}
    # We want all the services to be passive to avoid parasite checks during our test.
    ${random_string}    Generate Random String    ${metric_num_char}    [LOWER]
    Ctn Set Services Passive    ${0}    service_.*
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.1
    Ctn Broker Config Add Item    central    bbdo_version    3.0.1
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    tcp    error
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Source Log    central    1

    Ctn Config Broker Remove Rrd Output    central
    Ctn Clear Retention
    Ctn Clear Db    metrics

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start engine
    Ctn Broker Set Sql Manager Stats    51001    5    5

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${start_1}    Ctn Get Round Current Date

    Ctn Process Service Check Result With Metrics
    ...    host_1
    ...    service_${1}
    ...    ${1}
    ...    warning${0}
    ...    1
    ...    config0
    ...    ${random_string}

    ${content}    Create List    new perfdata inserted
    ${log}    Catenate    SEPARATOR=    ${BROKER_LOG}    /central-broker-master.log
    ${result}    Ctn Find In Log With Timeout    ${log}    ${start_1}    ${content}    60
    Should Be True    ${result}    A message fail to handle a metric with ${metric_num_char} characters.

    ${metrics}    Ctn Get Metrics For Service    1    ${random_string}0
    Should Not Be Equal    ${metrics}    ${None}    no metric found for service

    Examples:    id    metric_num_char    --
    ...    1    1020
    ...    2    150


*** Keywords ***
Ctn Test Clean
    Ctn Stop engine
    Ctn Kindly Stop Broker
    Ctn Save Logs If Failed

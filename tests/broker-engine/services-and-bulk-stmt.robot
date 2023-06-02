*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/resources.robot
Library             Process
Library             OperatingSystem
Library             DateTime
Library             Collections
Library             DatabaseLibrary
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save logs If Failed


*** Test Cases ***
EBBPS1
    [Documentation]    1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table resources.
    [Tags]    broker    engine    services    unified_sql
    Config Engine    ${1}    ${1}    ${1000}
    # We want all the services to be passive to avoid parasite checks during our test.
    Set Services passive    ${0}    service_.*
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${content}=    Create List    INITIAL SERVICE STATE: host_1;service_1000;
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True
    ...    ${result}
    ...    msg=An Initial service state on host_1:service_1000 should be raised before we can start external commands.
    FOR    ${i}    IN RANGE    ${1000}
        Process Service Check result    host_1    service_${i+1}    1    warning${i}
    END
    ${content}=    Create List
    ...    connected to 'MariaDB' Server
    ...    Unified sql stream supports column-wise binding in prepared statements
    ${result}=    Find In Log with timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=Prepared statements should be supported with this version of MariaDB.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${date}=    Get Current Date    result_format=epoch
    Log To Console    date=${date}
    FOR    ${index}    IN RANGE    60
        ${output}=    Query
        ...    SELECT count(*) FROM resources WHERE name like 'service\_%' and parent_name='host_1' and status <> 1
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((0,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((0,),)

    FOR    ${i}    IN RANGE    ${1000}
        Process Service Check result    host_1    service_${i+1}    2    warning${i}
        IF    ${i} % 200 == 0
            Log to Console    Stopping Broker
            Kindly Stop Broker
            Log to Console    Waiting for 5s
            Sleep    5s
            Log to Console    Restarting Broker
            Start Broker
        END
    END
    ${content}=    Create List
    ...    connected to 'MariaDB' Server
    ...    Unified sql stream supports column-wise binding in prepared statements
    ${result}=    Find In Log with timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=Prepared statements should be supported with this version of MariaDB.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${date}=    Get Current Date    result_format=epoch
    Log To Console    date=${date}
    FOR    ${index}    IN RANGE    120
        ${output}=    Query
        ...    SELECT count(*) FROM resources WHERE name like 'service\_%' and parent_name='host_1' and status <> 2
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((0,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((0,),)
    Stop Engine
    Kindly Stop Broker

EBBPS2
    [Documentation]    1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table services.
    [Tags]    broker    engine    services    unified_sql
    Config Engine    ${1}    ${1}    ${1000}
    # We want all the services to be passive to avoid parasite checks during our test.
    Set Services passive    ${0}    service_.*
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    trace
    Config Broker Sql Output    central    unified_sql
    Clear Retention
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${content}=    Create List    INITIAL SERVICE STATE: host_1;service_1000;
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True
    ...    ${result}
    ...    msg=An Initial service state on host_1:service_1000 should be raised before we can start external commands.
    FOR    ${i}    IN RANGE    ${1000}
        Process Service Check result    host_1    service_${i+1}    1    warning${i}
    END
    ${content}=    Create List
    ...    connected to 'MariaDB' Server
    ...    Unified sql stream supports column-wise binding in prepared statements
    ${result}=    Find In Log with timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=Prepared statements should be supported with this version of MariaDB.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${date}=    Get Current Date    result_format=epoch
    Log To Console    date=${date}
    FOR    ${index}    IN RANGE    120
        ${output}=    Query
        ...    SELECT count(*) FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description LIKE 'service\_%' AND s.state <> 1
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((0,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((0,),)

    FOR    ${i}    IN RANGE    ${1000}
        Process Service Check result    host_1    service_${i+1}    2    warning${i}
        IF    ${i} % 200 == 0
            Log to Console    Stopping Broker
            Kindly Stop Broker
            Log to Console    Waiting for 5s
            Sleep    5s
            Log to Console    Restarting Broker
            Start Broker
        END
    END
    ${content}=    Create List
    ...    connected to 'MariaDB' Server
    ...    Unified sql stream supports column-wise binding in prepared statements
    ${result}=    Find In Log with timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=Prepared statements should be supported with this version of MariaDB.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${date}=    Get Current Date    result_format=epoch
    Log To Console    date=${date}
    FOR    ${index}    IN RANGE    60
        ${output}=    Query
        ...    SELECT count(*) FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description LIKE 'service\_%' AND s.state <> 2
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((0,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((0,),)
    Stop Engine
    Kindly Stop Broker

EBMSSM
    [Documentation]    1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. GetSqlManagerStats is called to measure writes into data_bin.
    [Tags]    broker    engine    services    unified_sql    benchmark
    Clear Metrics
    Config Engine    ${1}    ${1}    ${1000}
    # We want all the services to be passive to avoid parasite checks during our test.
    Set Services passive    ${0}    service_.*
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    ${start}=    Get Current Date
    Start Broker    ${True}
    Start Engine
    Broker Set Sql Manager Stats    51001    5    5

    # Let's wait for the external command check start
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.

    ${start}=    Get Round Current Date
    # Let's wait for one "INSERT INTO data_bin" to appear in stats.
    FOR    ${i}    IN RANGE    ${1000}
        Process Service Check result with metrics    host_1    service_${i+1}    1    warning${i}    100
    END

    ${duration}=    Broker Get Sql Manager Stats    51001    INSERT INTO data_bin    300
    Should Be True    ${duration} > 0

	# Let's wait for all force checks to be in the storage database.
	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	FOR	${i}	IN RANGE	${500}
	  ${output}=	Query	SELECT COUNT(s.last_check) FROM metrics m LEFT JOIN index_data i ON m.index_id = i.id LEFT JOIN services s ON s.host_id = i.host_id AND s.service_id = i.service_id WHERE metric_name LIKE "metric_%" AND s.last_check >= ${start}
	  Exit For Loop If	${output[0][0]} >= 100000
          Sleep	1s
	END
	Should Be True	${output[0][0]} >= 100000
	Stop Engine
	Kindly Stop Broker	True

EBPS2
    [Documentation]    1000 services are configured with 20 metrics each. The rrd output is removed from
    [Tags]    broker    engine    services    unified_sql    benchmark
    Clear Metrics
    Config Engine    ${1}    ${1}    ${1000}
    # We want all the services to be passive to avoid parasite checks during our test.
    Set Services passive    ${0}    service_.*
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Flush Log     central     0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    trace
    Broker Config Log    central    perfdata    debug
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention

    ${start}=    Get Current Date
    Start Broker    ${True}
    Start Engine
    # Let's wait for the external command check start
    ${content}= Create List     check_for_external_commands()
    ${result}=  Find In Log with Timeout        ${engineLog0}   ${start}        ${content}      60
    Should Be True      ${result}       msg=A message telling check_for_external_commands() should be available.

    # Let's wait for one "INSERT INTO data_bin" to appear in stats.
    FOR    ${i}    IN RANGE    ${1000}
        Process Service Check result with metrics    host_1    service_${i+1}    1    warning${i}    20
    END
    ${start}=    Get Current Date
    ${content}=     create list     Check if some statements are ready,  sscr_bind connections
    ${result}=  Find In Log with Timeout        ${centralLog}   ${start}        ${content}      60
    Should Be True  ${result}       msg=A message telling that statements are available should be displayed
    Stop mysql
    Stop Engine
    Start mysql
    Kindly Stop Broker   ${True}

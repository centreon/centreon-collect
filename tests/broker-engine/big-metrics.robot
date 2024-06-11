*** Settings ***
Documentation       There tests are about big metric values

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Test Clean


*** Test Cases ***
EBBM1
    [Documentation]    A service status contains metrics that do not fit in a float number.
    [Tags]    broker    engine    services    unified_sql
    Ctn Config Engine    ${1}    ${1}    ${1}
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
    Ctn Wait For Engine To Be Ready    ${1}

    FOR    ${i}    IN RANGE    ${10}
        Ctn Process Service Check Result With Big Metrics
	...    host_1    service_1    1
	...    Big Metrics    ${10}
    END
    ${content}    Create List
    ...    Out of range value for column 'current_value'
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    not ${result}    It shouldn't be forbidden to store big metrics in the database.

#    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
#    ${date}    Get Current Date    result_format=epoch
#    Log To Console    date=${date}
#    FOR    ${index}    IN RANGE    60
#        ${output}    Query
#        ...    SELECT count(*) FROM resources WHERE name like 'service\_%%' and parent_name='host_1' and status <> 1
#        Log To Console    ${output}
#        Sleep    1s
#        IF    "${output}" == "((0,),)"    BREAK
#    END
#    Should Be Equal As Strings    ${output}    ((0,),)
#
#    FOR    ${i}    IN RANGE    ${1000}
#        Ctn Process Service Check Result    host_1    service_${i+1}    2    warning${i}
#        IF    ${i} % 200 == 0
#            ${first_service_status_content}    Create List    unified_sql service_status processing
#            ${result}    Ctn Find In Log With Timeout
#            ...    ${centralLog}
#            ...    ${start_broker}
#            ...    ${first_service_status_content}
#            ...    30
#            Should Be True    ${result}    No service_status processing found.
#            Log To Console    Stopping Broker
#            Ctn Kindly Stop Broker
#            Log To Console    Waiting for 5s
#            Sleep    5s
#            Log To Console    Restarting Broker
#            ${start_broker}    Get Current Date
#            Ctn Start Broker
#        END
#    END
#    ${content}    Create List
#    ...    connected to 'MariaDB' Server
#    ...    it supports column-wise binding in prepared statements
#    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
#    Should Be True    ${result}    Prepared statements should be supported with this version of MariaDB.
#
#    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
#    ${date}    Get Current Date    result_format=epoch
#    Log To Console    date=${date}
#    FOR    ${index}    IN RANGE    120
#        ${output}    Query
#        ...    SELECT count(*) FROM resources WHERE name like 'service\_%%' and parent_name='host_1' and status <> 2
#        Log To Console    ${output}
#        Sleep    1s
#        IF    "${output}" == "((0,),)"    BREAK
#    END
#    Should Be Equal As Strings    ${output}    ((0,),)


*** Keywords ***
Ctn Test Clean
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Ctn Save Logs If Failed

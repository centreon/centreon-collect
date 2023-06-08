*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/resources.robot
Library             Process
Library             OperatingSystem
Library             DateTime
Library             Collections
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save logs If Failed


*** Test Cases ***
SDER
    [Documentation]
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${1}    ${25}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Add Item    module0    bbdo_version    3.0.0
    Broker Config Add Item    central    bbdo_version    3.0.0
    Broker Config Add Item    rrd    bbdo_version    3.0.0
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    ${start}=    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}=    Create List    check_for_external_commands()
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=A message telling check_for_external_commands() should be available.

    ${start}=    Get Round Current Date
    # Let's wait for one "INSERT INTO data_bin" to appear in stats.
    FOR    ${i}    IN RANGE    ${25}
        Process Service Check result    host_1    service_${i+1}    1    critical${i}
    END
    Stop Engine

    modify retention dat    0   host_1  service_1  current_attempt   280
    modify retention dat    0   host_1  service_1  max_attempts   280
    modify retention dat host       0   host_1  max_attempts    280
    Start Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

   FOR    ${index}    IN RANGE    3
        Log To Console    SELECT check_attempts from resources WHERE name='service_1'
        ${output}=    Query    SELECT check_attempts from resources WHERE name='service_1'
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((280,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((280,),)

        Sleep    1s

    FOR    ${index}    IN RANGE    10
        Log To Console    SELECT max_check_attempts  from resources WHERE name='service_1'
        ${output}=    Query    SELECT max_check_attempts from resources WHERE name='service_1'
        Log To Console    ${output}
        Sleep    1s
        IF    "${output}" == "((280,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((280,),)

   Stop Engine
   Kindly Stop Broker

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
Test Teardown       Save Logs If Failed


*** Test Cases ***
SDER
    [Documentation]    The check attempts and the max check attempts of (host_1,service_1) are changed to 280 thanks to the retention.dat file. Then engine and broker are started and broker should write these values in the services and resources tables. We only test the services table because we need a resources table that allows bigger numbers for these two attributes. But we see that broker doesn't crash anymore.
    [Tags]    MON-24549    MON-25069
    Config Engine    ${1}    ${1}    ${25}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Config BBDO3    1
    Broker Config Log    central    sql    debug
    Broker Config Log    module0    neb    trace
    Config Broker Sql Output    central    unified_sql
    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    Stop Engine

    Modify Retention Dat    0    host_1    service_1    current_attempt    280
    # modified attributes is a bit field. We must set the bit corresponding to MAX_ATTEMPTS to be allowed to change max_attempts. Otherwise it will be set to 3.
    Modify Retention Dat    0    host_1    service_1    modified_attributes    65535
    Modify Retention Dat    0    host_1    service_1    max_attempts    280

    Modify Retention Dat    0    host_1    service_1    current_state    2
    Modify Retention Dat    0    host_1    service_1    state_type    1
    Start Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    FOR    ${index}    IN RANGE    30
        Log To Console    SELECT check_attempt from services WHERE description='service_1'
        ${output}    Query    SELECT check_attempt from services WHERE description='service_1'
        Log To Console    ${output}
        IF    "${output}" == "((280,),)"    BREAK
        Sleep    1s
    END
    Should Be Equal As Strings    ${output}    ((280,),)

    Stop Engine
    Kindly Stop Broker

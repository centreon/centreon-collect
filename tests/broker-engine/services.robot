*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs 


*** Test Cases ***
SDER
    [Documentation]    The check attempts and the max check attempts of (host_1,service_1)
    ...    are changed to 280 thanks to the retention.dat file. Then Engine and Broker are started
    ...    and Broker should write these values in the services and resources tables.
    ...    We only test the services table because we need a resources table that allows bigger numbers
    ...    for these two attributes. But we see that Broker doesn't crash anymore.
    [Tags]    broker    engine    host    extcmd
    Ctn Config Engine    ${1}    ${1}    ${25}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    central    processing    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    module0    neb    trace
    Ctn Broker Config Log    module0    processing    trace
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Engine Config Replace Value In Services    0    service_1    max_check_attempts    42

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Stop Engine

    Ctn Modify Retention Dat    0    host_1    service_1    current_attempt    280
    # modified attributes is a bit field. We must set the bit corresponding to MAX_ATTEMPTS to be allowed to change max_attempts. Otherwise it will be set to 3.
    Ctn Modify Retention Dat    0    host_1    service_1    modified_attributes    65535
    Ctn Modify Retention Dat    0    host_1    service_1    max_attempts    280

    Ctn Modify Retention Dat    0    host_1    service_1    current_state    2
    Ctn Modify Retention Dat    0    host_1    service_1    state_type    1
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}


    FOR    ${index}    IN RANGE    20
        Log To Console    SELECT check_attempt from services WHERE description='service_1'
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        ${output}    Query    SELECT check_attempt from services WHERE description='service_1'
        Log To Console    ${output}
        IF    "${output}" == "((280,),)"    BREAK
        Sleep    1s
    END

    Should Be Equal As Strings    ${output}    ((280,),)

SRSAS
    [Documentation]
    ...    Given the service "service_1" on "host_1" has its "real_state" set to
    ...    "CRITICAL" in the "services" table
    ...    and its "state" set to "WARNING"
    ...    When the "cbd" service is started
    ...    Then the "state" of "service_1" is changed to "CRITICAL"
    ...    And the "real_state" of "service_1" in the "services" table is set to NULL

    [Tags]    broker    engine    service    MON-152343
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1

    Ctn Start Broker
    Ctn Start Engine

    Log To Console    Let's wait for the service to be created in the "services" table
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${t}    IN RANGE    ${60}
        ${output}    Query    SELECT count(*) FROM services WHERE description='service_1' AND enabled=1
        IF    ${output} == ((1,),)    BREAK
        Sleep    1s
    END

    Should Be Equal As Strings    ${output}    ((1,),)    We should have one service named service_1 in the "services" table
    Ctn Kindly Stop Broker

    Log To Console    Initializing real_state to CRITICAL and state to WARNING
    Execute SQL String
    ...    UPDATE services SET real_state=2, state=1 WHERE description='service_1'

    Ctn Start Broker

    Ctn Schedule Forced Service Check    host_1    service_1

    Log To Console    Let's wait for the real_state to be NULL and state to be CRITICAL
    FOR    ${t}    IN RANGE    ${60}
        ${output}    Query    SELECT real_state, state FROM services WHERE description='service_1' AND enabled=1
        IF    ${output} == ((None, 2),)    BREAK
        Sleep    1s
    END
    Should Be Equal As Strings    ${output}    ((None, 2),)    real_state should be NULL and state should be CRITICAL
    Disconnect From Database

HRSAS
    [Documentation]
    ...    Given the host "host_1" has its "real_state" set to "DOWN" in the "hosts" table
    ...    and its "state" set to "UP"
    ...    When the "cbd" service is started
    ...    Then the "state" of "host_1" is changed to "DOWN"
    ...    And the "real_state" of "host_1" in the "hosts" table is set to NULL

    [Tags]    broker    engine    host    MON-152343
    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1

    Ctn Start Broker
    Ctn Start Engine

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${t}    IN RANGE    ${60}
        ${output}    Query    SELECT count(*) FROM hosts WHERE name='host_1' AND enabled=1
        IF    ${output} == ((1,),)    BREAK
        log to console    ${output}
        Sleep    1s
    END

    Should Be Equal As Strings    ${output}    ((1,),)    We should have one host named host_1 in the hosts table
    Ctn Kindly Stop Broker

    Log To Console    Initializing real_state to DOWN and state to UP
    Execute SQL String
    ...    UPDATE hosts SET real_state=1, state=0 WHERE name='host_1';

    Ctn Start Broker

    Ctn Schedule Forced Host Check    host_1

    Log To Console    Let's wait for the real_state to be NULL and state to be DOWN
    FOR    ${t}    IN RANGE    ${60}
        ${output}    Query    SELECT real_state, state FROM hosts WHERE name='host_1' AND enabled=1
        IF    ${output} == ((None, 1),)    BREAK
        Sleep    1s
    END
    Should Be Equal As Strings    ${output}    ((None, 1),)    real_state should be NULL and state should be DOWN
    Disconnect From Database

ICON_UPDATE
    [Documentation]
    ...    Given the service "service_1" on "host_1" has its "icon_image" set to empty in the "services" and icon_id to zero in resources table
    ...    Then the "icon_image" of "service_1" is set to "test.png" and icon_id is set to 5, we restart centengine and resources and services must be updated
    ...    Then the "icon_image" of "service_1" is set to "test2.png" and icon_id is set to 55, we reload centengine and resources and services must be updated

    [Tags]    broker    engine    service    MON-23925

    Ctn Config Engine    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1

    Ctn Clear Db    services
    Ctn Clear Db    resources

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${t}    IN RANGE    ${60}
        ${output}    Query    SELECT s.service_id, s.icon_image, r.icon_id FROM services s JOIN resources r on s.host_id = r.parent_id and s.service_id = r.id WHERE s.service_id = 1
        IF    ${output} == ((1, '', 0),)    BREAK
        log to console    ${output}
        Sleep    1s
    END

    Should Be Equal As Strings    ${output}    ((1, '', 0),)    We should have service_1 without icon in the services table

    Ctn Engine Config Set Value In Services    0    service_1    icon_image    test.png
    Ctn Engine Config Set Value In Services    0    service_1    icon_id    5

    #restart engine
    ${start}    Get Current Date
    Ctn Stop Engine
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    FOR    ${t}    IN RANGE    ${60}
        ${output}    Query    SELECT s.service_id, s.icon_image, r.icon_id FROM services s JOIN resources r on s.host_id = r.parent_id and s.service_id = r.id WHERE s.service_id = 1
        IF    ${output} == ((1, 'test.png', 5),)    BREAK
        log to console    ${output}
        Sleep    1s
    END
    Should Be Equal As Strings    ${output}    ((1, 'test.png', 5),)    We should have service_1 with icon in the services table    

    Ctn Engine Config Replace Value In Services    0    service_1    icon_image    test2.png
    Ctn Engine Config Replace Value In Services    0    service_1    icon_id    55
    
    #reload engine
    Ctn Reload Engine
    FOR    ${t}    IN RANGE    ${60}
        ${output}    Query    SELECT s.service_id, s.icon_image, r.icon_id FROM services s JOIN resources r on s.host_id = r.parent_id and s.service_id = r.id WHERE s.service_id = 1
        IF    ${output} == ((1, 'test2.png', 55),)    BREAK
        log to console    ${output}
        Sleep    1s
    END
    Should Be Equal As Strings    ${output}    ((1, 'test2.png', 55),)    We should have service_1 with icon in the services table
    
    Disconnect From Database
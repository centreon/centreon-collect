*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Test Cases ***
SDER
    [Documentation]    The check attempts and the max check attempts of (host_1,service_1) are changed to 280 thanks to the retention.dat file. Then engine and broker are started and broker should write these values in the services and resources tables. We only test the services table because we need a resources table that allows bigger numbers for these two attributes. But we see that broker doesn't crash anymore.
    [Tags]    broker    engine    host    extcmd
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


LCDNU
    [Documentation]    One service is configured .
    [Tags]    broker    engine    services    lua
    Config Engine    ${1}    ${1}    ${1}
    Set Services Passive    ${0}    service_1
    Config Broker    central
    Config Broker    rrd
    Broker Config Add Item    rrd    bbdo_version    3.0.1
    Broker Config Add Item    central    bbdo_version    3.0.1
    Broker Config Flush Log    central    0
    Broker Config Log    central    core    error
    Broker Config Log    central    tcp    error
    Broker Config Log    central    sql    error
    Config Broker Sql Output    central    unified_sql
    Config Broker Remove Rrd Output    central
    Clear Retention
    Remove File    /tmp/testLUA.log
    
    ${new_content}    Catenate
    ...    function init(params)
    ...        broker_log:set_parameters(1, "/tmp/test-LUA.log")
    ...    end
    ...    
    ...    function write(d)
    ...       -- d et de type service status type id = 65565
    ...       if d._type == 65565 then
    ...           local svc = broker_cache:get_service(d.host_id, d.service_id)
    ...       -- Example using status enum 
    ...           local state = d.state
    ...           broker_log:info(0, "Service status: " .. tostring(d.state))
    ...           broker_log:info(0, "Service: " .. broker.json_encode(d)) -- pour debuguer
    ...           local now = os.time()
    ...           broker_log:info(0, "Current time: " .. tostring(now))
    ...           
    ...           --broker_log:info(0, " new state: " .. tostring(state) .. " old state: " .. tostring(svc.state))
    ...        end
    ...        return true
    ...    end

    # Create the initial LUA script file
    Create File    /tmp/test-LUA.lua    ${new_content}

    Broker Config Add Lua Output    central    test-LUA    /tmp/test-LUA.lua

    ${start}    Get Current Date
    Start Broker
    Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.


    ## Time to set the service to warning  hard
    FOR   ${i}    IN RANGE    ${4}
        Process Service Result Hard    host_1    service_1    ${1}    The service_1 is WARNING
            Sleep    1s
    END

    Set Service State    ${31}    ${1}

    ${result}    Check Service Status With Timeout    host_1    service_1    ${1}    60    HARD

    ${content}    Create List    host_1;service_1;1;The service_1
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    service1


    ## Time to set the service to UP  hard
    FOR   ${i}    IN RANGE    ${4}
        Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK
            Sleep    1s
    END

    Set Service State    ${30}    ${0}

    ${result}    Check Service Status With Timeout    host_1    service_1    ${0}    90    HARD

    ${content}    Create List    host_1;service_1;0;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    service1


    ## Time to set the service to CRITICAL HARD.
    FOR   ${i}    IN RANGE    ${4}
        Process Service Result Hard    host_1    service_1    ${2}    The service_4 is CRITICAL
            Sleep    1s
    END

    Set Service State    ${38}    ${2}

    ${result}    Check Service Status With Timeout    host_1    service_1    ${2}    60    HARD

    ${content}    Create List    host_1;service_1;2;
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    service1

    Wait Until Created    /tmp/test-LUA.log    90s
    ${grep_res}    Grep File    /tmp/test-LUA.log    name:
    Should Not Be Empty    ${grep_res}    service name not found

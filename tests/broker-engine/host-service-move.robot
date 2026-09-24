*** Settings ***
Documentation       When a host or service is moved between pollers, brokermust not evict it from cache.
Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BEMC1
    [Documentation]    When host_1 is moved from engine0 to engine1, reloading engine1 first
    ...                (so broker cache records instance_id=2 for host_1) and then reloading
    ...                engine0 (which sends a DELETE with instance_id=1) must not evict host_1
    ...                from broker cache. The broker log must contain "ignoring stale deletion".
    [Tags]    broker    engine    cache    MON-197731
    Ctn Config Engine    ${2}    ${5}    ${0}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${2}
    Ctn Config BBDO3    ${2}
    Ctn Broker Config Log    central    core    debug
    Ctn Broker Config Log    central    lua    trace
    Remove File    /tmp/test-LUA.log
    Ctn Broker Config Add Lua Output    central    test-cache    ${SCRIPTS}dump_host.lua
    ${start}    Get Current Date
    Ctn Start Broker
    Wait Until Created    /tmp/test-LUA.log
    Ctn Start Engine

    # Wait for both engines to be ready
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    engine0 did not reach check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog1}    ${start}    ${content}    60
    Should Be True    ${result}    engine1 did not reach check_for_external_commands

    # Wait for host_1 (owned by engine0 / instance_id=1) to be registered in DB
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT instance_id FROM hosts WHERE name='host_1' AND enabled=1
        Sleep    1s
        IF    "${output}" == "((1,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((1,),)
    Log To Console    "host_1 have the instance" ${output}

    # --- Simulate moving host_1 from engine0 to engine1 ---
    # Step 1: move host_1 from engine0's config to engine1's config
    Ctn Engine Config Move Host To Engine    0    1    host_1

    # Step 2: reload engine1 so broker learns host_1 belongs to instance_id=2
    ${reload1_time}    Get Current Date
    Ctn Reload Engine    1
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog1}    ${reload1_time}    ${content}    60
    Should Be True    ${result}    engine1 did not complete reload

    FOR    ${index}    IN RANGE    30
        ${output}    Query    SELECT instance_id FROM hosts WHERE name='host_1' AND enabled=1
        Sleep    1s
        IF    "${output}" == "((2,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((2,),)

    # Step 3: reload engine0 so it sends DELETE(host_1, instance_id=1)
    ${reload0_time}    Get Current Date
    Ctn Reload Engine    0
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${reload0_time}    ${content}    60
    Should Be True    ${result}    engine0 did not complete reload

    # Verify host_1 is still visible through broker cache after the stale DELETE.
    FOR    ${index}    IN RANGE    30
        ${lua_output}    Get File    /tmp/test-LUA.log
        IF    'host 1 :' in $lua_output    BREAK
        Sleep    1s
    END
    Should Contain    ${lua_output}    host 1 :    host_1 was removed from broker cache after the stale DELETE

    # The guard must fire and log the stale deletion
    ${content}    Create List    cache: ignoring stale deletion of host
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${reload0_time}    ${content}    30
    Should Be True    ${result}    Broker did not ignore the stale DELETE for host_1

    Ctn Stop Engine
    Ctn Kindly Stop Broker


BEMC2
    [Documentation]    When service_1 on host_1 is moved from engine0 to engine1, reloading
    ...                engine1 first (broker records instance_id=2) and then reloading engine0
    ...                (DELETE with instance_id=1) must not evict service_1 from broker cache.
    [Tags]    broker    engine    cache    MON-197731
    Ctn Config Engine    ${2}    ${5}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${2}
    Ctn Config BBDO3    ${2}
    Ctn Broker Config Log    central    core    debug
    Ctn Broker Config Log    central    lua    trace
    Remove File    /tmp/test-LUA.log
    Ctn Broker Config Add Lua Output    central    test-cache    ${SCRIPTS}dump_service.lua
    ${start}    Get Current Date
    Ctn Start Broker
    Wait Until Created    /tmp/test-LUA.log
    Ctn Start Engine

    # Wait for both engines to be ready
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    engine0 did not reach check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog1}    ${start}    ${content}    60
    Should Be True    ${result}    engine1 did not reach check_for_external_commands

    # Wait for host_1 and service_1 to be registered in DB
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT count(*) FROM services s JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.enabled=1
        Sleep    1s
        IF    "${output}" == "((1,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((1,),)

    # --- Simulate moving host_1 + service_1 from engine0 to engine1 ---
    # Step 1: move host_1 and its services from engine0's config to engine1's config
    Ctn Engine Config Move Host To Engine    0    1    host_1
    Ctn Engine Config Move Services To Engine    0    1    host_1

    # Step 2: reload engine1 so broker learns service_1 belongs to instance_id=2
    ${reload1_time}    Get Current Date
    Ctn Reload Engine    1
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog1}    ${reload1_time}    ${content}    60
    Should Be True    ${result}    engine1 did not complete reload

    FOR    ${index}    IN RANGE    30
        ${output}    Query    SELECT h.instance_id FROM services s JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.enabled=1
        Sleep    1s
        IF    "${output}" == "((2,),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    ((2,),)

    # Step 3: reload engine0 — it detects service_1 is gone and sends DELETE(service_1, instance_id=1)
    Run    truncate -s 0 /tmp/test-LUA.log
    ${reload0_time}    Get Current Date
    Ctn Reload Engine    0
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${reload0_time}    ${content}    60
    Should Be True    ${result}    engine0 did not complete reload

    # Verify service_1 is still visible through broker cache after the stale DELETE.
    FOR    ${index}    IN RANGE    30
        ${lua_output}    Get File    /tmp/test-LUA.log
        IF    'serv 1 :' in $lua_output    BREAK
        Sleep    1s
    END
    Should Contain    ${lua_output}    serv 1 :    service_1 was removed from broker cache after the stale DELETE

    # The guard must fire and log the stale deletion for the service
    ${content}    Create List    cache: ignoring stale deletion of service
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${reload0_time}    ${content}    30
    Should Be True    ${result}    Broker did not ignore the stale DELETE for service_1

    Ctn Stop Engine
    Ctn Kindly Stop Broker

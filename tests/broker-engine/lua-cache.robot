*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
LCDNU
    [Documentation]    the lua cache updates correctly service cache.
    [Tags]    broker    engine    services    lua    MON-24745
    Ctn Clear Commands Status
    Ctn Clear Retention

    Remove File    /tmp/test-LUA.log
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    neb    trace
    Ctn Broker Config Log    central    sql    error
    Ctn Broker Config Log    central    lua    debug

    ${new_content}    Catenate    SEPARATOR=\n
    ...    function init(params)
    ...        broker_log:set_parameters(2, '/tmp/test-LUA.log')
    ...        broker_log:info(0, 'lua start test')
    ...    end
    ...    
    ...    function write(e)
    ...        if e._type == 65563 then --Service id
    ...            broker_log:info(0, 'configuration of ('.. e.host_id.. ','.. e.service_id.. ')')
    ...            local svc = broker_cache:get_service(e.host_id,e.service_id)
    ...            broker_log:info(0, broker.json_encode(svc))
    ...        end
    ...        if e._type == 65565 then --service status ID
    ...            broker_log:info(0, 'Status of ('.. e.host_id.. ','.. e.service_id.. ')')
    ...            local svc = broker_cache:get_service(e.host_id,e.service_id)
    ...            local field = {"checked", "check_type", "state", "state_type", "last_state_change", "last_hard_state", "last_hard_state_change", "last_time_ok", "last_time_warning", "last_time_critical", "last_time_unknown", "output", "perfdata", "flapping", "percent_state_change", "latency", "execution_time", "last_check", "next_check", "should_be_scheduled", "check_attempt", "notification_number", "no_more_notifications", "last_notification", "next_notification", "acknowledgement_type", "scheduled_downtime_depth"}
    ...            local ko = 0
    ...            for i, v in ipairs(field) do
    ...              if svc[v] ~= e[v] then
    ...                broker_log:info(0, v.." doesn't match (".. svc[v].." ~= ".. e[v]..")")
    ...                ko = ko + 1
    ...              end
    ...            end
    ...            if ko == 0 then
    ...              broker_log:info(0, "Service cache OK")
    ...            end
    ...        end
    ...        return true
    ...    end

    # Create the initial LUA script file
    Create File    /tmp/test-LUA.lua    ${new_content}

    Ctn Broker Config Add Lua Output    central    test-LUA    /tmp/test-LUA.lua

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Wait Until Created    /tmp/test-LUA.log
    FOR    ${i}    IN RANGE    60
        ${result}    Grep File    /tmp/test-LUA.log    configuration of (1,1)    regexp=False
        IF    len("""${result}""") > 0
	    Log To Console    ${result}
	    BREAK
	END
	Sleep    1s
    END
    Should Not Be Empty    ${result}    Configuration error

    ## Time to set the service to OK hard

    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK

    ## check that we check the correct service
    FOR    ${i}    IN RANGE    60
        ${grep_res}    Grep File    /tmp/test-LUA.log    Status of
        IF    len("""${grep_res}""") > 0    BREAK
	Sleep    1s
    END
    Should Not Be Empty    ${result}    No message about the service (1,1) status

    FOR    ${i}    IN RANGE    60
        ${grep_res}    Grep File    /tmp/test-LUA.log    Service cache OK    regexp=False
        IF    len("""${grep_res}""") > 0    BREAK
	Sleep    1s
    END
    Should Not Be Empty    ${grep_res}    Some checks failed

    Ctn Stop Engine
    Ctn Kindly Stop Broker


LCDNUH
    [Documentation]    the lua cache updates correctly host cache
    [Tags]    broker    engine    services    lua    MON-24745
    Ctn Clear Commands Status
    Ctn Clear Retention

    Remove File    /tmp/test-LUA.log
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    neb    trace
    Ctn Broker Config Log    central    sql    error
    Ctn Broker Config Log    central    lua    debug
    Ctn Config Broker Sql Output    central    unified_sql

    ${new_content}    Catenate    SEPARATOR=\n
    ...    function init(params)
    ...        broker_log:set_parameters(2, '/tmp/test-LUA.log')
    ...        broker_log:info(0, 'lua start test')
    ...    end
    ...
    ...    function write(e)
    ...        if e._type == 65566 then --Host ID
    ...            broker_log:info(0, 'configuration of ('.. e.host_id.. ')')
    ...        end
    ...        if e._type == 65568 then --Host status ID
    ...            broker_log:info(0, 'Status of ('.. e.host_id.. ')')
    ...            local host = broker_cache:get_host(e.host_id)
    ...            local field = {"checked", "check_type", "state", "state_type", "last_state_change", "last_hard_state", "last_hard_state_change", "last_time_up", "last_time_down", "last_time_unreachable", "output", "perfdata", "flapping", "percent_state_change", "latency", "execution_time", "last_check", "next_check", "should_be_scheduled", "check_attempt", "notification_number", "no_more_notifications", "last_notification", "next_host_notification", "acknowledgement_type", "scheduled_downtime_depth"}
    ...            local ko = 0
    ...            for i, v in ipairs(field) do
    ...              if host[v] ~= e[v] then
    ...                broker_log:info(0, v.." doesn't match (".. host[v].." ~= ".. e[v]..")")
    ...                ko = ko + 1
    ...              end
    ...            end
    ...            if ko == 0 then
    ...              broker_log:info(0, "Host cache OK")
    ...            end
    ...        end
    ...        return true
    ...    end

    # Create the initial LUA script file
    Create File    /tmp/test-LUA.lua    ${new_content}

    Ctn Broker Config Add Lua Output    central    test-LUA    /tmp/test-LUA.lua

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Wait Until Created    /tmp/test-LUA.log
    FOR    ${i}    IN RANGE    60
        ${result}    Grep File    /tmp/test-LUA.log    configuration of (1)    regexp=False
        IF    len("""${result}""") > 0    BREAK
	Sleep    1s
    END
    Should Not Be Empty    ${result}    Configuration error

    ## Time to set the host to UP hard

    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    0    host_1 UP
    END

    FOR    ${i}    IN RANGE    60
        ${grep_res}    Grep File    /tmp/test-LUA.log    Status of
        IF    len("""${grep_res}""") > 0    BREAK
	Sleep    1s
    END
    Should Not Be Empty    ${result}    No message about the host (1) status

    FOR    ${i}    IN RANGE    60
        ${grep_res}    Grep File    /tmp/test-LUA.log    Host cache OK    regexp=False
        IF    len("""${grep_res}""") > 0    BREAK
	Sleep    1s
    END
    Should Not Be Empty    ${grep_res}    Some checks failed

    Ctn Stop Engine
    Ctn Kindly Stop Broker

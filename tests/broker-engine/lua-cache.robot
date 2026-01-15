*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource
Library             ../resources/lua.py

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs 


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
    Ctn Config Broker Sql Output    central    unified_sql

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
        IF    len("""${result}""") > 0    BREAK
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

LUA_CACHE_SAVE_BBDO3
    [Documentation]    Given a engine broker configured in bbdo2, we check that services and hosts are stored in bbdo3 format in cache
    ...    To do that we compare host and service event with lua cache
    [Tags]    broker    engine    services    lua    MON-157906
    Ctn Clear Commands Status
    Ctn Clear Retention

    Remove File    /tmp/test-LUA.log
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
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
    ...        if e._type == 65548 then --Host ID
    ...            broker_log:info(0, 'configuration of ('.. e.host_id.. ')')
    ...            local host = broker_cache:get_host(e.host_id)
    ...            local field = { "host_id", "acknowledged", "enabled", "check_command", "check_interval", "check_period", "event_handler_enabled", "event_handler", "execution_time",  "notification_number",   "latency", "max_check_attempts", "next_check", "no_more_notifications", "output", "percent_state_change", "retry_interval", "should_be_scheduled", "action_url", "address", "alias", "check_freshness", "default_event_handler_enabled", "display_name", "first_notification_delay", "flap_detection_on_down", "flap_detection_on_unreachable", "flap_detection_on_up", "freshness_threshold", "high_flap_threshold", "icon_image", "icon_image_alt", "low_flap_threshold", "notes", "notes_url", "notification_interval", "notification_period", "notify_on_down", "notify_on_downtime", "notify_on_flapping", "notify_on_recovery", "notify_on_unreachable", "stalk_on_down", "stalk_on_unreachable", "stalk_on_up", "statusmap_image", "retain_nonstatus_information", "retain_status_information", "timezone"}
    ...            local ko = 0
    ...            for i, v in ipairs(field) do
    ...              if host[v] ~= e[v] then
    ...                broker_log:info(0, v.." doesn't match ".. host[v])
    ...                broker_log:info(0, v.." doesn't match (".. host[v].." ~= ".. e[v]..")")
    ...                ko = ko + 1
    ...              end
    ...            end
    ...            if ko == 0 then
    ...              broker_log:info(0, "Host cache OK")
    ...            else
    ...              broker_log:info(0, "Host cache KO for host ".. e.host_id) 
    ...            end
    ...        end
    ...        if e._type == 65559 then --Service ID
    ...            broker_log:info(0, 'Configuration of service ('.. e.host_id.. ":".. e.service_id.. ')')
    ...            local service = broker_cache:get_service(e.host_id, e.service_id)
    ...            local field = {"host_id", "service_id", "acknowledged", "enabled", "check_command", "check_interval", "check_period", "event_handler_enabled", "event_handler", "execution_time",  "notification_number", "latency", "max_check_attempts", "next_check", "no_more_notifications", "percent_state_change", "retry_interval", "should_be_scheduled", "action_url", "check_freshness", "default_event_handler_enabled", "display_name", "first_notification_delay", "flap_detection_on_critical", "flap_detection_on_ok", "flap_detection_on_unknown", "flap_detection_on_warning", "freshness_threshold", "high_flap_threshold", "low_flap_threshold", "icon_image", "icon_image_alt", "notes", "notes_url", "notification_interval", "notification_period", "notify_on_critical", "notify_on_downtime", "notify_on_flapping", "notify_on_recovery", "notify_on_unknown", "notify_on_warning", "stalk_on_critical", "stalk_on_ok", "stalk_on_unknown", "stalk_on_warning", "retain_nonstatus_information", "retain_status_information"}
    ...            local ko = 0
    ...            for i, v in ipairs(field) do
    ...              if service[v] ~= e[v] then
    ...                broker_log:info(0, v.." doesn't match ")
    ...                broker_log:info(0, v.." doesn't match (".. service[v].." ~= ".. e[v]..")")
    ...                ko = ko + 1
    ...              end
    ...            end
    ...            if ko == 0 then
    ...              broker_log:info(0, "Service cache OK")
    ...            else
    ...              broker_log:info(0, "Service cache KO for service ".. e.host_id.. e.service_id)
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
        ${result}    Grep File    /tmp/test-LUA.log    Host cache OK    regexp=False
        IF    len("""${result}""") > 0    BREAK
        Sleep    1s
    END
    Should Not Be Empty    ${result}    Host cache OK not found in lua output
    FOR    ${i}    IN RANGE    60
        ${result}    Grep File    /tmp/test-LUA.log   Service cache OK    regexp=False
        IF    len("""${result}""") > 0    BREAK
        Sleep    1s
    END
    Should Not Be Empty    ${result}    Service cache OK not found in lua output

LUA_CACHE_SAVE_HOST_GROUP
    [Documentation]    Given two engines configured, 
    ...    we create a first host group of two hosts of poller1, then we create a second host group that only contains one host of poller1
    ...    then we delete all hosts from second host group, we delete a host from first host
    ...    we create a group that contains one host of each poller, we remove first host group, we remove one host from the last created group
    ...    we expect that lua cache will be synchronized with all that modifications

    [Tags]    broker    engine    services    lua    MON-191980
    Ctn Clear Commands Status
    Ctn Clear Retention

    Remove File    /tmp/test-LUA.log
    Ctn Config Engine    ${2}    ${4}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${2}
    Ctn Config Broker    rrd
    Ctn Config BBDO3    ${2}

    Ctn Broker Config Log    central    neb    trace
    Ctn Broker Config Log    central    lua    debug
    Ctn Broker Config Add Lua Output    central    test-LUA    ${SCRIPTS}/dump_host.lua

    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2"]

    Ctn Clear Db    instances
    Ctn Clear Db    hosts
    Ctn Clear Db    services

    ${empty_dict}    Create Dictionary

    Log To Console    group 1 ["host_1", "host_2"]
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${content}    Create List    processing pb host group
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    1
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    1    no host group 1 for host_1 ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    2
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    1    no host group 1 for host_2 ${host_info}

    ${hostgroup_info}    Ctn Get Hostgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${hostgroup_info}[name]    hostgroup_1    no hostgroup 1 ${hostgroup_info}

    # add host group 2 with host_2 inside
    Log To Console    group 1 ["host_1", "host_2"] group 2 ["host_2"]
    Ctn Add Host Group    ${0}    ${2}    ["host_2"]
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    1
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    1    no host group 1 for host_1 ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    2
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    1    no host group 1 for host_2 ${host_info}
    Should Be Equal    ${host_info}[hostgroups][2][name][group_id]    2    no host group 2 for host_2 ${host_info} 

    ${hostgroup_info}    Ctn Get Hostgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${hostgroup_info}[name]    hostgroup_1    no hostgroup 1 ${hostgroup_info}

    ${hostgroup_info}    Ctn Get Hostgroup Cache Info    /tmp/test-LUA.log    ${2}
    Should Be Equal    ${hostgroup_info}[name]    hostgroup_2    no hostgroup 2 ${hostgroup_info}

    #remove host group 2
    Sleep    ${1}
    Log To Console    group 1 ["host_1", "host_2"]
    Remove File    /tmp/test-LUA.log
    Ctn Engine Config Del Block In Cfg    ${0}    hostgroup    2    hostgroups.cfg
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    1
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    1    no host group 1 for host_1 ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    2
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    1    no host group 1 for host_2 ${host_info}
    ${len}    Evaluate    len(${host_info}[hostgroups])
    Should Be Equal    ${len}    ${1}    several host groups for host 2: ${host_info}

    ${hostgroup_info}    Ctn Get Hostgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${hostgroup_info}[name]    hostgroup_1    no hostgroup 1 ${hostgroup_info}

    ${hostgroup_info}    Ctn Get Hostgroup Cache Info    /tmp/test-LUA.log    ${2}
    Should Be Equal    ${hostgroup_info}    ${empty_dict}    hostgroup 2 ${hostgroup_info}


    # only host_2 in host group 1
    Sleep    ${1}
    Log To Console    group 1 ["host_2"]
    Remove File    /tmp/test-LUA.log
    Remove File    ${EtcRoot}/centreon-engine/config0/hostgroups.cfg
    Ctn Add Host Group    ${0}    ${1}    ["host_2"]
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    2
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    1    no host group 1 for host_2 ${host_info}
    ${len}    Evaluate    len(${host_info}[hostgroups])
    Should Be Equal    ${len}    ${1}    several host groups for host 2: ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    1
    ${len}    Evaluate    len(${host_info}[hostgroups])
    Should Be Equal    ${len}    ${0}    at least one host groups for host 1: ${host_info}

    ${hostgroup_info}    Ctn Get Hostgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${hostgroup_info}[name]    hostgroup_1    no hostgroup 1 ${hostgroup_info}

    ${hostgroup_info}    Ctn Get Hostgroup Cache Info    /tmp/test-LUA.log    ${2}
    Should Be Equal    ${hostgroup_info}    ${empty_dict}    hostgroup 2 ${hostgroup_info}

    #group1 poller0, group 2 on two pollers
    Sleep    ${1}
    Log To Console    group 1 ["host_1", "host_2"] group 2 ["host_2", "host_3", "host_4"]
    Remove File    /tmp/test-LUA.log
    Remove File    ${EtcRoot}/centreon-engine/config0/hostgroups.cfg
    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2"]
    Ctn Add Host Group    ${0}    ${2}    ["host_2"]
    Ctn Add Host Group    ${1}    ${2}    ["host_3", "host_4"]
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    1
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    1    no host group 1 for host_1 ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    2
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    1    no host group 1 for host_2 ${host_info}
    Should Be Equal    ${host_info}[hostgroups][2][name][group_id]    2    no host group 2 for host_2 ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    3
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    2    no host group 2 for host_3 ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    4
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    2    no host group 2 for host_4 ${host_info}

    ${hostgroup_info}    Ctn Get Hostgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${hostgroup_info}[name]    hostgroup_1    no hostgroup 1 ${hostgroup_info}

    ${hostgroup_info}    Ctn Get Hostgroup Cache Info    /tmp/test-LUA.log    ${2}
    Should Be Equal    ${hostgroup_info}[name]    hostgroup_2    no hostgroup 2 ${hostgroup_info}


    #remove host group 1
    Sleep    ${1}
    Log To Console    group 2 ["host_2", "host_3", "host_4"]
    Remove File    /tmp/test-LUA.log
    Remove File    ${EtcRoot}/centreon-engine/config0/hostgroups.cfg
    Remove File    ${EtcRoot}/centreon-engine/config1/hostgroups.cfg
    Ctn Add Host Group    ${0}    ${2}    ["host_2"]
    Ctn Add Host Group    ${1}    ${2}    ["host_3", "host_4"]
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    1
    ${len}    Evaluate    len(${host_info}[hostgroups])
    Should Be Equal    ${len}    ${0}    at least one host groups for host 1: ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    2
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    2    no host group 2 for host_2 ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    3
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    2    no host group 2 for host_3 ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    4
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    2    no host group 2 for host_4 ${host_info}

    ${hostgroup_info}    Ctn Get Hostgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${hostgroup_info}    ${empty_dict}    hostgroup 1 ${hostgroup_info}

    ${hostgroup_info}    Ctn Get Hostgroup Cache Info    /tmp/test-LUA.log    ${2}
    Should Be Equal    ${hostgroup_info}[name]    hostgroup_2    no hostgroup 2 ${hostgroup_info}

    #remove host 4 from group 2
    Sleep    ${1}
    Log To Console    group 2 [ "host_2", "host_3"]
    Remove File    /tmp/test-LUA.log
    Remove File    ${EtcRoot}/centreon-engine/config1/hostgroups.cfg
    Ctn Add Host Group    ${1}    ${2}    ["host_3"]
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    1
    ${len}    Evaluate    len(${host_info}[hostgroups])
    Should Be Equal    ${len}    ${0}    at least one host groups for host 1: ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    2
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    2    no host group 2 for host_2 ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    3
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    2    no host group 2 for host_3 ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    4
    ${len}    Evaluate    len(${host_info}[hostgroups])
    Should Be Equal    ${len}    ${0}    at least one host groups for host 4: ${host_info}

    ${hostgroup_info}    Ctn Get Hostgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${hostgroup_info}    ${empty_dict}    hostgroup 1 ${hostgroup_info}

    ${hostgroup_info}    Ctn Get Hostgroup Cache Info    /tmp/test-LUA.log    ${2}
    Should Be Equal    ${hostgroup_info}[name]    hostgroup_2    no hostgroup 2 ${hostgroup_info}

    #move host 2 to group 1
    Sleep    ${1}
    Log To Console    group 1 ["host_1", "host_2"] group 2 ["host_3"]
    Remove File    /tmp/test-LUA.log
    Remove File    ${EtcRoot}/centreon-engine/config0/hostgroups.cfg
    Ctn Add Host Group    ${0}    ${1}    ["host_1", "host_2"]
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    1
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    1    no host group 1 for host_1 ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    2
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    1    no host group 1 for host_2 ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    3
    Should Be Equal    ${host_info}[hostgroups][1][name][group_id]    2    no host group 2 for host_3 ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    4
    ${len}    Evaluate    len(${host_info}[hostgroups])
    Should Be Equal    ${len}    ${0}    at least one host groups for host 4: ${host_info}

    ${hostgroup_info}    Ctn Get Hostgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${hostgroup_info}[name]    hostgroup_1    no hostgroup 1 ${hostgroup_info}

    ${hostgroup_info}    Ctn Get Hostgroup Cache Info    /tmp/test-LUA.log    ${2}
    Should Be Equal    ${hostgroup_info}[name]    hostgroup_2    no hostgroup 2 ${hostgroup_info}

    #remove all host groups
    Sleep    ${1}
    Log To Console    Remove all host groups
    Remove File    /tmp/test-LUA.log
    Ctn Config Engine Remove Cfg File    ${0}    hostgroups.cfg
    Ctn Config Engine Remove Cfg File    ${1}    hostgroups.cfg
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${5}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    1
    ${len}    Evaluate    len(${host_info}[hostgroups])
    Should Be Equal    ${len}    ${0}    at least one host groups for host 1: ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    2
    ${len}    Evaluate    len(${host_info}[hostgroups])
    Should Be Equal    ${len}    ${0}    at least one host groups for host 2: ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    3
    ${len}    Evaluate    len(${host_info}[hostgroups])
    Should Be Equal    ${len}    ${0}    at least one host groups for host 3: ${host_info}

    ${host_info}    Ctn Get Host Cache Info    /tmp/test-LUA.log    4
    ${len}    Evaluate    len(${host_info}[hostgroups])
    Should Be Equal    ${len}    ${0}    at least one host groups for host 4: ${host_info}


LUA_CACHE_SAVE_SERVICE_GROUP
    [Documentation]    Given two engines configured, 
    ...    we create a first service group of two services of poller1, then we create a second service group that only contains one service of poller1
    ...    then we delete all services from second service group, we delete a service from first service
    ...    we create a group that contains one service of each poller, we remove first service group, we remove one service from the last created group
    ...    we expect that lua cache will be synchronized with all that modifications

    [Tags]    broker    engine    services    lua    MON-191980
    Ctn Clear Commands Status
    Ctn Clear Retention

    Remove File    /tmp/test-LUA.log
    Ctn Config Engine    ${2}    ${2}    ${2}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${2}
    Ctn Config Broker    rrd
    Ctn Config BBDO3    ${2}

    #Ctn Broker Config Log    central    processing    trace
    Ctn Broker Config Log    central    neb    trace
    Ctn Broker Config Log    central    lua    debug
    Ctn Broker Config Add Lua Output    central    test-LUA    ${SCRIPTS}/dump_service.lua
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg

    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_1", "host_1","service_2"]


    Ctn Clear Db    instances
    Ctn Clear Db    hosts
    Ctn Clear Db    services

    ${empty_dict}    Create Dictionary

    Log To Console    group 1 ["host_1","service_1", "host_1","service_2"]
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${content}    Create List    processing pb service group
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    1
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    1    no serv group 1 for service_1 ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    2
     Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    1    no serv group 1 for host_2 ${serv_info}

    ${servgroup_info}    Ctn Get Servgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${servgroup_info}[name]    servicegroup_1    no servgroup 1 ${servgroup_info}

    # add service group 2 with service_2 inside
    Log To Console    group 1 ["host_1","service_1", "host_1","service_2"] group 2 ["host_1", "service_2"]
    Ctn Add Service Group    ${0}    ${2}    ["host_1","service_2"]
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    1
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    1    no serv group 1 for host_1 ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    2
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    1    no serv group 1 for host_2 ${serv_info}
    Should Be Equal    ${serv_info}[servgroups][2][name][group_id]    2    no serv group 2 for host_2 ${serv_info} 

    ${servgroup_info}    Ctn Get Servgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${servgroup_info}[name]    servicegroup_1    no servgroup 1 ${servgroup_info}

    ${servgroup_info}    Ctn Get Servgroup Cache Info    /tmp/test-LUA.log    ${2}
    Should Be Equal    ${servgroup_info}[name]    servicegroup_2    no servgroup 2 ${servgroup_info}

    #remove service group 2
    Sleep    ${1}
    Log To Console    group 1 ["host_1","service_1", "host_1","service_2"]
    Remove File    /tmp/test-LUA.log
    Ctn Engine Config Del Block In Cfg    ${0}    servicegroup    2    servicegroups.cfg
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    1
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    1    no serv group 1 for service_1 ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    2
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    1    no serv group 1 for service_2 ${serv_info}
    ${len}    Evaluate    len(${serv_info}[servgroups])
    Should Be Equal    ${len}    ${1}    several service groups for service 2: ${serv_info}

    ${servgroup_info}    Ctn Get Servgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${servgroup_info}[name]    servicegroup_1    no servgroup 1 ${servgroup_info}

    ${servgroup_info}    Ctn Get Servgroup Cache Info    /tmp/test-LUA.log    ${2}
    Should Be Equal    ${servgroup_info}    ${empty_dict}    servicegroup 2 ${servgroup_info}


    # only service_2 in service group 1
    Sleep    ${1}
    Log To Console    group 1 ["host_1","service_2"]
    Remove File    /tmp/test-LUA.log
    Remove File    ${EtcRoot}/centreon-engine/config0/servicegroups.cfg
    Ctn Add Service Group    ${0}    ${1}    ["host_1","service_2"]
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    2
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    1    no serv group 1 for service_2 ${serv_info}
    ${len}    Evaluate    len(${serv_info}[servgroups])
    Should Be Equal    ${len}    ${1}    several service groups for service 2: ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    1
    ${len}    Evaluate    len(${serv_info}[servgroups])
    Should Be Equal    ${len}    ${0}    at least one service groups for service 1: ${serv_info}

    ${servgroup_info}    Ctn Get Servgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${servgroup_info}[name]    servicegroup_1    no servgroup 1 ${servgroup_info}

    ${servgroup_info}    Ctn Get Servgroup Cache Info    /tmp/test-LUA.log    ${2}
    Should Be Equal    ${servgroup_info}    ${empty_dict}    servicegroup 2 ${servgroup_info}

    #group1 poller0, group 2 on two pollers
    Sleep    ${1}
    Log To Console    group 1 ["host_1" "service_1", "host_1" "service_2"] group 2 ["host_1" "service_2", "host_2" "service_3", "host_2" "service_4"]
    Remove File    /tmp/test-LUA.log
    Remove File    ${EtcRoot}/centreon-engine/config0/servicegroups.cfg
    Ctn Config Engine Add Cfg File    ${1}    servicegroups.cfg
    Ctn Add Service Group    ${0}    ${1}    ["host_1", "service_1", "host_1", "service_2"]
    Ctn Add Service Group    ${0}    ${2}    ["host_1", "service_2"]
    Ctn Add Service Group    ${1}    ${2}    ["host_2", "service_3", "host_2", "service_4"]
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    1
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    1    no serv group 1 for service_1 ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    2
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    1    no serv group 1 for service_2 ${serv_info}
    Should Be Equal    ${serv_info}[servgroups][2][name][group_id]    2    no serv group 2 for service_2 ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    3
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    2    no serv group 2 for service_3 ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    4
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    2    no serv group 2 for service_4 ${serv_info}

    ${servgroup_info}    Ctn Get Servgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${servgroup_info}[name]    servicegroup_1    no servgroup 1 ${servgroup_info}

    ${servgroup_info}    Ctn Get Servgroup Cache Info    /tmp/test-LUA.log    ${2}
    Should Be Equal    ${servgroup_info}[name]    servicegroup_2    no servgroup 2 ${servgroup_info}


    #remove service group 1
    Sleep    ${1}
    Log To Console    group 2 ["host_1" "service_2", "host_2" "service_3", "host_2" "service_4"]
    Remove File    /tmp/test-LUA.log
    Remove File    ${EtcRoot}/centreon-engine/config0/servicegroups.cfg
    Remove File    ${EtcRoot}/centreon-engine/config1/servicegroups.cfg
    Ctn Add Service Group    ${0}    ${2}    ["host_1", "service_2"]
    Ctn Add Service Group    ${1}    ${2}    ["host_2","service_3", "host_2", "service_4"]
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    1
    ${len}    Evaluate    len(${serv_info}[servgroups])
    Should Be Equal    ${len}    ${0}    at least one service groups for service 1: ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    2
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    2    no serv group 2 for service_2 ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    3
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    2    no serv group 2 for service_3 ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    4
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    2    no serv group 2 for service_4 ${serv_info}

    ${servgroup_info}    Ctn Get Servgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${servgroup_info}    ${empty_dict}    servicegroup 1 ${servgroup_info}

    ${servgroup_info}    Ctn Get Servgroup Cache Info    /tmp/test-LUA.log    ${2}
    Should Be Equal    ${servgroup_info}[name]    servicegroup_2    no servgroup 2 ${servgroup_info}

    #remove service 4 from group 2
    Sleep    ${1}
    Log To Console    group 2 [ "host_1" "service_2", "host_2" "service_3"]
    Remove File    /tmp/test-LUA.log
    Remove File    ${EtcRoot}/centreon-engine/config1/servicegroups.cfg
    Ctn Add Service Group    ${1}    ${2}    ["host_2", "service_3"]
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    1
    ${len}    Evaluate    len(${serv_info}[servgroups])
    Should Be Equal    ${len}    ${0}    at least one service groups for service 1: ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    2
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    2    no serv group 2 for service_2 ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    3
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    2    no serv group 2 for service_3 ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    4
    ${len}    Evaluate    len(${serv_info}[servgroups])
    Should Be Equal    ${len}    ${0}    at least one host groups for service 4: ${serv_info}

    ${servgroup_info}    Ctn Get Servgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${servgroup_info}    ${empty_dict}    servicegroup 1 ${servgroup_info}

    ${servgroup_info}    Ctn Get Servgroup Cache Info    /tmp/test-LUA.log    ${2}
    Should Be Equal    ${servgroup_info}[name]    servicegroup_2    no servgroup 2 ${servgroup_info}

    #move service 2 to group 1
    Sleep    ${1}
    Log To Console    group 1 ["host_1" "service_1", "host_1" "service_2"] group 2 ["host_2" "service_3"]
    Remove File    /tmp/test-LUA.log
    Remove File    ${EtcRoot}/centreon-engine/config0/servicegroups.cfg
    Ctn Add Service Group    ${0}    ${1}    ["host_1", "service_1", "host_1", "service_2"]
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${1}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    1
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    1    no serv group 1 for service_1 ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    2
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    1    no serv group 1 for service_2 ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    3
    Should Be Equal    ${serv_info}[servgroups][1][name][group_id]    2    no serv group 2 for service_3 ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    4
    ${len}    Evaluate    len(${serv_info}[servgroups])
    Should Be Equal    ${len}    ${0}    at least one service groups for service 4: ${serv_info}

    ${servgroup_info}    Ctn Get Servgroup Cache Info    /tmp/test-LUA.log    ${1}
    Should Be Equal    ${servgroup_info}[name]    servicegroup_1    no servgroup 1 ${servgroup_info}

    ${servgroup_info}    Ctn Get Servgroup Cache Info    /tmp/test-LUA.log    ${2}
    Should Be Equal    ${servgroup_info}[name]    servicegroup_2    no servgroup 2 ${servgroup_info}

    #remove all host groups
    Sleep    ${1}
    Log To Console    Remove all service groups
    Remove File    /tmp/test-LUA.log
    Ctn Config Engine Remove Cfg File    ${0}    servicegroups.cfg
    Ctn Config Engine Remove Cfg File    ${1}    servicegroups.cfg
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine

    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Sleep    ${2}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    1
    ${len}    Evaluate    len(${serv_info}[servgroups])
    Should Be Equal    ${len}    ${0}    at least one service groups for service 1: ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    2
    ${len}    Evaluate    len(${serv_info}[servgroups])
    Should Be Equal    ${len}    ${0}    at least one service groups for service 2: ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    3
    ${len}    Evaluate    len(${serv_info}[servgroups])
    Should Be Equal    ${len}    ${0}    at least one service groups for service 3: ${serv_info}

    ${serv_info}    Ctn Get Service Cache Info    /tmp/test-LUA.log    4
    ${len}    Evaluate    len(${serv_info}[servgroups])
    Should Be Equal    ${len}    ${0}    at least one service groups for service 4: ${serv_info}





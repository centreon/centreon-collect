*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
LCDNU
    [Documentation]    the lua cache does not update last_state_change
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
    ...            broker_log:info(0, 'Status of ('.. e.host_id.. ','.. e.service_id.. '):'.. broker.json_encode(e))
    ...            local svc = broker_cache:get_service(e.host_id,e.service_id)
    ...            if svc.last_state_change ~= e.last_state_change then
    ...                broker_log:info(0, 'last state change doesnt match ('.. svc.last_state_change..'~'.. e.last_state_change..')')
    ...            else
    ...                broker_log:info(0, 'last state change OK ('.. svc.last_state_change..'='.. e.last_state_change..')')
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
    
    ${content}    Create List    configuration of (1,1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Configuration
    ## Time to set the service to OK hard
    
    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK
    
    ## check that we check the correct service
    ${content}    Create List    Status of (1,1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Status
    
    ${grep_res}    Grep File    /tmp/test-LUA.log    last state change doesnt match
    Should Not Be Empty    ${grep_res}    last state change is nil
    Run Keyword If    "'last state change doesnt match' not in ${grep_res}"    Log To Console    The previous step FAILED!

    ${content}    Create List    last state change OK
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    20
    Should Be True    ${result}    ${grep_res}

    Ctn Stop Engine
    Ctn Kindly Stop Broker


LCDNU1
    [Documentation]    the lua cache does not update last_hard_state_change
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
    ...            broker_log:info(0, 'Status of ('.. e.host_id.. ','.. e.service_id.. '):'.. broker.json_encode(e))
    ...            local svc = broker_cache:get_service(e.host_id,e.service_id)
    ...            if svc.last_hard_state_change ~= e.last_hard_state_change then
    ...                broker_log:info(0, 'last hard state change doesnt match ('.. svc.last_hard_state_change..'~'.. e.last_hard_state_change..')')
    ...            else
    ...                broker_log:info(0, 'last hard state change match ('.. svc.last_hard_state_change..'='.. e.last_hard_state_change..')')
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
    
    ${content}    Create List    configuration of (1,1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Configuration
    ## Time to set the service to OK hard
    
    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK
    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK
    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK
    
    ## check that we check the correct service
    ${content}    Create List    Status of (1,1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Status
    
    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK
    
    ${grep_res}    Grep File    /tmp/test-LUA.log    last hard state change doesnt match
    Should Not Be Empty    ${grep_res}    last hard state is nil
    Run Keyword If    "'last hard state change doesnt match' not in ${grep_res}"    Log To Console    The previous step FAILED!

    ${content}    Create List    last hard state change match
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    20
    Should Be True    ${result}    ${grep_res}

    Ctn Stop Engine
    Ctn Kindly Stop Broker


LCDNU2
    [Documentation]    the lua cache does not update last_time_critical
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
    ...            broker_log:info(0, 'Status of ('.. e.host_id.. ','.. e.service_id.. '):'.. broker.json_encode(e))
    ...            local svc = broker_cache:get_service(e.host_id,e.service_id)
    ...            if svc.last_time_critical ~= e.last_time_critical then
    ...                broker_log:info(0, 'last time critical doesnt match ('.. svc.last_time_critical..'~'.. e.last_time_critical..')')
    ...            else
    ...                broker_log:info(0, 'last time critical match ('.. svc.last_time_critical..'='.. e.last_time_critical..')')
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
    
    ${content}    Create List    configuration of (1,1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Configuration
    
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL
    
    ${content}    Create List    Status of (1,1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Status
    ## Time to set the service to Critical hard
    
    Ctn Process Service Result Hard    host_1    service_1    ${2}    The service_1 is CRITICAL

    ${grep_res}    Grep File    /tmp/test-LUA.log    last time critical doesnt match
    Should Not Be Empty    ${grep_res}    last time critical is nil
    Run Keyword If    "'last time critical doesnt match' not in ${grep_res}"    Log To Console    The previous step FAILED!

    ${content}    Create List    last time critical match
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    20
    Should Be True    ${result}    ${grep_res}

    Ctn Stop Engine
    Ctn Kindly Stop Broker


LCDNU3
    [Documentation]    the lua cache does not update last_time_ok
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
    ...            broker_log:info(0, 'Status of ('.. e.host_id.. ','.. e.service_id.. '):'.. broker.json_encode(e))
    ...            local svc = broker_cache:get_service(e.host_id,e.service_id)
    ...            if svc.last_time_ok ~= e.last_time_ok then
    ...                broker_log:info(0, 'last time ok doesnt match ('.. svc.last_time_ok..'~'.. e.last_time_ok..')')
    ...            else
    ...                broker_log:info(0, 'last time ok match ('.. svc.last_time_ok..'='.. e.last_time_ok..')')
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
    
    ${content}    Create List    configuration of (1,1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Configuration
    
    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK
    
    ${content}    Create List    Status of (1,1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Status
    ## Time to set the service to OK hard
    
    Ctn Process Service Result Hard    host_1    service_1    ${0}    The service_1 is OK
    
    ${grep_res}    Grep File    /tmp/test-LUA.log    last time ok doesnt match
    Should Not Be Empty    ${grep_res}    last time ok is nil
    Run Keyword If    "'last time ok change doesnt match' not in ${grep_res}"    Log To Console    The previous step FAILED!
    
    ${content}    Create List    last time ok match
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    20
    Should Be True    ${result}    ${grep_res}

    Ctn Stop Engine
    Ctn Kindly Stop Broker

LCDNU4
    [Documentation]    the lua cache does not update last_time_unknown
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
    ...            broker_log:info(0, 'Status of ('.. e.host_id.. ','.. e.service_id.. '):'.. broker.json_encode(e))
    ...            local svc = broker_cache:get_service(e.host_id,e.service_id)
    ...            if svc.last_time_unknown ~= e.last_time_unknown then
    ...                broker_log:info(0, 'last time unknown doesnt match ('.. svc.last_time_unknown..'~'.. e.last_time_unknown..')')
    ...            else
    ...                broker_log:info(0, 'last time unknown match ('.. svc.last_time_unknown..'='.. e.last_time_unknown..')')
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
    
    ${content}    Create List    configuration of (1,1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Configuration
    
    Ctn Process Service Result Hard    host_1    service_1    ${3}    The service_1 is Unkown

    ${content}    Create List    Status of (1,1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Status
    ## Time to set the service to Unkown hard
    
    ${grep_res}    Grep File    /tmp/test-LUA.log    last time unknown doesnt match
    Should Not Be Empty    ${grep_res}    last time unknown is nil
    Run Keyword If    "'last time unknown doesnt match' not in ${grep_res}"    Log To Console    The previous step FAILED!
    
    ${content}    Create List    last time unknown match
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    20
    Should Be True    ${result}    ${grep_res}

    Ctn Stop Engine
    Ctn Kindly Stop Broker

LCDNU5
    [Documentation]    the lua cache does not update last_time_warning
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
    ...            broker_log:info(0, 'Status of ('.. e.host_id.. ','.. e.service_id.. '):'.. broker.json_encode(e))
    ...            local svc = broker_cache:get_service(e.host_id,e.service_id)
    ...            if svc.last_time_warning ~= e.last_time_warning then
    ...                broker_log:info(0, 'last time warning doesnt match ('.. svc.last_time_warning..'~'.. e.last_time_warning..')')
    ...            else
    ...                broker_log:info(0, 'last time warning match ('.. svc.last_time_warning..'='.. e.last_time_warning..')')
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
    
    ${content}    Create List    configuration of (1,1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Configuration
    
    Ctn Process Service Result Hard    host_1    service_1    ${1}    The service_1 is warning

    ${content}    Create List    Status of (1,1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Status
    ## Time to set the service to warning hard

    ${grep_res}    Grep File    /tmp/test-LUA.log    last time warning doesnt match
    Should Not Be Empty    ${grep_res}    last time warning is nil
    Run Keyword If    "'last time warning doesnt match' not in ${grep_res}"    Log To Console    The previous step FAILED!
    
    ${content}    Create List    last time warning match
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    20
    Should Be True    ${result}    ${grep_res}

    Ctn Stop Engine
    Ctn Kindly Stop Broker


LCDNUH
    [Documentation]    the lua cache does not update last_hard_state_change for host 
    [Tags]    broker    engine    host    lua    MON-24745
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
    ...        if e._type == 65566 then --Host id
    ...            broker_log:info(0, 'configuration of ('.. e.host_id.. ')')
    ...            local host = broker_cache:get_host(e.host_id)
    ...            broker_log:info(0, broker.json_encode(host))
    ...        end
    ...        if e._type == 65568 then --host status ID
    ...            broker_log:info(0, 'Status of ('.. e.host_id.. '):'.. broker.json_encode(e))
    ...            local host = broker_cache:get_host(e.host_id)
    ...            if host.last_hard_state_change ~= e.last_hard_state_change then
    ...                broker_log:info(0, 'last hard state change doesnt match ('.. host.last_hard_state_change..'~'.. e.last_hard_state_change..')')
    ...            else
    ...                broker_log:info(0, 'last hard state change match ('.. host.last_hard_state_change..'='.. e.last_hard_state_change..')')
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

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    
    ${content}    Create List    configuration of (1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Configuration
    ## Time to set the service to Down hard
    
    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    1    host_1 DOWN
    END
    
    ## check that we check the correct host
    ${content}    Create List    Status of (1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Status
    
    ${grep_res}    Grep File    /tmp/test-LUA.log    last hard state change doesnt match
    Run Keyword If    "'last hard state change doesnt match' not in ${grep_res}"    Log To Console    The previous step FAILED!

    ${content}    Create List    last hard state change match
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    20
    Should Be True    ${result}    ${grep_res}

    Ctn Stop Engine
    Ctn Kindly Stop Broker

LCDNUH1
    [Documentation]    the lua cache does not update last_time_down for host 
    [Tags]    broker    engine    host    lua    MON-24745
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
    ...        if e._type == 65566 then --Host id
    ...            broker_log:info(0, 'configuration of ('.. e.host_id.. ')')
    ...            local host = broker_cache:get_host(e.host_id)
    ...            broker_log:info(0, broker.json_encode(host))
    ...        end
    ...        if e._type == 65568 then --host status ID
    ...            broker_log:info(0, 'Status of ('.. e.host_id.. '):'.. broker.json_encode(e))
    ...            local host = broker_cache:get_host(e.host_id)
    ...            if host.last_time_down ~= e.last_time_down then
    ...                broker_log:info(0, 'last time down doesnt match ('.. host.last_time_down..'~'.. e.last_time_down..')')
    ...            else
    ...                broker_log:info(0, 'last time down match ('.. host.last_time_down..'='.. e.last_time_down..')')
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

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    
    ${content}    Create List    configuration of (1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Configuration
    ## Time to set the host to Down hard
    
    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    1    host_1 DOWN
    END
    
    ## check that we check the correct Host
    ${content}    Create List    Status of (1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Status
    
    ${grep_res}    Grep File    /tmp/test-LUA.log    last time down doesnt match
    Run Keyword If    "'last time down doesnt match' not in ${grep_res}"    Log To Console    The previous step FAILED!
 
    ${content}    Create List     last time down match
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    20
    Should Be True    ${result}    ${grep_res}

    Ctn Stop Engine
    Ctn Kindly Stop Broker

LCDNUH2
    [Documentation]    the lua cache does not update last_time_unreachable for host 
    [Tags]    broker    engine    host    lua    MON-24745
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
    ...        if e._type == 65566 then --Host id
    ...            broker_log:info(0, 'configuration of ('.. e.host_id.. ')')
    ...            local host = broker_cache:get_host(e.host_id)
    ...            broker_log:info(0, broker.json_encode(host))
    ...        end
    ...        if e._type == 65568 then --host status ID
    ...            broker_log:info(0, 'Status of ('.. e.host_id.. '):'.. broker.json_encode(e))
    ...            local host = broker_cache:get_host(e.host_id)
    ...            if host.last_time_unreachable ~= e.last_time_unreachable then
    ...                broker_log:info(0, 'last time unreachable doesnt match ('.. host.last_time_unreachable..'~'.. e.last_time_unreachable..')')
    ...            else
    ...                broker_log:info(0, 'last time unreachable match ('.. host.last_time_unreachable..'='.. e.last_time_unreachable..')')
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

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    
    ${content}    Create List    configuration of (1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Configuration
    ## Time to set the HOST to Ureachable hard
    
    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    2    host_1 unreachable
    END
    
    ## check that we check the correct HOST
    ${content}    Create List    Status of (1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Status
    
    ${grep_res}    Grep File    /tmp/test-LUA.log    last time unreachable doesnt match
    Run Keyword If    "'last time unreachable doesnt match' not in ${grep_res}"    Log To Console    The previous step FAILED!

    ${content}    Create List    last time unreachable match
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    20
    Should Be True    ${result}    ${grep_res}

    Ctn Stop Engine
    Ctn Kindly Stop Broker

LCDNUH3
    [Documentation]    the lua cache does not update last_time_up for host 
    [Tags]    broker    engine    host    lua    MON-24745
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
    ...        if e._type == 65566 then --Host id
    ...            broker_log:info(0, 'configuration of ('.. e.host_id.. ')')
    ...            local host = broker_cache:get_host(e.host_id)
    ...            broker_log:info(0, broker.json_encode(host))
    ...        end
    ...        if e._type == 65568 then --host status ID
    ...            broker_log:info(0, 'Status of ('.. e.host_id.. '):'.. broker.json_encode(e))
    ...            local host = broker_cache:get_host(e.host_id)
    ...            if host.last_time_up ~= e.last_time_up then
    ...                broker_log:info(0, 'last time up doesnt match ('.. host.last_time_up..'~'.. e.last_time_up..')')
    ...            else
    ...                broker_log:info(0, 'last time up match ('.. host.last_time_up..'='.. e.last_time_up..')')
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

    ${content}    Create List    INITIAL HOST STATE: host_1;
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.
    
    ${content}    Create List    configuration of (1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Configuration
    ## Time to set the host to UP hard
    
    FOR   ${i}    IN RANGE    ${3}
        Ctn Process Host Check Result    host_1    0    host_1 UP
    END
    
    ## check that we check the correct Host
    ${content}    Create List    Status of (1)
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    60
    Should Be True    ${result}    error Status
    
    ${grep_res}    Grep File    /tmp/test-LUA.log    last time up doesnt match
    Run Keyword If    "'last time up doesnt match' not in ${grep_res}"    Log To Console    The previous step FAILED!

    ${content}    Create List    last time up match
    ${result}    Ctn Find In Log with Timeout    /tmp/test-LUA.log    ${start}    ${content}    20
    Should Be True    ${result}    ${grep_res}

    Ctn Stop Engine
    Ctn Kindly Stop Broker


broker_api_version = 2

function init(conf)
  broker_log:set_parameters(3, "/tmp/lua-engine.log")
  broker_log:info(0, "Starting test-dump-groups.lua")
end

function write(e)
    local service_group_name = broker_cache:get_servicegroup_name(1)
    if service_group_name then
        broker_log:info(0, "service_group_name:" .. service_group_name)
    else
        broker_log:info(0, "no service_group_name 1")
    end
    local host_group_name = broker_cache:get_hostgroup_name(1)
    if host_group_name then
        broker_log:info(0, "host_group_name:" .. host_group_name)
    else
        broker_log:info(0, "no host_group_name 1")
    end
    return true
end



#!/usr/bin/lua
--------------------------------------------------------------------------------
-- Test Script for broker_cache - Stream Connector
--------------------------------------------------------------------------------

serv_set = {}

--------------------------------------------------------------------------------
-- display recursively datas of a table in a json format
--------------------------------------------------------------------------------
function dump(o, indent)
    indent = indent or 0

    if type(o) == 'table' then
        local s = '{'
        local sep = ' '
        for k, v in pairs(o) do
            local key_str = type(k) == 'number' and '"' .. k .. '"' or '"' .. tostring(k) .. '"'
            s = s .. sep .. key_str .. ' : '
            if type(v) == 'table' then
                s = s .. dump(v, indent + 1)
            else
                s = s .. '"' .. tostring(v) .. '"'
            end
            sep = ','
        end
        return s .. '}'
    else
        return '"' .. tostring(o) .. '"'
    end
end

--------------------------------------------------------------------------------
-- convert value to string
--------------------------------------------------------------------------------
function safe_tostring(value)
    if value == nil then
        return "nil"
    elseif type(value) == "table" then
        return dump(value)
    else
        return tostring(value)
    end
end

--------------------------------------------------------------------------------
-- get all infos of a service from cache
--------------------------------------------------------------------------------
function get_service_all_infos(host_id, service_id)
    if not broker_cache then
        broker_log:error(1, "broker_cache is not available!")
        return nil
    end

    local serv_info = {}

    serv_info.host_id = host_id
    serv_info.service_id = service_id

    local hostname = broker_cache:get_hostname(host_id)

    if type(hostname) == "table" then
        serv_info.host_name = hostname.name or hostname[1] or hostname.hostname or "unknown"
        serv_info.host_name_raw = hostname -- Garder la structure complète
    else
        serv_info.host_name = hostname
    end

    serv_info.service_name = broker_cache:get_service_description(host_id, service_id)

    -- service exist?
    if not serv_info.service_name or serv_info.service_name == "" then
        broker_log:warning(1, "Host ID " .. host_id .. "Service ID " .. service_id .. " not found in cache or has no name")
        return nil
    end

    -- Servicegroups
    serv_info.servgroups = {}
    local servgroups = broker_cache:get_servicegroups(host_id, service_id)

    if servgroups and type(servgroups) == "table" then
        for sg_id, sg_name in pairs(servgroups) do
            table.insert(serv_info.servgroups, {
                id = sg_id,
                name = sg_name
            })
        end
    end

    return serv_info
end


function get_servgroup_all_infos(group_id)
    if not broker_cache then
        broker_log:error(1, "broker_cache is not available!")
        return nil
    end

    local servgroup_info = {}
    servgroup_info.id = group_id
    local name = broker_cache:get_servicegroup_name(group_id)
    if name == nil then
        return nil
    end
    servgroup_info.name = name
    return servgroup_info
end

--------------------------------------------------------------------------------
-- Stream Connector interface
--------------------------------------------------------------------------------

function init(conf)
    broker_log:set_parameters(3, "/tmp/test-LUA.log")
    broker_log:info(1, "=================================================")
    broker_log:info(1, "Test Host Cache Stream Connector - Initialized")
    broker_log:info(1, "=================================================")
    broker_log:info(1, "Waiting for events...")
    broker_log:info(1, "")
end

function write(event)
    broker_log:info(3, "Event received - Category: " .. tostring(event.category) ..
        ", Element: " .. tostring(event.element) ..
        ", Host ID: " .. tostring(event.host_id))

    broker_log:info(1, "")

    -- Vérifier que broker_cache existe
    if not broker_cache then
        broker_log:error(1, "ERROR: broker_cache is nil!")
        broker_log:error(1, "Make sure NEB events are enabled in broker configuration")
        return true
    end

    broker_log:info(1, "broker_cache is available, calling get_serv_all_infos...")

    if event.element == 21 or event.element == 51 then
        local sg_1 = get_servgroup_all_infos(1)
        if (sg_1) then
            broker_log:info(1, "servgroup 1 : " .. dump(sg_1))
        end
        local sg_2 = get_servgroup_all_infos(2)
        if (sg_2) then
            broker_log:info(1, "servgroup 2 : " .. dump(sg_2))
        end
    end
    if event.service_id and event.host_id then
        serv_set[event.service_id] = event.host_id
    end
    for serv_id, host_id in pairs(serv_set) do
        local serv_info = get_service_all_infos(host_id, serv_id)

        if serv_info then
            broker_log:info(1, "serv " .. tostring(serv_info.service_id) .. " : " .. dump(serv_info))
        end
    end

    return true
end

-- we only want host and pb_host events
function filter(category, element)

    if category ~= 1 then
        return false
    end
    if element ~= 23 and element ~= 27 and element ~= 21 and element ~= 22 and element ~= 51 and element ~= 52 then
        return false
    end

    return true
end
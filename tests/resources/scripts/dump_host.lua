#!/usr/bin/lua
--------------------------------------------------------------------------------
-- Test Script for broker_cache - Stream Connector
--------------------------------------------------------------------------------

host_set = {}

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
-- get all infos of a host from cache
--------------------------------------------------------------------------------
function get_host_all_infos(host_id)
    if not broker_cache then
        broker_log:error(1, "broker_cache is not available!")
        return nil
    end

    local host_info = {}

    -- Informations de base
    host_info.host_id = host_id

    -- Récupérer le hostname (peut être une table ou une string)
    local hostname = broker_cache:get_hostname(host_id)
    broker_log:info(2, "DEBUG: get_hostname value: " .. safe_tostring(hostname))

    -- Si c'est une table, essayer d'extraire le nom
    if type(hostname) == "table" then
        host_info.name = hostname.name or hostname[1] or hostname.hostname or "unknown"
        host_info.name_raw = hostname -- Garder la structure complète
    else
        host_info.name = hostname
    end

    -- Notes
    local notes = broker_cache:get_notes(host_id)
    if type(notes) == "table" then
        host_info.notes = notes.notes or notes[1] or ""
        host_info.notes_raw = notes
    else
        host_info.notes = notes
    end

    -- Notes URL
    local notes_url = broker_cache:get_notes_url(host_id)
    if type(notes_url) == "table" then
        host_info.notes_url = notes_url.notes_url or notes_url[1] or ""
        host_info.notes_url_raw = notes_url
    else
        host_info.notes_url = notes_url
    end

    -- Action URL
    local action_url = broker_cache:get_action_url(host_id)
    if type(action_url) == "table" then
        host_info.action_url = action_url.action_url or action_url[1] or ""
        host_info.action_url_raw = action_url
    else
        host_info.action_url = action_url
    end

    -- host exist?
    if not host_info.name or host_info.name == "" then
        broker_log:warning(1, "Host ID " .. host_id .. " not found in cache or has no name")
        return nil
    end

    -- Hostgroups
    host_info.hostgroups = {}
    local hostgroups = broker_cache:get_hostgroups(host_id)

    if hostgroups and type(hostgroups) == "table" then
        for hg_id, hg_name in pairs(hostgroups) do
            table.insert(host_info.hostgroups, {
                id = hg_id,
                name = hg_name
            })
        end
    end

    return host_info
end


function get_hostgroup_all_infos(hostgroup_id)
    if not broker_cache then
        broker_log:error(1, "broker_cache is not available!")
        return nil
    end

    local hostgroup_info = {}
    hostgroup_info.id = hostgroup_id
    local hg_name = broker_cache:get_hostgroup_name(hostgroup_id)
    if hg_name == nil then
        return nil
    end
    hostgroup_info.name = hg_name
    local hg_alias = broker_cache:get_hostgroup_alias(hostgroup_id)
    hostgroup_info.alias = hg_alias
    return hostgroup_info
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

    broker_log:info(1, "broker_cache is available, calling get_host_all_infos...")

    if event.element == 10 or event.element == 49 then
        local hg_1 = get_hostgroup_all_infos(1)
        if (hg_1) then
            broker_log:info(1, "hostgroup 1 : " .. dump(hg_1))
        end
        local hg_2 = get_hostgroup_all_infos(2)
        if (hg_2) then
            broker_log:info(1, "hostgroup 2 : " .. dump(hg_2))
        end
    end
    if event.host_id then
        host_set[event.host_id] = 1
    end
    for host_id, v in pairs(host_set) do
        local host_info = get_host_all_infos(host_id)

        if host_info then
            broker_log:info(1, "host " .. tostring(host_info.host_id) .. " : " .. dump(host_info))
        end
    end

    return true
end

-- we only want host and pb_host events
function filter(category, element)

    if category ~= 1 then
        return false
    end
    if element ~= 12 and element ~= 30 and element ~= 10 and element ~= 11 and element ~= 49 and element ~= 50 then
        return false
    end

    return true
end
#!/usr/bin/lua
--------------------------------------------------------------------------------
-- Test Script for broker_cache - HUGE_CONF
-- Logs cache content for specific services using get_host and get_service.
-- Target services are passed via broker config param "target_services" as a
-- comma-separated list of "host_id:service_id" pairs.
--------------------------------------------------------------------------------

local LUA_LOG = "/tmp/test-huge-cache.log"
-- key: service_id, value: host_id
local TARGET_SERVICES = {}
local logged = {}

function init(conf)
    broker_log:set_parameters(3, LUA_LOG)
    broker_log:info(1, "HUGE_CONF cache test initialized")

    local target_str = conf["target_services"] or ""
    for pair in target_str:gmatch("([^,]+)") do
        local h, s = pair:match("(%d+):(%d+)")
        if h and s then
            TARGET_SERVICES[tonumber(s)] = tonumber(h)
        end
    end
end

function write(event)
    if not broker_cache then
        broker_log:error(1, "broker_cache is nil")
        return true
    end

    local sid = event.service_id
    if not TARGET_SERVICES[sid] or logged[sid] then
        return true
    end
    logged[sid] = true

    local hid = event.host_id

    local host = broker_cache:get_host(hid)
    if host then
        broker_log:info(1, "host " .. tostring(hid) .. " : " .. broker.json_encode(host))
    else
        broker_log:warning(1, "host " .. tostring(hid) .. " not found in cache")
    end

    local svc = broker_cache:get_service(hid, sid)
    if svc then
        broker_log:info(1, "serv " .. tostring(sid) .. " : " .. broker.json_encode(svc))
    else
        broker_log:warning(1, "serv " .. tostring(sid) .. " not found in cache")
    end

    return true
end

function filter(category, element)
    -- NEB category only
    if category ~= 1 then
        return false
    end
    if element == 29 then
        return true
    end
    return false
end

broker_api_version = 2

function init(conf)
  broker_log:set_parameters(3, "/tmp/lua.log")
  broker_log:info(0, "Starting test.lua")
end

function write(e)
  local js = broker.json_encode(e)
  js = js:gsub("(%d%.%d%d%d%d%d)%d*", "%1")
  local str = broker.md5(js)
  broker_log:info(0, tostring(e._type) .. " " .. str .. " " .. js)
  return true
end

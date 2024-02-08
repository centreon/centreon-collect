broker_api_version = 2

function init(conf)
  broker_log:set_parameters(3, "/tmp/lua.log")
  broker_log:info(0, "Starting test.lua")
end

function write(e)
  if e._type == 196621 then
    broker_log:info(0, broker.json_encode(e))
  end
  return true
end
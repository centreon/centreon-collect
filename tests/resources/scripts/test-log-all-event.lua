broker_api_version = 2

function init(conf)
  broker_log:set_parameters(3, "/tmp/all_lua_event.log")
end

function write(e)
  broker_log:info(0, broker.json_encode(e))
  return true
end

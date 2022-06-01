broker_api_version = 2

function init(conf)
  broker_log:set_parameters(3, "/tmp/pbservice.log")
end

function write(e)
  if e._type == 65563 then
    broker_log:info(0, broker.json_encode(e))
  end
  return true
end

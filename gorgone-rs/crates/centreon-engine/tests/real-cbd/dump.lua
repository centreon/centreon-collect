-- Writes every event cbd hands to this output as one JSON line, so that the
-- test can read what cbd decoded from the scheduler.
broker_api_version = 2

local file

function init(conf)
  file = io.open(conf.path, "a")
end

function write(event)
  file:write(broker.json_encode(event), "\n")
  file:flush()
  return true
end

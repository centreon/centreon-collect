-- Init function
function init(params)
    broker_log:set_parameters(1, "/tmp/test4.log")
end

-- Write function
function write(d)
    if d._type == 196620 then
        broker_log:info(0, "metric_name: " .. tostring(d.metric_name))
    elseif d._type == 196617 then
        local mapping = broker_cache:get_metric_mapping(d.metric_id)
        if mapping then
            broker_log:info(0, "metric id: " .. tostring(d.metric_id) .. " metric_name: " .. tostring(mapping.metric_name))
        end
    end
    return true
end
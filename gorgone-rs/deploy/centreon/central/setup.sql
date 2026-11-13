-- centreon-web sets a new platform up for a broker running on its own host.
-- In this bench the brokers are containers: send the flows to them.

-- Central broker -> RRD broker.
UPDATE cfg_centreonbroker_info i
JOIN cfg_centreonbroker b ON b.config_id = i.config_id
SET i.config_value = 'broker-rrd'
WHERE b.config_filename = 'central-broker.json'
  AND i.config_group = 'output' AND i.config_key = 'host'
  AND i.config_value = 'localhost';

-- Engine of the Central poller -> central broker.
UPDATE cfg_centreonbroker_info i
JOIN cfg_centreonbroker b ON b.config_id = i.config_id
SET i.config_value = 'broker'
WHERE b.config_filename = 'central-module.json'
  AND i.config_group = 'output' AND i.config_key = 'host';

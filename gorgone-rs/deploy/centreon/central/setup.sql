-- centreon-web sets a new platform up for a broker running on its own host.
-- In this bench the brokers are containers: send the flows to them.

-- Engine of the Central poller -> central broker.
UPDATE cfg_centreonbroker_info i
JOIN cfg_centreonbroker b ON b.config_id = i.config_id
SET i.config_value = 'broker'
WHERE b.config_filename = 'central-module.json'
  AND i.config_group = 'output' AND i.config_key = 'host';

-- Metrics go to VictoriaMetrics instead of RRD files: the central broker's
-- output to the RRD broker becomes a victoria_metrics output.
DELETE i FROM cfg_centreonbroker_info i
JOIN cfg_centreonbroker b ON b.config_id = i.config_id
WHERE b.config_filename = 'central-broker.json'
  AND i.config_group = 'output'
  AND i.config_group_id = (
    SELECT g.config_group_id FROM (
      SELECT x.config_group_id
      FROM cfg_centreonbroker_info x
      JOIN cfg_centreonbroker y ON y.config_id = x.config_id
      WHERE y.config_filename = 'central-broker.json'
        AND x.config_group = 'output' AND x.config_key = 'port'
        AND x.config_value = '5670'
    ) g
  );

INSERT INTO cfg_centreonbroker_info
  (config_id, config_key, config_value, config_group, config_group_id)
SELECT b.config_id, v.k, v.val, 'output', 9
FROM cfg_centreonbroker b
JOIN (
  SELECT 'name' AS k, 'central-broker-master-victoria-metrics' AS val
  UNION ALL SELECT 'type', 'victoria_metrics'
  UNION ALL SELECT 'db_host', 'victoria-metrics'
  UNION ALL SELECT 'db_port', '8428'
  -- The bench VictoriaMetrics has no authentication.
  UNION ALL SELECT 'db_user', ''
  UNION ALL SELECT 'db_password', ''
) v
WHERE b.config_filename = 'central-broker.json'
  AND NOT EXISTS (
    SELECT 1 FROM cfg_centreonbroker_info e
    WHERE e.config_id = b.config_id AND e.config_group = 'output'
      AND e.config_key = 'type' AND e.config_value = 'victoria_metrics'
  );

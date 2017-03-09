
-- TOPOLOGY
DELETE FROM topology WHERE topology_page = '677';
DELETE FROM topology WHERE topology_page = '67701';

-- Delete Output
DELETE cfg_centreonbroker_info FROM cfg_centreonbroker_info
JOIN (SELECT config_id, config_group_id
    FROM cfg_centreonbroker_info
    WHERE config_group = 'output'
    AND config_key = 'type'
    AND config_value = 'discovery'
) AS p
ON cfg_centreonbroker_info.config_id = p.config_id
AND cfg_centreonbroker_info.config_group_id = p.config_group_id;

-- Centreon Broker configuration objects.
DELETE FROM cb_module WHERE name='Automation';
DELETE FROM cb_field WHERE fieldname='cfg_file' AND fieldtype='text' LIMIT 1;

-- Delete nmap discovery command
DELETE FROM command WHERE command_name = 'centreon-discovery-nmap';

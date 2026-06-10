INSERT IGNORE INTO `nagios_server` (
       id, name, localhost, is_default, last_restart, ns_ip_address,
       ns_activate, ns_status,
       ssh_port,gorgone_communication_type,gorgone_port, uid,
       init_script_centreontrapd, snmp_trapd_path_conf, centreonbroker_cfg_path, engine_name, engine_version, centreonbroker_logs_path, remote_id, remote_server_use_as_proxy,updated)
  VALUES
  (
    3, 'remote', '0', 0, 1711560733, '127.0.0.1',
    '1', '0',
    22, '4', 443, 43994325, 'centreontrapd', '/etc/snmp/centreon_traps/', '/etc/centreon-broker',
    NULL, NULL, NULL, NULL, '1', '0'
  );

UPDATE `nagios_server` SET `remote_id` = 3 WHERE `id` = 2;
-- service discovery tables
DROP TABLE IF EXISTS `mod_auto_disco_macro`;
DROP TABLE IF EXISTS `mod_auto_disco_rule_service_relation`;
DROP TABLE IF EXISTS `mod_auto_disco_rule_contact_relation`;
DROP TABLE IF EXISTS `mod_auto_disco_ht_rule_relation`;
DROP TABLE IF EXISTS `mod_auto_disco_inclusion_exclusion`;
DROP TABLE IF EXISTS `mod_auto_disco_change`;
DROP TABLE IF EXISTS `mod_auto_disco_inst_rule_relation`;
DROP TABLE IF EXISTS `mod_auto_disco_rule`;

-- host discovery tables
DROP TABLE IF EXISTS `mod_host_disco_host_already_discovered`;
DROP TABLE IF EXISTS `mod_host_disco_host`;
DROP TABLE IF EXISTS `mod_host_disco_credential_parameter`;
DROP TABLE IF EXISTS `mod_host_disco_credential_field`;
DROP TABLE IF EXISTS `mod_host_disco_job_parameter`;
DROP TABLE IF EXISTS `mod_host_disco_mapper`;
DROP TABLE IF EXISTS `mod_host_disco_job`;
DROP TABLE IF EXISTS `mod_host_disco_credential`;
DROP TABLE IF EXISTS `mod_host_disco_provider_mapping`;
DROP TABLE IF EXISTS `mod_host_disco_provider`;
DROP TABLE IF EXISTS `mod_host_disco_provider_type`;
DROP TABLE IF EXISTS `mod_host_disco_provider_compatibility`;

DELETE FROM topology WHERE topology_page = '60210' AND topology_name = 'Scan';
DELETE FROM topology WHERE topology_page = '60214' AND topology_name = 'Overview';
DELETE FROM topology WHERE topology_page = '60215' AND topology_name = 'Rules';
DELETE FROM topology WHERE topology_parent = '602' AND topology_name = 'Auto Discovery';
DELETE FROM topology_JS WHERE id_page = '60215';
DELETE FROM topology_JS WHERE id_page = '60210';

-- Delete Host Discovery entry
DELETE FROM topology WHERE topology_page = 60130;

-- Old command
DELETE FROM command WHERE command_name IN ('autodisco_snmp_generic_file_system', 'autodisco_snmp_generic_interface', 'autodisco_snmp_generic_process', 'check_centreon_traffic_by_name', 'check_centreon_traffic_by_id', 'check_centreon_traffic_by_id',
'check_centreon_remote_storage_by_id', 'azure-host-discovery');

-- Old service
DELETE FROM service WHERE service_description IN ('Auto-Disco-SNMP-Generic-File-System-By-Name', 'Auto-Disco-SNMP-Generic-File-System-By-ID', 'Auto-Disco-SNMP-Generic-Interface-By-Name', 'Auto-Disco-SNMP-Generic-Interface-By-ID', 'Auto-Disco-SNMP-Generic-Processus');
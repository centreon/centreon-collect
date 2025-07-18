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


DELETE FROM topology;
DELETE FROM topology_JS;
DELETE FROM service;
DELETE FROM host;

DELETE FROM command;
DELETE FROM ns_host_relation;
DELETE FROM host_template_relation;


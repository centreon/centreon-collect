--old name
DELETE FROM `topology` WHERE `topology_parent` = 650 AND `topology_name` = 'Plugin packs';
DELETE FROM `topology` WHERE `topology_page` = 65001 AND `topology_name` = 'Setup';
DELETE FROM `topology` WHERE `topology_page` = 650 AND `topology_name` = 'Plugin packs';
DELETE FROM `topology` WHERE `topology_page` = 65099 AND `topology_name` = 'Plugin packs documentation';
DELETE FROM `topology` WHERE `topology_page` = 650 AND `topology_parent` = 6;
--new name
DELETE FROM `topology` WHERE `topology_parent` = 650 AND `topology_name` = 'Plugin pack';
DELETE FROM `topology` WHERE `topology_page` = 650 AND `topology_name` = 'Plugin pack';
DELETE FROM `topology` WHERE `topology_page` = 65099 AND `topology_name` = 'Plugin pack documentation';

DROP TABLE IF EXISTS `mod_ppm_information_url`;
DROP TABLE IF EXISTS `mod_ppm_discovery_validator_argument`;
DROP TABLE IF EXISTS `mod_ppm_discovery_validator`;
DROP TABLE IF EXISTS `mod_ppm_resultset_tpl`;
DROP TABLE IF EXISTS `mod_ppm_pluginpack_service`;
DROP TABLE IF EXISTS `mod_ppm_pluginpack_host`;
DROP TABLE IF EXISTS `mod_ppm_pluginpack_dependency`;
DROP TABLE IF EXISTS `mod_ppm_pluginpack_command`;
DROP TABLE IF EXISTS `mod_ppm_host_service_relation`;
DROP TABLE IF EXISTS `mod_ppm_pluginpack`;
DROP TABLE IF EXISTS `mod_ppm_icons`;
DROP TABLE IF EXISTS `mod_ppm_discovery_category`;
DROP TABLE IF EXISTS `mod_pp_commands`;
DROP TABLE IF EXISTS `mod_pp_service_templates`;
DROP TABLE IF EXISTS `mod_pp_host_templates`;
DROP TABLE IF EXISTS `mod_pluginpack`;
DROP TABLE IF EXISTS `mod_ppm_categories`;
DELETE FROM topology WHERE topology_page = '611';
DELETE FROM topology WHERE topology_page = '61101';

DROP TABLE IF EXISTS mod_export_pluginpack_host;
DROP TABLE IF EXISTS mod_ppm_pluginpack;
DROP TABLE IF EXISTS mod_export_discovery_category;
DROP TABLE IF EXISTS mod_export_command;
DROP TABLE IF EXISTS mod_export_serviceTpl;
DROP TABLE IF EXISTS mod_export_hostTpl;
DROP TABLE IF EXISTS mod_export_tags_relation;
DROP TABLE IF EXISTS mod_export_tags;
DROP TABLE IF EXISTS mod_export_requirements;
DROP TABLE IF EXISTS mod_export_dependencies;
DROP TABLE IF EXISTS mod_export_description;
DROP TABLE IF EXISTS mod_export_icons;
DROP TABLE IF EXISTS mod_export_links;
DROP TABLE IF EXISTS mod_export_pluginpack;
DROP TABLE IF EXISTS mod_export_packs;
DROP TABLE IF EXISTS mod_export_tpl_status;

-- QDE: We should purge DB here BUT
-- 1) mod_export_links was not deleted before
-- 2) given the state of our module + backups, I prefer not to remove data
-- DROP TABLE `centreon`.`mod_export_packs` ;
-- DROP TABLE `centreon`.`mod_export_links` ;

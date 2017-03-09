-- New configuration menu
INSERT INTO `topology` (`topology_name` ,`topology_parent` ,`topology_page` ,`topology_order` ,`topology_modules` ,`topology_show`)
VALUES ( 'Plugin pack', '6', 650, '70', '1', '1');

UPDATE `topology` SET
`topology_name` = 'Plugin pack',
`topology_parent` = 650,
`topology_page` = NULL,
`topology_order` = NULL,
`topology_group` = 1,
`topology_url` = NULL,
`topology_url_opt` = NULL,
`topology_popup` = '0',
`topology_modules` = '1',
`topology_show` = '1',
`topology_style_class` = NULL,
`topology_style_id` = NULL,
`topology_OnClick` = NULL,
`readonly` = '1'
WHERE `topology_parent` = 507
AND `topology_name` = 'Plugin packs';

UPDATE `topology` SET
`topology_parent` = 650,
`topology_page` = 65001,
`topology_order` = 1,
`topology_group` = 1,
`topology_url` = './modules/centreon-pp-manager/core/manager/main.php',
`topology_url_opt` = NULL,
`topology_popup` = '0',
`topology_modules` = '1',
`topology_show` = '1',
`readonly` = '0'
WHERE `topology_page` = 50704
AND `topology_name` = 'Setup';

UPDATE `topology` SET
`topology_parent` = 650,
`topology_page` = 65099,
`topology_order` = 99,
`topology_group` = 1
WHERE `topology_page` = 50799
AND `topology_name` = 'Plugin packs documentation';

ALTER TABLE `mod_ppm_pluginpack_dependency` DROP COLUMN pluginpack_dep_version;

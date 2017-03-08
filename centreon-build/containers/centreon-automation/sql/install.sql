
-- TOPOLOGY
INSERT INTO `topology` (`topology_id`, `topology_name`, `topology_parent`, `topology_page`, `topology_order`, `topology_group`, `topology_url`, `topology_url_opt`, `topology_popup`, `topology_modules`, `topology_show`) VALUES
('', 'Automation', '6', '677', '10', '1', './modules/centreon-automation/frontend/app/index.php', NULL, '0', '1', '1'),
('', 'Automation', '677', '67701', '10', '1', './modules/centreon-automation/frontend/app/index.php', NULL, NULL, '1', '1');

-- Insert centreon broker configuration objects
INSERT INTO `cb_module` (`name`, `libname`, `loading_pos`) VALUES ('Automation', 'discovery.so', 80);
INSERT INTO `cb_type` (`type_name`, `type_shortname`, `cb_module_id`)
 VALUES ('Automation', 'discovery', (SELECT `cb_module_id` FROM `cb_module` WHERE `libname` = 'discovery.so'));
INSERT INTO `cb_field` (`fieldname`, `displayname`, `description`, `fieldtype`)
 VALUES ('cfg_file', 'Configuration file', 'The discovery commands configuration file.', 'text');
INSERT INTO `cb_type_field_relation` (`cb_type_id`, `cb_field_id`, `is_required`)
  VALUES (
  (SELECT `cb_type_id` FROM `cb_type` WHERE `type_shortname` = 'discovery'),
  (SELECT `cb_field_id` FROM `cb_field` WHERE `description` = 'The discovery commands configuration file.'),
   1
   );
INSERT INTO `cb_tag_type_relation` (`cb_tag_id`, `cb_type_id`, `cb_type_uniq`)
 VALUES (1, (SELECT `cb_type_id` FROM `cb_type` WHERE `type_shortname` = 'discovery'), 1);

-- Insert nmap discovery command
INSERT INTO `command` (`command_name`, `command_line`, `command_type`, `command_activate`, `command_locked`)
VALUES ('centreon-discovery-nmap', '$CENTREONPLUGINS$/centreon_discovery_nmap.pl --plugin=discovery::nmap::plugin --mode=fastscan --range="$NETWORK$"', 4, '1', 1);
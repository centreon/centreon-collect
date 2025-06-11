 CREATE TABLE IF NOT EXISTS `mod_auto_disco_rule` (
  `rule_id` int(11) NOT NULL AUTO_INCREMENT,
  `rule_alias` varchar(255) DEFAULT NULL,
  `service_display_name` varchar(255) DEFAULT NULL,
  `rule_activate` enum('0','1') NOT NULL DEFAULT '1',
  `rule_disable` enum('0','1') NOT NULL DEFAULT '0',
  `rule_update` enum('0','1') NOT NULL DEFAULT '0',
  `rule_comment` text DEFAULT NULL,
  `rule_scan_display_custom` text DEFAULT NULL,
  `rule_variable_custom` text DEFAULT NULL,
  `command_command_id` int(11) DEFAULT NULL,
  `command_command_id2` int(11) DEFAULT NULL,
  `service_template_model_id` int(11) DEFAULT NULL,
  PRIMARY KEY (`rule_id`),
  KEY `command_command_id` (`command_command_id`),
  KEY `command_command_id2` (`command_command_id2`),
  KEY `service_template_model_id` (`service_template_model_id`),
  CONSTRAINT `mod_auto_disco_rule_fk_1` FOREIGN KEY (`command_command_id`) REFERENCES `command` (`command_id`) ON DELETE SET NULL,
  CONSTRAINT `mod_auto_disco_rule_fk_2` FOREIGN KEY (`service_template_model_id`) REFERENCES `service` (`service_id`) ON DELETE SET NULL,
  CONSTRAINT `mod_auto_disco_rule_fk_3` FOREIGN KEY (`command_command_id2`) REFERENCES `command` (`command_id`) ON DELETE SET NULL
) ENGINE=InnoDB AUTO_INCREMENT=72 DEFAULT CHARSET=utf8 COLLATE=utf8_general_ci;

-- instead of creating a real command using a real plugin, we just echo the output expected to mock the plugin.
-- This doesn't really test if arguments are correctly passed to the plugin, but it is enough to test the auto-discovery feature.
-- Unit tests will be used to test the command execution and arguments passing.
INSERT IGNORE INTO command (
  command_id, connector_id, command_name, command_line, command_example, command_type, enable_shell, command_comment, command_activate, graph_id, cmd_cat_id, command_locked
) VALUES (
  1, NULL, 'OS-Linux-tests-Discover',
  'echo ''<?xml version="1.0" encoding="utf-8"?>
<data>
  <label name="/run" storageid="35" total="304492544"/>
  <label total="20956397568" storageid="36" name="/"/>
  <label name="/dev/shm" storageid="38" total="1522458624"/>
  <label total="5242880" storageid="39" name="/run/lock"/>
  <label name="/run/user/1001" storageid="50" total="304488448"/>
</data>''',
  NULL, 4, 0, NULL, 1, NULL, NULL, 1
),
(
  2, NULL, 'OS-Linux-tests-Macro',
  'echo ''<?xml version="1.0" encoding="utf-8"?>
<data>
  <element>name</element>
  <element>total</element>
  <element>storageid</element>
</data>''',
  NULL, 4, 0, NULL, 1, NULL, NULL, 1
);
INSERT IGNORE INTO service (
  service_id, service_template_model_stm_id, service_description, service_alias, service_is_volatile,
  service_max_check_attempts, service_normal_check_interval, service_retry_check_interval,
  service_active_checks_enabled, service_passive_checks_enabled, service_parallelize_check,
  service_obsess_over_service, service_check_freshness, service_event_handler_enabled,
  service_flap_detection_enabled, service_process_perf_data, service_retain_status_information,
  service_retain_nonstatus_information, service_notifications_enabled, contact_additive_inheritance,
  cg_additive_inheritance, service_inherit_contacts_from_host, service_locked, service_register, service_activate
) VALUES
(
  40, 39, 'OS-Linux-Disk-Global-SNMP-custom', 'Disk-Global', 2,
  NULL, NULL, NULL,
  2, 2, 2,
  2, 2, 2,
  2, 2, 2,
  2, 2, 0,
  0, 1, 0, 0, 1
),
(
  39, 2, 'OS-Linux-Disk-Global-SNMP', 'Disk-Global', 2,
  3, 30, 1,
  2, 2, 2,
  2, 2, 2,
  2, 2, 2,
  2, 2, 0,
  0, 1, 1, 0, 1
),
(
  2, 1, 'generic-active-service-custom', 'generic-active-service', 2,
  NULL, NULL, NULL,
  2, 2, 2,
  2, 2, 2,
  2, 2, 2,
  2, 2, 0,
  0, 1, 0, 0, 1
),
(
  1, NULL, 'generic-active-service', 'generic-active-service', 0,
  3, 5, 1,
  1, 0, 2,
  2, 2, 2,
  2, 2, 2,
  2, 2, 0,
  0, 1, 1, 0, 1
);

INSERT IGNORE INTO mod_auto_disco_rule (
  rule_id, rule_alias, service_display_name, rule_activate, rule_disable, rule_update, rule_comment, rule_scan_display_custom, rule_variable_custom, command_command_id, command_command_id2, service_template_model_id
) VALUES (
  2, 'OS-Linux-SNMP-Disk-Name', 'Disk-$name$', '1', '0', '0', '', '', '', 1, 2, 40
);

 CREATE TABLE IF NOT EXISTS `mod_auto_disco_change` (
  `change_id` int(11) NOT NULL AUTO_INCREMENT,
  `rule_id` int(11) NOT NULL,
  `change_str` varchar(521) NOT NULL,
  `change_regexp` varchar(512) DEFAULT NULL,
  `change_replace` varchar(512) DEFAULT NULL,
  `change_modifier` varchar(512) DEFAULT NULL,
  `change_order` int(11) NOT NULL,
  PRIMARY KEY (`change_id`),
  KEY `mod_auto_disco_change_fk_1` (`rule_id`),
  CONSTRAINT `mod_auto_disco_change_fk_1` FOREIGN KEY (`rule_id`) REFERENCES `mod_auto_disco_rule` (`rule_id`) ON DELETE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=3 DEFAULT CHARSET=utf8 COLLATE=utf8_general_ci;

 CREATE TABLE IF NOT EXISTS `mod_auto_disco_inclusion_exclusion` (
  `exinc_id` int(11) NOT NULL AUTO_INCREMENT,
  `exinc_type` enum('0','1') DEFAULT '0',
  `exinc_str` varchar(521) NOT NULL,
  `exinc_regexp` varchar(512) DEFAULT NULL,
  `rule_id` int(11) NOT NULL,
  `exinc_order` int(11) NOT NULL,
  PRIMARY KEY (`exinc_id`),
  KEY `mod_auto_disco_inclusion_exclusion_fk_1` (`rule_id`),
  CONSTRAINT `mod_auto_disco_inclusion_exclusion_fk_1` FOREIGN KEY (`rule_id`) REFERENCES `mod_auto_disco_rule` (`rule_id`) ON DELETE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=51 DEFAULT CHARSET=utf8 COLLATE=utf8_general_ci;

 CREATE TABLE IF NOT EXISTS `mod_auto_disco_macro` (
  `macro_id` int(11) NOT NULL AUTO_INCREMENT,
  `macro_name` varchar(255) NOT NULL,
  `macro_value` varchar(255) NOT NULL,
  `rule_id` int(11) NOT NULL,
  `is_empty` tinyint(2) DEFAULT 0,
  PRIMARY KEY (`macro_id`),
  KEY `mod_auto_disco_macro_fk_2` (`rule_id`),
  CONSTRAINT `mod_auto_disco_macro_fk_2` FOREIGN KEY (`rule_id`) REFERENCES `mod_auto_disco_rule` (`rule_id`) ON DELETE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=98 DEFAULT CHARSET=utf8 COLLATE=utf8_general_ci;

 CREATE TABLE IF NOT EXISTS `mod_auto_disco_inst_rule_relation` (
  `hrr_id` int(11) NOT NULL AUTO_INCREMENT,
  `instance_id` int(11) NOT NULL,
  `rule_rule_id` int(11) NOT NULL,
  PRIMARY KEY (`hrr_id`),
  KEY `instance_id` (`instance_id`),
  KEY `rule_rule_id` (`rule_rule_id`),
  CONSTRAINT `mod_auto_disco_inst_rule_relation_fk_1` FOREIGN KEY (`instance_id`) REFERENCES `nagios_server` (`id`) ON DELETE CASCADE,
  CONSTRAINT `mod_auto_disco_inst_rule_relation_fk_2` FOREIGN KEY (`rule_rule_id`) REFERENCES `mod_auto_disco_rule` (`rule_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_general_ci;

 CREATE TABLE IF NOT EXISTS `mod_auto_disco_ht_rule_relation` (
  `hrr_id` int(11) NOT NULL AUTO_INCREMENT,
  `host_host_id` int(11) NOT NULL,
  `rule_rule_id` int(11) NOT NULL,
  PRIMARY KEY (`hrr_id`),
  KEY `host_host_id` (`host_host_id`),
  KEY `rule_rule_id` (`rule_rule_id`),
  CONSTRAINT `mod_auto_disco_ht_rule_relation_fk_1` FOREIGN KEY (`host_host_id`) REFERENCES `host` (`host_id`) ON DELETE CASCADE,
  CONSTRAINT `mod_auto_disco_ht_rule_relation_fk_2` FOREIGN KEY (`rule_rule_id`) REFERENCES `mod_auto_disco_rule` (`rule_id`) ON DELETE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=73 DEFAULT CHARSET=utf8 COLLATE=utf8_general_ci;

 CREATE TABLE IF NOT EXISTS `mod_auto_disco_rule_service_relation` (
  `rsr_id` int(11) NOT NULL AUTO_INCREMENT,
  `service_service_id` int(11) NOT NULL,
  `rule_rule_id` int(11) NOT NULL,
  PRIMARY KEY (`rsr_id`),
  UNIQUE KEY `uniq_service_rule` (`service_service_id`,`rule_rule_id`),
  KEY `service_service_id` (`service_service_id`),
  KEY `rule_rule_id` (`rule_rule_id`),
  CONSTRAINT `mod_auto_disco_rule_service_relation_fk_1` FOREIGN KEY (`service_service_id`) REFERENCES `service` (`service_id`) ON DELETE CASCADE,
  CONSTRAINT `mod_auto_disco_rule_service_relation_fk_2` FOREIGN KEY (`rule_rule_id`) REFERENCES `mod_auto_disco_rule` (`rule_id`) ON DELETE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=25 DEFAULT CHARSET=utf8 COLLATE=utf8_general_ci;

CREATE TABLE IF NOT EXISTS `mod_auto_disco_rule_contact_relation` (
	`rcr_id` int(11) NOT NULL AUTO_INCREMENT,
	`rule_id` int(11) NOT NULL,
	`contact_id` int(11) DEFAULT NULL,
	`cg_id` int(11) DEFAULT NULL,
	PRIMARY KEY (`rcr_id`),
	KEY `rule_id` (`rule_id`),
	KEY `contact_id` (`contact_id`),
	KEY `cg_id` (`cg_id`),
    CONSTRAINT `mod_auto_disco_rule_contact_relation_fk_1` FOREIGN KEY (`rule_id`) REFERENCES `mod_auto_disco_rule` (`rule_id`) ON DELETE CASCADE,
    CONSTRAINT `mod_auto_disco_rule_contact_relation_fk_2` FOREIGN KEY (`contact_id`) REFERENCES `contact` (`contact_id`) ON DELETE SET NULL,
    CONSTRAINT `mod_auto_disco_rule_contact_relation_fk_3` FOREIGN KEY (`cg_id`) REFERENCES `contactgroup` (`cg_id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;



INSERT IGNORE INTO host (
  host_id, host_name, host_alias,
  host_active_checks_enabled, host_passive_checks_enabled, host_checks_enabled,
  host_obsess_over_host, host_check_freshness, host_event_handler_enabled,
  host_flap_detection_enabled, host_process_perf_data,
  host_retain_status_information, host_retain_nonstatus_information,
  host_notifications_enabled, contact_additive_inheritance, cg_additive_inheritance,
  host_locked, host_register, host_activate
) VALUES (
  10, 'OS-Linux-SNMP-custom', 'Template to check Linux server using SNMP protocol',
  2, 2, 2,
  2, 2, 2,
  2, 2,
  2, 2,
  2, 0, 0,
  0, '0', 1
);

INSERT IGNORE INTO host (host_id, host_name, host_address, host_activate)
VALUES (81, 'localhost', '127.0.0.1', '1');

INSERT IGNORE INTO host_template_relation (host_host_id, host_tpl_id, `order`)
VALUES (81, 10, 1);

INSERT IGNORE INTO ns_host_relation (host_host_id, nagios_server_id)
VALUES (81, 1);

INSERT IGNORE INTO mod_auto_disco_ht_rule_relation (hrr_id, host_host_id, rule_rule_id) VALUES
(2, 10, 2);

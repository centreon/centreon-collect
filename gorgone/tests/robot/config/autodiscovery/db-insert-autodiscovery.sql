
INSERT INTO `command` VALUES
(1,NULL,'OS-Linux-tests-Discover','echo ''<?xml version=\"1.0\" encoding=\"utf-8\"?> <data> <label name=\"/var/lib/docker\" storageid=\"35\" total=\"304492544\"/> <label total=\"20956397568\" storageid=\"36\" name=\"/\"/> <label name=\"/var/lib\" storageid=\"38\" total=\"1522458624\"/> <label total=\"5242880\" storageid=\"39\" name=\"/home\"/> <label name=\"/run/user/1001\" storageid=\"50\" total=\"304488448\"/> </data>''',NULL,4,0,NULL,'0',NULL,NULL,1),
(2,NULL,'OS-Linux-tests-Macro','echo ''<?xml version=\"1.0\" encoding=\"utf-8\"?> <data> <element>name</element> <element>total</element> <element>storageid</element> </data>''',NULL,4,0,NULL,'0',NULL,NULL,1),
(3,NULL,'OS-Linux-injection-Discover','echo "toto" ;touch /tmp/robotInjectionAutodiscoverychecker',NULL,4,0,NULL,'0',NULL,NULL,1);

INSERT IGNORE INTO `service` VALUES
(1,NULL,NULL,NULL,NULL,NULL,'generic-active-service','generic-active-service',NULL,'0',3,5,1,'0','0',NULL,'1','1','1',NULL,'1',NULL,NULL,'1','1','1','1',NULL,NULL,NULL,'1',0,0,'0','0',NULL,NULL,NULL,NULL,NULL,NULL,NULL,1,'0','0'),
(2,1,NULL,NULL,NULL,NULL,'generic-active-service-custom','generic-active-service',NULL,'1',NULL,NULL,NULL,'1','1',NULL,'1','1','1',NULL,'1',NULL,NULL,'1','1','1','1',NULL,NULL,NULL,'1',0,0,'0','0',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,'0','0'),
(39,2,NULL,NULL,NULL,NULL,'OS-Linux-Disk-Global-SNMP','Disk-Global',NULL,'1',3,30,1,'1','1',NULL,'1','1','1',NULL,'1',NULL,NULL,'1','1','1','1',NULL,NULL,NULL,'1',0,0,'0','0',NULL,NULL,NULL,NULL,NULL,NULL,NULL,1,'0','0'),
(40,39,NULL,NULL,NULL,NULL,'OS-Linux-Disk-Global-SNMP-custom','Disk-Global',NULL,'1',NULL,NULL,NULL,'1','1',NULL,'1','1','1',NULL,'1',NULL,NULL,'1','1','1','1',NULL,NULL,NULL,'1',0,0,'0','0',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,'0','0');

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


INSERT INTO host (host_id, host_name, host_address, host_activate)
VALUES (81, 'localhost', '127.0.0.1', '1');

INSERT INTO host_template_relation (host_host_id, host_tpl_id, `order`)
VALUES (81, 10, 1);

INSERT INTO ns_host_relation (host_host_id, nagios_server_id)
VALUES (81, 1);


CREATE TABLE IF NOT EXISTS `mod_auto_disco_rule` (
	`rule_id` int(11) NOT NULL AUTO_INCREMENT,
	`rule_alias` varchar(255) DEFAULT NULL,
	`service_display_name` varchar(255) DEFAULT NULL,
	`rule_activate` enum('0','1') NOT NULL DEFAULT '1',
	`rule_disable` enum('0','1') NOT NULL DEFAULT '0',
	`rule_update` enum('0','1') NOT NULL DEFAULT '0',
	`rule_comment` text,
    `rule_scan_display_custom` text,
    `rule_variable_custom` text,
	`command_command_id` int(11) DEFAULT NULL,
	`command_command_id2` int(11) DEFAULT NULL,
	`service_template_model_id` int(11) DEFAULT NULL,

	PRIMARY KEY (`rule_id`),
	KEY `command_command_id` (`command_command_id`),
	KEY `command_command_id2` (`command_command_id2`),
	KEY `service_template_model_id` (`service_template_model_id`)
) ENGINE=InnoDB  DEFAULT CHARSET=utf8;

ALTER TABLE `mod_auto_disco_rule`
  ADD CONSTRAINT `mod_auto_disco_rule_fk_1` FOREIGN KEY (`command_command_id`) REFERENCES `command` (`command_id`) ON DELETE SET NULL,
  ADD CONSTRAINT `mod_auto_disco_rule_fk_2` FOREIGN KEY (`service_template_model_id`) REFERENCES `service` (`service_id`) ON DELETE SET NULL,
  ADD CONSTRAINT `mod_auto_disco_rule_fk_3` FOREIGN KEY (`command_command_id2`) REFERENCES `command` (`command_id`) ON DELETE SET NULL;


INSERT INTO `mod_auto_disco_rule` VALUES (2,'OS-Linux-SNMP-Disk-Name','Disk-$name$','1','0','0','','','',1,2,40),
                                         (3,'OS-Linux-SNMP-injection','Disk-$name$','0','0','0','','','',3,2,40);


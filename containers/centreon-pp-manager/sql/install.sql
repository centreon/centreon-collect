INSERT INTO `topology` (`topology_name`, `topology_parent`, `topology_page`, `topology_order`, `topology_group`, `topology_modules`, `topology_show`)
VALUES ( 'Plugin pack', '6', '650', '70', 1, '1', '1');
INSERT INTO `topology` (`topology_name`, `topology_parent`, `topology_page`, `topology_order`, `topology_group`, `topology_url`, `topology_url_opt`, `topology_popup`, `topology_modules`, `topology_show`, `topology_style_class`, `topology_style_id`, `topology_OnClick`, `readonly`)
VALUES ('Plugin pack',650,NULL,NULL,1,NULL,NULL,'0','1','1',NULL,NULL,NULL,'1'),
       ('Setup',650,65001,1,1,'./modules/centreon-pp-manager/core/manager/main.php',NULL,'0','1','1',NULL,NULL,NULL,'0'),
       ('Plugin pack documentation',650,65099,99,1,'./modules/centreon-pp-manager/core/manager/displayDocumentation.php',NULL,NULL,NULL,'0',NULL,NULL,NULL,'1');


CREATE TABLE IF NOT EXISTS `mod_ppm_discovery_category` (
  `discovery_category_id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `name` VARCHAR(100) NOT NULL,
  `slug` VARCHAR(100) NOT NULL,
  PRIMARY KEY (`discovery_category_id`),
  UNIQUE INDEX `name_UNIQUE` (`name` ASC),
  UNIQUE INDEX `slug_UNIQUE` (`slug` ASC))
ENGINE=InnoDB DEFAULT CHARSET=utf8;


INSERT INTO `mod_ppm_discovery_category` ( `slug`, `name`) VALUES
  ('applications', 'Applications'),
  ('centreon', 'Centreon'),
  ('cloud', 'Cloud'),
  ('database', 'Database'),
  ('hardware-server', 'Hardware - Server'),
  ('network', 'Network'),
  ('operating-system', 'Operating System'),
  ('printer', 'Printer'),
  ('protocol', 'Protocol'),
  ('sensor', 'Sensor'),
  ('storage', 'Storage'),
  ('toip-voip', 'ToIP-VoIP'),
  ('ups-pdu', 'UPS-PDU'),
  ('virtualization', 'Virtualization');

CREATE TABLE IF NOT EXISTS `mod_ppm_icons` (
  `icon_id` int(11) NOT NULL AUTO_INCREMENT,
  `icon_file` BLOB DEFAULT NULL,
  PRIMARY KEY (`icon_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE IF NOT EXISTS `mod_ppm_pluginpack` (
  `pluginpack_id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `name` VARCHAR(255) NOT NULL,
  `slug` VARCHAR(255) NOT NULL,
  `version` VARCHAR(50) NOT NULL,
  `status` VARCHAR(20) NOT NULL,
  `status_message` VARCHAR(255) NULL,
  `discovery_category` INT UNSIGNED NULL,
  `changelog` TEXT NULL,
  `monitoring_procedure` TEXT NULL,
  `icon` int(11) DEFAULT NULL,
  `complete` BOOL DEFAULT true,
  PRIMARY KEY (`pluginpack_id`),
  UNIQUE INDEX `slug_UNIQUE` (`slug` ASC),
  INDEX `mod_ppm_pluginpack_fk01_idx` (`discovery_category` ASC),
  CONSTRAINT `mod_ppm_pluginpack_fk01`
    FOREIGN KEY (`discovery_category`)
    REFERENCES `mod_ppm_discovery_category` (`discovery_category_id`)
    ON DELETE SET NULL
    ON UPDATE NO ACTION,
  CONSTRAINT `mod_ppm_pluginpack_fk02` FOREIGN KEY (`icon`) REFERENCES `mod_ppm_icons` (`icon_id`) ON DELETE CASCADE
)
ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `mod_ppm_pluginpack_command` (
  `pluginpack_id` INT UNSIGNED NOT NULL,
  `command_id` INT NOT NULL,
  PRIMARY KEY (`pluginpack_id`, `command_id`),
  INDEX `mod_ppm_pluginpack_command_fk02_idx` (`command_id` ASC),
  CONSTRAINT `mod_ppm_pluginpack_command_fk01`
    FOREIGN KEY (`pluginpack_id`)
    REFERENCES `mod_ppm_pluginpack` (`pluginpack_id`)
    ON DELETE CASCADE
    ON UPDATE NO ACTION,
  CONSTRAINT `mod_ppm_pluginpack_command_fk02`
    FOREIGN KEY (`command_id`)
    REFERENCES `command` (`command_id`)
    ON DELETE CASCADE
    ON UPDATE NO ACTION)
ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `mod_ppm_pluginpack_dependency` (
  `pluginpack_id` INT UNSIGNED NOT NULL,
  `pluginpack_dep_id` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`pluginpack_id`, `pluginpack_dep_id`),
  INDEX `mod_ppm_pluginpack_dependency_fk02_idx` (`pluginpack_dep_id` ASC),
  CONSTRAINT `mod_ppm_pluginpack_dependency_fk01`
    FOREIGN KEY (`pluginpack_id`)
    REFERENCES `mod_ppm_pluginpack` (`pluginpack_id`)
    ON DELETE CASCADE
    ON UPDATE NO ACTION,
  CONSTRAINT `mod_ppm_pluginpack_dependency_fk02`
    FOREIGN KEY (`pluginpack_dep_id`)
    REFERENCES `mod_ppm_pluginpack` (`pluginpack_id`)
    ON DELETE CASCADE
    ON UPDATE NO ACTION)
ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE IF NOT EXISTS `mod_ppm_pluginpack_host` (
  `pluginpack_id` INT UNSIGNED NOT NULL,
  `host_id` INT NOT NULL,
  PRIMARY KEY (`pluginpack_id`, `host_id`),
  INDEX `mod_ppm_pluginpack_host_fk02_idx` (`host_id` ASC),
  CONSTRAINT `mod_ppm_pluginpack_host_fk01`
    FOREIGN KEY (`pluginpack_id`)
    REFERENCES `mod_ppm_pluginpack` (`pluginpack_id`)
    ON DELETE CASCADE
    ON UPDATE NO ACTION,
  CONSTRAINT `mod_ppm_pluginpack_host_fk02`
    FOREIGN KEY (`host_id`)
    REFERENCES `host` (`host_id`)
    ON DELETE CASCADE
    ON UPDATE NO ACTION)
ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE IF NOT EXISTS `mod_ppm_pluginpack_service` (
  `pluginpack_id` INT UNSIGNED NOT NULL,
  `svc_id` int(11) NOT NULL,
  `discovery_command_options` VARCHAR(100) NULL,
  `monitoring_protocol` VARCHAR(100) NULL,
  PRIMARY KEY (`pluginpack_id`, `svc_id`),
  INDEX `mod_ppm_pluginpack_service_fk02_idx` (`svc_id` ASC),
  CONSTRAINT `mod_ppm_pluginpack_service_fk01`
    FOREIGN KEY (`pluginpack_id`)
    REFERENCES `mod_ppm_pluginpack` (`pluginpack_id`)
    ON DELETE CASCADE
    ON UPDATE NO ACTION,
  CONSTRAINT `mod_ppm_pluginpack_service_fk02`
    FOREIGN KEY (`svc_id`)
    REFERENCES `service` (`service_id`)
    ON DELETE CASCADE
    ON UPDATE NO ACTION)
ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE IF NOT EXISTS `mod_ppm_resultset_tpl` (
  `pluginpack_id` INT UNSIGNED NOT NULL,
  `command_id` INT NOT NULL,
  `resultset_tpl` TEXT NOT NULL,
  PRIMARY KEY (`pluginpack_id`, `command_id`),
  CONSTRAINT `mod_ppm_resultset_tpl_fk01`
    FOREIGN KEY (`pluginpack_id` , `command_id`)
    REFERENCES `mod_ppm_pluginpack_command` (`pluginpack_id` , `command_id`)
    ON DELETE CASCADE
    ON UPDATE NO ACTION)
ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE IF NOT EXISTS `mod_ppm_discovery_validator` (
  `pluginpack_id` INT UNSIGNED NOT NULL,
  `host_id` INT NOT NULL,
  `command_id` INT NOT NULL,
  PRIMARY KEY (`pluginpack_id`, `host_id`, `command_id`),
  INDEX `mod_ppm_discovery_validators_fk02_idx` (`command_id` ASC),
  CONSTRAINT `mod_ppm_discovery_validators_fk01`
    FOREIGN KEY (`pluginpack_id` , `host_id`)
    REFERENCES `mod_ppm_pluginpack_host` (`pluginpack_id` , `host_id`)
    ON DELETE CASCADE
    ON UPDATE NO ACTION,
  CONSTRAINT `mod_ppm_discovery_validators_fk02`
    FOREIGN KEY (`command_id`)
    REFERENCES `command` (`command_id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE IF NOT EXISTS `mod_ppm_discovery_validator_argument` (
  `pluginpack_id` INT UNSIGNED NOT NULL,
  `host_id` INT NOT NULL,
  `command_id` INT NOT NULL,
  `argname` VARCHAR(50) NOT NULL,
  `argvalue` VARCHAR(100) NOT NULL,
  PRIMARY KEY (`pluginpack_id`, `host_id`, `command_id`, `argname`),
CONSTRAINT `mod_ppm_discovery_validator_argument_fk01`
    FOREIGN KEY (`pluginpack_id` , `host_id` , `command_id`)
    REFERENCES `mod_ppm_discovery_validator` (`pluginpack_id` , `host_id` , `command_id`)
    ON DELETE CASCADE
    ON UPDATE NO ACTION
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE IF NOT EXISTS `mod_ppm_information_url` (
  `svc_id` int(11) NOT NULL,
  `url` VARCHAR(255) NOT NULL,
  PRIMARY KEY (`svc_id`),
  CONSTRAINT `mod_ppm_information_url_fk01`
    FOREIGN KEY (`svc_id`)
    REFERENCES `service` (`service_id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `mod_ppm_host_service_relation` (
  `pluginpack_id` int(10) UNSIGNED NOT NULL,
  `host_id` int(11) NOT NULL,
  `service_id` int(11) NOT NULL,
  PRIMARY KEY (`pluginpack_id`,`host_id`,`service_id`),
  CONSTRAINT `mod_ppm_host_service_relation_fk01`
    FOREIGN KEY (`pluginpack_id`)
    REFERENCES `mod_ppm_pluginpack` (`pluginpack_id`)
    ON DELETE CASCADE ,
  CONSTRAINT `mod_ppm_host_service_relation_fk02`
    FOREIGN KEY (`host_id`)
    REFERENCES `host` (`host_id`)
    ON DELETE CASCADE ,
  CONSTRAINT `mod_ppm_host_service_relation_fk03`
    FOREIGN KEY (`service_id`)
    REFERENCES `service` (`service_id`)
    ON DELETE CASCADE )
ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT INTO `nagios_macro` (`macro_name`) VALUES ('$CENTREONPLUGINS$');

-- Creation of a new table with the categories for the ppm
CREATE TABLE mod_ppm_categories
(
`id` INT PRIMARY KEY NOT NULL AUTO_INCREMENT,
`name` VARCHAR(100) NOT NULL,
`parent_id` INT(11) DEFAULT NULL,
`icon` VARCHAR(100) NULL,
`priority` INT(11) NULL,
KEY `mpc_index` (`parent_id`),
CONSTRAINT `mpc_fk` FOREIGN KEY (`parent_id`) REFERENCES `mod_ppm_categories` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--Insert in mod_ppm_categories the categories
INSERT INTO mod_ppm_categories (`name`, `parent_id`, `icon`, `priority`)
VALUES
('Sensor', NULL, 'sensor.png', 1),
('Printer', NULL, 'printer.png', 2),
('Network', NULL, 'network.png', 3),
('Hardware-server', NULL, 'hardware-server.png', 4),
('Operating System', (SELECT ppmc.`id` FROM mod_ppm_categories ppmc WHERE ppmc.`name` = 'Hardware-server'), 'operating-system.png', 5),
('Database', (SELECT ppmc.`id` FROM mod_ppm_categories ppmc WHERE ppmc.`name` = 'Hardware-server'), 'database.png', 6),
('Centreon', (SELECT ppmc.`id` FROM mod_ppm_categories ppmc WHERE ppmc.`name` = 'Hardware-server'), 'centreon.png', 7),
('Applications', (SELECT ppmc.`id` FROM mod_ppm_categories ppmc WHERE ppmc.`name` = 'Hardware-server'), 'applications.png', 8),
('Storage', NULL, 'storage.png', 9),
('ToIp-VoIP', NULL, 'toip-voip.png', 10),
('UPS-PDU', NULL, 'ups-pdu.png', 11),
('Protocol', NULL, 'protocol.png', 12),
('Cloud', NULL, 'cloud.png', 13),
('Virtualization', NULL, 'virtualization.png', 14);

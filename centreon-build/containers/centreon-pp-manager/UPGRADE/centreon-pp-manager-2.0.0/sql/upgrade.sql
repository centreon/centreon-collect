-- Lock plugin pack commands
UPDATE command
SET command_locked = 1
WHERE command_id IN (
  SELECT DISTINCT command_id FROM mod_pp_commands
);

INSERT INTO `topology` (`topology_name`, `topology_parent`, `topology_page`, `topology_order`, `topology_group`, `topology_url`, `topology_url_opt`, `topology_popup`, `topology_modules`, `topology_show`, `topology_style_class`, `topology_style_id`, `topology_OnClick`, `readonly`)
VALUES ('Plugin packs description',507,50799,40,40,'./modules/centreon-pp-manager/core/manager/displayDocumentation.php',NULL,NULL,NULL,'0',NULL,NULL,NULL,'1');

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

-- Drop foreign keys
ALTER TABLE `mod_pp_commands` DROP FOREIGN KEY `mod_pp_command_fk1`;
ALTER TABLE `mod_pp_commands` DROP FOREIGN KEY `mod_pp_command_fk2`;
ALTER TABLE `mod_pp_host_templates` DROP FOREIGN KEY `mod_pp_host_templates_fk1`;
ALTER TABLE `mod_pp_host_templates` DROP FOREIGN KEY `mod_pp_host_templates_fk2`;
ALTER TABLE `mod_pp_service_templates` DROP FOREIGN KEY `mod_pp_service_templates_fk1`;
ALTER TABLE `mod_pp_service_templates` DROP FOREIGN KEY `mod_pp_service_templates_fk2`;

-- Transform table mod_pluginpack to mod_ppm_pluginpack
RENAME TABLE `mod_pluginpack`  TO `mod_ppm_pluginpack`;
ALTER TABLE `mod_ppm_pluginpack` CHANGE `pp_id` `pluginpack_id` INT UNSIGNED NOT NULL AUTO_INCREMENT;
ALTER TABLE `mod_ppm_pluginpack` CHANGE `pp_name` `name` VARCHAR(255) NOT NULL;
ALTER TABLE `mod_ppm_pluginpack` CHANGE `pp_version` `version` VARCHAR(50) NOT NULL;
ALTER TABLE `mod_ppm_pluginpack` CHANGE `pp_status` `status` VARCHAR(20) NOT NULL;
ALTER TABLE `mod_ppm_pluginpack` CHANGE `pp_status_message` `status_message` VARCHAR(255) NULL;
ALTER TABLE `mod_ppm_pluginpack` DROP `pp_release`;
ALTER TABLE `mod_ppm_pluginpack` ADD `slug` VARCHAR(255) AFTER `name`;
ALTER TABLE `mod_ppm_pluginpack` ADD `discovery_category` INT UNSIGNED NULL;
ALTER TABLE `mod_ppm_pluginpack` ADD `changelog` TEXT NULL;
ALTER TABLE `mod_ppm_pluginpack` ADD `monitoring_procedure` TEXT NULL;
ALTER TABLE `mod_ppm_pluginpack` ADD `icon` INT(11) DEFAULT NULL;
CREATE UNIQUE INDEX `slug_UNIQUE` ON `mod_ppm_pluginpack` (`slug` ASC);
CREATE INDEX `mod_ppm_pluginpack_fk01_idx` ON `mod_ppm_pluginpack` (`discovery_category` ASC);
ALTER TABLE `mod_ppm_pluginpack` ADD CONSTRAINT `mod_ppm_pluginpack_fk01`
  FOREIGN KEY (`discovery_category`)
  REFERENCES `mod_ppm_discovery_category` (`discovery_category_id`)
  ON DELETE SET NULL;
ALTER TABLE `mod_ppm_pluginpack` ADD CONSTRAINT `mod_ppm_pluginpack_fk02`
  FOREIGN KEY (`icon`)
  REFERENCES `mod_ppm_icons` (`icon_id`)
  ON DELETE CASCADE;

RENAME TABLE `mod_pp_commands` TO `mod_ppm_pluginpack_command`;
ALTER TABLE `mod_ppm_pluginpack_command` CHANGE `pp_id` `pluginpack_id` INT UNSIGNED NOT NULL;
ALTER TABLE `mod_ppm_pluginpack_command` DROP INDEX `pp_id`;
ALTER TABLE `mod_ppm_pluginpack_command` DROP INDEX `command_id`;
ALTER TABLE `mod_ppm_pluginpack_command` ADD CONSTRAINT `mod_ppm_pluginpack_command_fk01`
  FOREIGN KEY (`pluginpack_id`)
  REFERENCES `mod_ppm_pluginpack` (`pluginpack_id`)
  ON DELETE CASCADE
  ON UPDATE NO ACTION;
ALTER TABLE `mod_ppm_pluginpack_command` ADD CONSTRAINT `mod_ppm_pluginpack_command_fk02`
  FOREIGN KEY (`command_id`)
  REFERENCES `command` (`command_id`)
  ON DELETE CASCADE
  ON UPDATE NO ACTION;
ALTER TABLE `mod_ppm_pluginpack_command` ADD PRIMARY KEY (`pluginpack_id`,`command_id`);
ALTER TABLE `mod_ppm_pluginpack_command` DROP INDEX `mod_ppm_pluginpack_command_fk02`,
ADD INDEX `mod_ppm_pluginpack_command_fk02_idx` (`command_id` ASC);

CREATE TABLE IF NOT EXISTS `mod_ppm_pluginpack_dependency` (
  `pluginpack_id` INT UNSIGNED NOT NULL,
  `pluginpack_dep_id` INT UNSIGNED NOT NULL,
  `pluginpack_dep_version` VARCHAR(50) NOT NULL,
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

RENAME TABLE `mod_pp_host_templates` TO `mod_ppm_pluginpack_host`;
ALTER TABLE `mod_ppm_pluginpack_host` CHANGE `pp_id` `pluginpack_id` INT UNSIGNED NOT NULL;
ALTER TABLE `mod_ppm_pluginpack_host` ADD CONSTRAINT `mod_ppm_pluginpack_host_fk01`
  FOREIGN KEY (`pluginpack_id`)
  REFERENCES `mod_ppm_pluginpack` (`pluginpack_id`)
  ON DELETE CASCADE
  ON UPDATE NO ACTION;
ALTER TABLE `mod_ppm_pluginpack_host` ADD CONSTRAINT `mod_ppm_pluginpack_host_fk02`
  FOREIGN KEY (`host_id`)
  REFERENCES `host` (`host_id`)
  ON DELETE CASCADE
  ON UPDATE NO ACTION;
ALTER TABLE `mod_ppm_pluginpack_host` ADD PRIMARY KEY (`pluginpack_id`, `host_id`);
ALTER TABLE `mod_ppm_pluginpack_host` DROP INDEX `host_id`,
ADD INDEX `mod_ppm_pluginpack_host_fk02_idx` (`host_id` ASC);
ALTER TABLE `mod_ppm_pluginpack_host` DROP INDEX `pp_id`,
ADD INDEX `pluginpack_id` (`pluginpack_id`);

RENAME TABLE `mod_pp_service_templates` TO `mod_ppm_pluginpack_service`;
ALTER TABLE `mod_ppm_pluginpack_service` CHANGE `pp_id` `pluginpack_id` INT UNSIGNED NOT NULL;
ALTER TABLE `mod_ppm_pluginpack_service` CHANGE `service_id` `svc_id` int(11) NOT NULL;
ALTER TABLE `mod_ppm_pluginpack` ADD `discovery_command_options` VARCHAR(100) NULL;
ALTER TABLE `mod_ppm_pluginpack` ADD `monitoring_protocol` VARCHAR(100) NULL;
ALTER TABLE `mod_ppm_pluginpack_service` ADD CONSTRAINT `mod_ppm_pluginpack_service_fk01`
  FOREIGN KEY (`pluginpack_id`)
  REFERENCES `mod_ppm_pluginpack` (`pluginpack_id`)
  ON DELETE CASCADE
  ON UPDATE NO ACTION;
ALTER TABLE `mod_ppm_pluginpack_service` ADD CONSTRAINT `mod_ppm_pluginpack_service_fk02`
  FOREIGN KEY (`svc_id`)
  REFERENCES `service` (`service_id`)
  ON DELETE CASCADE
  ON UPDATE NO ACTION;
ALTER TABLE `mod_ppm_pluginpack_service` ADD PRIMARY KEY (`pluginpack_id`, `svc_id`);
ALTER TABLE `mod_ppm_pluginpack_service` DROP INDEX `service_id`,
ADD INDEX `mod_ppm_pluginpack_service_fk02_idx` (`svc_id` ASC);
ALTER TABLE `mod_ppm_pluginpack_service` DROP INDEX `pp_id`,
ADD INDEX `pluginpack_id` (`pluginpack_id`);

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

UPDATE mod_ppm_pluginpack pp SET pp.slug = pp.name;
ALTER TABLE `mod_ppm_pluginpack` MODIFY `slug` VARCHAR(255) NOT NULL;
ALTER TABLE mod_ppm_pluginpack ADD UNIQUE (slug);


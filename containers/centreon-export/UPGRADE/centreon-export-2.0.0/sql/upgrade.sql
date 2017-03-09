INSERT INTO `topology_JS` (`id_page`, `o`, `PathName_js`, `Init`) VALUES (61101,NULL,'./include/common/javascript/jquery/plugins/sheepit/jquery.sheepItPlugin.min.js',NULL);
INSERT INTO `topology_JS` (`id_page`, `o`, `PathName_js`, `Init`) VALUES (61101,NULL,'./include/common/javascript/centreon/doClone.js',NULL);

CREATE TABLE IF NOT EXISTS `mod_export_tpl_status` (
  `htpl_id` int(11) DEFAULT NULL,
  `status` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE IF NOT EXISTS `mod_export_icons` (
  `icons_id` int(11) NOT NULL AUTO_INCREMENT,
  `icons_file` BLOB DEFAULT NULL,
  PRIMARY KEY (`icons_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE IF NOT EXISTS `mod_export_pluginpack` (
  `plugin_id` int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`plugin_id`),
  `plugin_name` varchar(255) NOT NULL,
  `plugin_display_name` varchar(15) NULL,
  `plugin_slug` varchar(255) NOT NULL,
  `plugin_author` varchar(255) NULL,
  `plugin_email` varchar(255) NULL,
  `plugin_website` varchar(255) NULL,
  `plugin_status` varchar(255) NULL,
  `plugin_compatibility` varchar(255) NULL,
  `plugin_status_message` varchar(255) NULL,
  `plugin_last_update` int(11) NOT NULL,
  `plugin_path` varchar(255) NULL,
  `plugin_icon` int(11) DEFAULT NULL,
  `plugin_version` varchar(20) NOT NULL,
  `plugin_monitoring_procedure` varchar(255) NULL,
  `plugin_discovery_category` varchar(30) NULL,
  `plugin_change_log` TEXT DEFAULT NULL,
  `plugin_flag` varchar(1) NOT NULL DEFAULT 1,
  `plugin_time_generate` int(11) NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS  `mod_export_links` (
  `link_id` INT(11) NOT NULL AUTO_INCREMENT,
  `link_pack` VARCHAR( 255 ) NULL ,
  `link_htpl` INT(11) NULL ,
  `link_stpl` INT(11) NULL ,
  `link_included_command` int(11) DEFAULT NULL,
  `link_exclude_htpl` int(11) DEFAULT NULL,
  `link_exclude_stpl` int(11) DEFAULT NULL,
  `link_pack_id` int(11) DEFAULT NULL,
  PRIMARY KEY ( `link_id` ),
  CONSTRAINT `mod_export_links_ibfk_1` FOREIGN KEY (`link_pack_id`) REFERENCES `mod_export_pluginpack` (`plugin_id`) ON DELETE CASCADE
) ENGINE = InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `mod_export_locale` (
  `locale_id` int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`locale_id`),
  `locale_short_name` varchar(3) NOT NULL,
  `locale_long_name` varchar(255) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE IF NOT EXISTS `mod_export_description` (
  `description_id` int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`description_id`),
  `description_plugin_id` int(11) NOT NULL,
  `description_text` TEXT DEFAULT NULL,
  `description_locale` int(11) DEFAULT NULL,
   CONSTRAINT `mod_export_description_ibfk_1` FOREIGN KEY (`description_plugin_id`) REFERENCES `mod_export_pluginpack` (`plugin_id`) ON DELETE CASCADE,
   CONSTRAINT `mod_export_description_ibfk_2` FOREIGN KEY (`description_locale`) REFERENCES `mod_export_locale` (`locale_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE IF NOT EXISTS `mod_export_dependencies` (
  `dependencies_id` int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`dependencies_id`),
  `dependencies_plugin_id` int(11) NOT NULL,
  `dependencies_name` varchar(255) DEFAULT NULL,
  `dependencies_version` varchar(255) DEFAULT NULL,
   CONSTRAINT `mod_export_dependencies_ibfk_1` FOREIGN KEY (`dependencies_plugin_id`) REFERENCES `mod_export_pluginpack` (`plugin_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE IF NOT EXISTS `mod_export_requirements` (
  `requirement_id` int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`requirement_id`),
  `requirement_plugin_id` int(11) NOT NULL,
  `requirement_name` varchar(255) DEFAULT NULL,
  `requirement_version` varchar(255) DEFAULT NULL,
   CONSTRAINT `mod_export_requirements_ibfk_1` FOREIGN KEY (`requirement_plugin_id`) REFERENCES `mod_export_pluginpack` (`plugin_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `mod_export_tags` (
  `tags_id` int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`tags_id`),
  `tags_name` varchar(255) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE IF NOT EXISTS `mod_export_tags_relation` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`id`),
  `tag_id` int(11) DEFAULT NULL,
  `plugin_id` int(11) DEFAULT NULL,
  CONSTRAINT `mod_export_tags_relation_ibfk_1` FOREIGN KEY (`tag_id`) REFERENCES `mod_export_tags` (`tags_id`) ON DELETE CASCADE,
  CONSTRAINT `mod_export_tags_relation_ibfk_2` FOREIGN KEY (`plugin_id`) REFERENCES `mod_export_pluginpack` (`plugin_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

     
DELETE FROM `topology` WHERE `topology_page` = '61102';

CREATE TABLE IF NOT EXISTS `mod_export_hostTpl` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`id`),
  `host_id` int(11) NOT NULL,
  `status` enum('0','1') DEFAULT '0',
  `plugin_id` int(11) DEFAULT NULL,
   CONSTRAINT `mod_export_hostTpl_ibfk_1` FOREIGN KEY (`plugin_id`) REFERENCES `mod_export_pluginpack` (`plugin_id`) ON DELETE CASCADE,
   CONSTRAINT `mod_export_hostTpl_ibfk_2` FOREIGN KEY (`host_id`) REFERENCES `host` (`host_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE IF NOT EXISTS `mod_export_serviceTpl` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`id`),
  `service_id` int(11) NOT NULL,
  `status` enum('0','1') DEFAULT '0',
  `plugin_id` int(11) DEFAULT NULL,
   CONSTRAINT `mod_export_serviceTpl_ibfk_1` FOREIGN KEY (`plugin_id`) REFERENCES `mod_export_pluginpack` (`plugin_id`) ON DELETE CASCADE,
   CONSTRAINT `mod_export_serviceTpl_ibfk_2` FOREIGN KEY (`service_id`) REFERENCES `service` (`service_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;



CREATE TABLE IF NOT EXISTS `mod_export_command` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`id`),
  `command_id` int(11) NOT NULL,
  `plugin_id` int(11) DEFAULT NULL,
   CONSTRAINT `mod_export_command_ibfk_1` FOREIGN KEY (`plugin_id`) REFERENCES `mod_export_pluginpack` (`plugin_id`) ON DELETE CASCADE,
   CONSTRAINT `mod_export_command_ibfk_2` FOREIGN KEY (`command_id`) REFERENCES `command` (`command_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


DROP TABLE IF EXISTS `mod_export_discovery_category` ;
CREATE TABLE `mod_export_discovery_category` (
  `discovery_category_id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `name` VARCHAR(100) NOT NULL,
  `slug` VARCHAR(100) NOT NULL,
  PRIMARY KEY (`discovery_category_id`),
  UNIQUE INDEX `name_UNIQUE` (`name` ASC),
  UNIQUE INDEX `slug_UNIQUE` (`slug` ASC))
ENGINE = InnoDB;


INSERT INTO `mod_export_discovery_category` ( `slug`, `name`) VALUES
('applications', 'Applications'),
('centreon','Centreon'),
('cloud','Cloud'),
('database','Database'),
('hardware-server','Hardware - Server'),
('network','Network'),
('operating-system','Operating System'),
('printer','Printer'),
('protocol','Protocol'),
('sensor','Sensor'),
('storage','Storage'),
('toip-voip','ToIP-VoIP'),
('ups-pdu','UPS-PDU'),
('virtualization','Virtualization');


DROP TABLE IF EXISTS `mod_ppm_pluginpack` ;
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
  PRIMARY KEY (`pluginpack_id`),
  UNIQUE INDEX `slug_UNIQUE` (`slug` ASC),
  INDEX `mod_ppm_pluginpack_fk01_idx` (`discovery_category` ASC),
  CONSTRAINT `mod_ppm_pluginpack_fk01`
    FOREIGN KEY (`discovery_category`)
    REFERENCES `mod_export_discovery_category` (`discovery_category_id`)
    ON DELETE SET NULL
    ON UPDATE NO ACTION)
ENGINE = InnoDB;

INSERT INTO `mod_ppm_pluginpack` (`pluginpack_id`, `name`, `slug`, `version`, `status`, `discovery_category`) VALUES
(1, 'Windows Servers', 'windows-servers', '1.0.0', 'active', 9),
(2, 'Linux Servers', 'linux-servers', '1.0.0', 'active', 9),
(3, 'Cisco Hardware', 'cisco-hardware', '1.0.0', 'active', 8);


DROP TABLE IF EXISTS `mod_export_pluginpack_host` ;
CREATE TABLE IF NOT EXISTS `mod_export_pluginpack_host` (
  `pluginpack_id` int(11) NOT NULL,
  `discovery_protocol` VARCHAR(100),
  `discovery_command` VARCHAR(100),
  `discovery_validator` VARCHAR(100),
  `discovery_documentation` VARCHAR(100),
  `host_id` INT NOT NULL,
  PRIMARY KEY (`pluginpack_id`, `host_id`),
  INDEX `mod_export_pluginpack_host_fk02_idx` (`host_id` ASC),
  CONSTRAINT `mod_export_pluginpack_host_fk01`
    FOREIGN KEY (`pluginpack_id`)
    REFERENCES `mod_export_pluginpack` (`plugin_id`)
    ON DELETE CASCADE
    ON UPDATE NO ACTION,
  CONSTRAINT `mod_export_pluginpack_host_fk02`
    FOREIGN KEY (`host_id`)
    REFERENCES `host` (`host_id`)
    ON DELETE CASCADE
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


ALTER TABLE `mod_export_pluginpack` MODIFY  `plugin_monitoring_procedure` TEXT NULL;
ALTER TABLE `mod_export_serviceTpl` ADD  `discovery_command` VARCHAR(100);

ALTER TABLE `mod_export_pluginpack_host` drop foreign key mod_export_pluginpack_host_fk01;

ALTER TABLE `mod_export_pluginpack_host` drop foreign key mod_export_pluginpack_host_fk02;

ALTER TABLE `mod_export_pluginpack_host` ADD CONSTRAINT mod_export_pluginpack_host_fk01 FOREIGN KEY (`pluginpack_id`) REFERENCES `mod_export_pluginpack` (`plugin_id`) ON DELETE CASCADE;

ALTER TABLE `mod_export_pluginpack_host` ADD CONSTRAINT mod_export_pluginpack_host_fk02 FOREIGN KEY (`host_id`)  REFERENCES `host` (`host_id`) ON DELETE CASCADE ON UPDATE NO ACTION;

ALTER TABLE `mod_export_description`  drop foreign key mod_export_description_ibfk_2;

ALTER TABLE `mod_export_description`  ADD CONSTRAINT `mod_export_description_ibfk_2` FOREIGN KEY (`description_locale`) REFERENCES `locale` (`locale_id`) ON DELETE CASCADE;

DROP TABLE `mod_export_locale`;

ALTER TABLE `mod_export_hostTpl` MODIFY   `status` enum('0','1') DEFAULT '1';

ALTER TABLE `mod_export_serviceTpl` MODIFY   `status` enum('0','1') DEFAULT '1';

DROP TABLE mod_pp_host_templates;
DROP TABLE mod_pp_service_templates;
DROP TABLE mod_pp_commands;
DROP TABLE mod_pluginpacks;


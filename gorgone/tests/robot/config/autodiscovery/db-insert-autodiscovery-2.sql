-- this was separated in two because in only one file robotframework could not handle the file.
-- there was an error as if the last ; was not there.

CREATE TABLE IF NOT EXISTS `mod_auto_disco_inclusion_exclusion` (
	`exinc_id` int(11) NOT NULL AUTO_INCREMENT,
	`exinc_type` enum('0', '1') DEFAULT '0',
    `exinc_str` VARCHAR(521) NOT NULL,
	`exinc_regexp` VARCHAR(512),
	`rule_id` int(11) NOT NULL,
    `exinc_order` int(11) NOT NULL,
	PRIMARY KEY (`exinc_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

ALTER TABLE `mod_auto_disco_inclusion_exclusion`
  ADD CONSTRAINT `mod_auto_disco_inclusion_exclusion_fk_1` FOREIGN KEY (`rule_id`) REFERENCES `mod_auto_disco_rule` (`rule_id`) ON DELETE CASCADE;

INSERT INTO `mod_auto_disco_inclusion_exclusion` VALUES (2,'0','$name$','(/dev.*|/run.*|/sys.*|/boot)',2,0);

CREATE TABLE IF NOT EXISTS `mod_auto_disco_change` (
	`change_id` int(11) NOT NULL AUTO_INCREMENT,
  	`rule_id` int(11) NOT NULL,
    `change_str` VARCHAR(521) NOT NULL,
	`change_regexp` VARCHAR(512),
    `change_replace` VARCHAR(512),
    `change_modifier` VARCHAR(512),
    `change_order` int(11) NOT NULL,
	PRIMARY KEY (`change_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

ALTER TABLE `mod_auto_disco_change`
  ADD CONSTRAINT `mod_auto_disco_change_fk_1` FOREIGN KEY (`rule_id`) REFERENCES `mod_auto_disco_rule` (`rule_id`) ON DELETE CASCADE;

CREATE TABLE IF NOT EXISTS `mod_auto_disco_ht_rule_relation` (
	`hrr_id` int(11) NOT NULL AUTO_INCREMENT,
	`host_host_id` int(11) NOT NULL,
	`rule_rule_id` int(11) NOT NULL,
	PRIMARY KEY (`hrr_id`),
	KEY `host_host_id` (`host_host_id`),
	KEY `rule_rule_id` (`rule_rule_id`)
) ENGINE=InnoDB  DEFAULT CHARSET=utf8;

ALTER TABLE `mod_auto_disco_ht_rule_relation`
  ADD CONSTRAINT `mod_auto_disco_ht_rule_relation_fk_1` FOREIGN KEY (`host_host_id`) REFERENCES `host` (`host_id`) ON DELETE CASCADE,
  ADD CONSTRAINT `mod_auto_disco_ht_rule_relation_fk_2` FOREIGN KEY (`rule_rule_id`) REFERENCES `mod_auto_disco_rule` (`rule_id`) ON DELETE CASCADE;

INSERT INTO `mod_auto_disco_ht_rule_relation` (`host_host_id`, `rule_rule_id`)  VALUES (10,2);
--
-- STRUCTURE FOR mod_auto_disco_inst_rule_relation
--
CREATE TABLE IF NOT EXISTS `mod_auto_disco_inst_rule_relation` (
  `hrr_id` int(11) NOT NULL AUTO_INCREMENT,
  `instance_id` int(11) NOT NULL,
  `rule_rule_id` int(11) NOT NULL,
  PRIMARY KEY (`hrr_id`),
  KEY `instance_id` (`instance_id`),
  KEY `rule_rule_id` (`rule_rule_id`),
  CONSTRAINT `mod_auto_disco_inst_rule_relation_fk_1` FOREIGN KEY (`instance_id`) REFERENCES `nagios_server` (`id`) ON DELETE CASCADE,
  CONSTRAINT `mod_auto_disco_inst_rule_relation_fk_2` FOREIGN KEY (`rule_rule_id`) REFERENCES `mod_auto_disco_rule` (`rule_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- STRUCTURE FOR mod_auto_disco_rule_contact_relation
--
CREATE TABLE IF NOT EXISTS `mod_auto_disco_rule_contact_relation` (
	`rcr_id` int(11) NOT NULL AUTO_INCREMENT,
	`rule_id` int(11) NOT NULL,
	`contact_id` int(11) DEFAULT NULL,
	`cg_id` int(11) DEFAULT NULL,
	PRIMARY KEY (`rcr_id`),
	KEY `rule_id` (`rule_id`),
	KEY `contact_id` (`contact_id`),
	KEY `cg_id` (`cg_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

ALTER TABLE `mod_auto_disco_rule_contact_relation`
  ADD CONSTRAINT `mod_auto_disco_rule_contact_relation_fk_1` FOREIGN KEY (`rule_id`) REFERENCES `mod_auto_disco_rule` (`rule_id`) ON DELETE CASCADE,
  ADD CONSTRAINT `mod_auto_disco_rule_contact_relation_fk_2` FOREIGN KEY (`contact_id`) REFERENCES `contact` (`contact_id`) ON DELETE SET NULL,
  ADD CONSTRAINT `mod_auto_disco_rule_contact_relation_fk_3` FOREIGN KEY (`cg_id`) REFERENCES `contactgroup` (`cg_id`) ON DELETE SET NULL;

--
-- STRUCTURE FOR mod_auto_disco_rule_service_relation
--
CREATE TABLE IF NOT EXISTS `mod_auto_disco_rule_service_relation` (
	`rsr_id` int(11) NOT NULL AUTO_INCREMENT,
	`service_service_id` int(11) NOT NULL,
	`rule_rule_id` int(11) NOT NULL,
	PRIMARY KEY (`rsr_id`),
    UNIQUE KEY `uniq_service_rule` (`service_service_id`,`rule_rule_id`),
	KEY `service_service_id` (`service_service_id`),
	KEY `rule_rule_id` (`rule_rule_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

ALTER TABLE `mod_auto_disco_rule_service_relation`
  ADD CONSTRAINT `mod_auto_disco_rule_service_relation_fk_1` FOREIGN KEY (`service_service_id`) REFERENCES `service` (`service_id`) ON DELETE CASCADE,
  ADD CONSTRAINT `mod_auto_disco_rule_service_relation_fk_2` FOREIGN KEY (`rule_rule_id`) REFERENCES `mod_auto_disco_rule` (`rule_id`) ON DELETE CASCADE;

--
-- STRUCTURE FOR mod_auto_disco_macro
--
CREATE TABLE IF NOT EXISTS `mod_auto_disco_macro` (
	`macro_id` int(11) NOT NULL AUTO_INCREMENT,
	`macro_name` varchar(255) NOT NULL,
    `macro_value` varchar(255) NOT NULL,
	`rule_id` int(11) NOT NULL,
    `is_empty` tinyint(2) DEFAULT '0',
	PRIMARY KEY (`macro_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

ALTER TABLE `mod_auto_disco_macro`
  ADD CONSTRAINT `mod_auto_disco_macro_fk_2` FOREIGN KEY (`rule_id`) REFERENCES `mod_auto_disco_rule` (`rule_id`) ON DELETE CASCADE;

-- tables used for host discovery

-- used to store provider type (eg: vmware)
CREATE TABLE `mod_host_disco_provider_type` (
    `id` int(11) NOT NULL AUTO_INCREMENT,
    `name` varchar(255) DEFAULT NULL,
    `encryption_salt` varchar(255) NOT NULL,
    `credentials_template` TEXT DEFAULT NULL,
    `test` TEXT DEFAULT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uniq_type` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- used to store general informations of provider
CREATE TABLE `mod_host_disco_provider` (
    `id` int(11) NOT NULL AUTO_INCREMENT,
    `pluginpack_id` int(10) unsigned NOT NULL,
    `version` varchar(6) DEFAULT NULL,
    `name` varchar(255) NOT NULL,
    `slug` varchar(30) NOT NULL,
    `description` varchar(255) DEFAULT NULL,
    `type_id` int(11) NOT NULL,
    `command_id` int(11) DEFAULT NULL,
    `parameters_template` TEXT DEFAULT NULL,
    `attributes` TEXT DEFAULT NULL,
    `need_proxy` tinyint(3) unsigned NOT NULL DEFAULT 0,
    `host_template_id` int(11) DEFAULT NULL,
    `uuid_attributes` TEXT DEFAULT NULL,
    `discovery_examples` MEDIUMTEXT DEFAULT NULL,
    `mappers_examples` MEDIUMTEXT DEFAULT NULL,
    PRIMARY KEY (`id`),
    KEY `mod_host_disco_provider_fk_3` (`command_id`),
    CONSTRAINT `mod_host_disco_provider_fk_3` FOREIGN KEY (`command_id`) REFERENCES `command` (`command_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `mod_host_disco_provider_compatibility` (
     provider_version varchar(10)   not null,
     module_versions  varchar(255) not null,
     constraint mod_host_disco_provider_compatibility_uk unique (provider_version)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT INTO `mod_host_disco_provider_compatibility` (`provider_version`, `module_versions`)
    VALUES ('3.0', '[21.04.0-beta.1,]'), ('3.1', '[21.10.0-beta.1,]');

-- used to group the credential entities by name and provider
CREATE TABLE `mod_host_disco_credential` (
    `id` int(11) NOT NULL AUTO_INCREMENT,
    `type_id` int(11) NOT NULL,
    `name` varchar(100) NOT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `mod_host_disco_credential_UN` (`type_id`,`name`),
    KEY `mod_host_disco_credential_fk_1` (`type_id`),
    CONSTRAINT `mod_host_disco_credential_fk_1` FOREIGN KEY (`type_id`) REFERENCES `mod_host_disco_provider_type` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- describe each credential parameter to generate dynamically wizard form
CREATE TABLE `mod_host_disco_credential_parameter` (
   `credential_id` int(11) NOT NULL,
   `name` varchar(100) NOT NULL,
   `value` MEDIUMTEXT NOT NULL,
   PRIMARY KEY (`credential_id`,`name`),
   KEY `mod_host_disco_credential_parameter_fk_1` (`credential_id`),
   CONSTRAINT `mod_host_disco_credential_parameter_fk_1` FOREIGN KEY (`credential_id`) REFERENCES `mod_host_disco_credential` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- used to store the job which will be scheduled to discover hosts
CREATE TABLE `mod_host_disco_job` (
    `id` int(11) NOT NULL AUTO_INCREMENT,
    `alias` varchar(100) NOT NULL,
    `credential_id` int(11) DEFAULT NULL,
    `provider_id` int(11) NOT NULL,
    `creation_date` datetime DEFAULT current_timestamp(),
    `execution_mode` tinyint(1) UNSIGNED DEFAULT 0 NOT NULL,
    `cron_expression` varchar(1024) DEFAULT NULL,
    `analysis_mode` tinyint(1) UNSIGNED DEFAULT 0 NOT NULL,
    `save_mode` TINYINT(1) UNSIGNED DEFAULT 1 NOT NULL,
    `status` tinyint(3) unsigned DEFAULT 0 NOT NULL,
    `last_execution` datetime DEFAULT NULL,
    `duration` int(11) DEFAULT 0,
    `discovered_items` int(11) DEFAULT 0 NOT NULL,
    `message` mediumtext DEFAULT NULL,
    `monitoring_server_id` int(11) NOT NULL,
    `proxy` text DEFAULT NULL,
    `token` varchar(50) DEFAULT NULL,
    PRIMARY KEY (`id`),
    KEY `mod_host_disco_job_fk_1` (`credential_id`),
    CONSTRAINT `mod_host_disco_job_fk_1` FOREIGN KEY (`credential_id`) REFERENCES `mod_host_disco_credential` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `mod_host_disco_job_parameter` (
    `job_id` int(11) NOT NULL,
    `name` varchar(100) NOT NULL,
    `value` varchar(512) DEFAULT NULL,
    PRIMARY KEY (`job_id`,`name`),
    KEY `mod_host_disco_job_parameter_fk_1` (`job_id`),
    CONSTRAINT `mod_host_disco_job_parameter_fk_1` FOREIGN KEY (`job_id`) REFERENCES `mod_host_disco_job` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- used to store the information of discovered hosts
CREATE TABLE `mod_host_disco_host` (
    `id` int(11) NOT NULL AUTO_INCREMENT,
    `job_id` int(11) NOT NULL,
    `uuid` varchar(100) DEFAULT NULL,
    `discovery_result` MEDIUMTEXT,
    PRIMARY KEY (`id`),
    KEY `mod_host_disco_host_fk_1` (`job_id`),
    CONSTRAINT `mod_host_disco_host_fk_1` FOREIGN KEY (`job_id`) REFERENCES `mod_host_disco_job` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- used to store the mapper rules that will be applied to discovered hosts
CREATE TABLE `mod_host_disco_mapper` (
    `id` int(11) NOT NULL AUTO_INCREMENT,
    `job_id` int(11) NOT NULL,
    `order` int(10) unsigned NOT NULL DEFAULT 1,
    `type` varchar(100) NOT NULL,
    `parameters` TEXT NOT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `mod_auto_disco_mapper_UN` (`job_id`,`order`),
    CONSTRAINT `mod_auto_disco_mapper_FK` FOREIGN KEY (`job_id`) REFERENCES `mod_host_disco_job` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `mod_host_disco_host_already_discovered` (
  `pluginpack_slug` varchar(255) NOT NULL,
  `uuid` varchar(100) NOT NULL,
  `host_id` int(11) NOT NULL,
  UNIQUE KEY `mod_host_disco_unique_host_UN` (`pluginpack_slug`,`uuid`,`host_id`),
  KEY `mod_host_disco_unique_host_FK` (`host_id`),
  CONSTRAINT `mod_host_disco_unique_host_FK` FOREIGN KEY (`host_id`) REFERENCES `host` (`host_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT INTO `mod_host_disco_job` VALUES (10,'Mocked host discovery',NULL,22,'2025-06-24 17:37:23',0,NULL,0,0,2,'2025-06-24 17:37:23',0,0,'UNKNOWN: SNMP Session: unable to create ',1,NULL,'discovery_10_4414485e');

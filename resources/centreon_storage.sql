CREATE DATABASE IF NOT EXISTS `centreon_storage`;

USE centreon_storage;

SET FOREIGN_KEY_CHECKS=0;

-- BEWARE TO the order of table creation, respect dependencies

--
-- Table structure for table `instances`
--

DROP TABLE IF EXISTS `instances`;
CREATE TABLE `instances` (
  `instance_id` int(11) NOT NULL,
  `name` varchar(255) NOT NULL DEFAULT 'localhost',
  `active_host_checks` tinyint(1) DEFAULT NULL,
  `active_service_checks` tinyint(1) DEFAULT NULL,
  `address` varchar(128) DEFAULT NULL,
  `check_hosts_freshness` tinyint(1) DEFAULT NULL,
  `check_services_freshness` tinyint(1) DEFAULT NULL,
  `daemon_mode` tinyint(1) DEFAULT NULL,
  `description` varchar(128) DEFAULT NULL,
  `end_time` int(11) DEFAULT NULL,
  `engine` varchar(64) DEFAULT NULL,
  `event_handlers` tinyint(1) DEFAULT NULL,
  `failure_prediction` tinyint(1) DEFAULT NULL,
  `flap_detection` tinyint(1) DEFAULT NULL,
  `global_host_event_handler` text,
  `global_service_event_handler` text,
  `last_alive` int(11) DEFAULT NULL,
  `last_command_check` int(11) DEFAULT NULL,
  `last_log_rotation` int(11) DEFAULT NULL,
  `modified_host_attributes` int(11) DEFAULT NULL,
  `modified_service_attributes` int(11) DEFAULT NULL,
  `notifications` tinyint(1) DEFAULT NULL,
  `obsess_over_hosts` tinyint(1) DEFAULT NULL,
  `obsess_over_services` tinyint(1) DEFAULT NULL,
  `passive_host_checks` tinyint(1) DEFAULT NULL,
  `passive_service_checks` tinyint(1) DEFAULT NULL,
  `pid` int(11) DEFAULT NULL,
  `process_perfdata` tinyint(1) DEFAULT NULL,
  `running` tinyint(1) DEFAULT NULL,
  `start_time` int(11) DEFAULT NULL,
  `version` varchar(16) DEFAULT NULL,
  `deleted` tinyint(1) NOT NULL DEFAULT '0',
  `outdated` tinyint(1) NOT NULL DEFAULT '0',
  PRIMARY KEY (`instance_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `hosts`
--

DROP TABLE IF EXISTS `hosts`;
CREATE TABLE `hosts` (
  `host_id` int(11) NOT NULL,
  `name` varchar(255) NOT NULL,
  `instance_id` int(11) NOT NULL,
  `acknowledged` tinyint(1) DEFAULT NULL,
  `acknowledgement_type` smallint(6) DEFAULT NULL,
  `action_url` varchar(2048) DEFAULT NULL,
  `active_checks` tinyint(1) DEFAULT NULL,
  `address` varchar(75) DEFAULT NULL,
  `alias` varchar(100) DEFAULT NULL,
  `check_attempt` smallint(6) DEFAULT NULL,
  `check_command` text,
  `check_freshness` tinyint(1) DEFAULT NULL,
  `check_interval` double DEFAULT NULL,
  `check_period` varchar(75) DEFAULT NULL,
  `check_type` smallint(6) DEFAULT NULL,
  `checked` tinyint(1) DEFAULT NULL,
  `command_line` text,
  `default_active_checks` tinyint(1) DEFAULT NULL,
  `default_event_handler_enabled` tinyint(1) DEFAULT NULL,
  `default_failure_prediction` tinyint(1) DEFAULT NULL,
  `default_flap_detection` tinyint(1) DEFAULT NULL,
  `default_notify` tinyint(1) DEFAULT NULL,
  `default_passive_checks` tinyint(1) DEFAULT NULL,
  `default_process_perfdata` tinyint(1) DEFAULT NULL,
  `display_name` varchar(100) DEFAULT NULL,
  `enabled` tinyint(1) NOT NULL DEFAULT '1',
  `event_handler` varchar(255) DEFAULT NULL,
  `event_handler_enabled` tinyint(1) DEFAULT NULL,
  `execution_time` double DEFAULT NULL,
  `failure_prediction` tinyint(1) DEFAULT NULL,
  `first_notification_delay` double DEFAULT NULL,
  `flap_detection` tinyint(1) DEFAULT NULL,
  `flap_detection_on_down` tinyint(1) DEFAULT NULL,
  `flap_detection_on_unreachable` tinyint(1) DEFAULT NULL,
  `flap_detection_on_up` tinyint(1) DEFAULT NULL,
  `flapping` tinyint(1) DEFAULT NULL,
  `freshness_threshold` double DEFAULT NULL,
  `high_flap_threshold` double DEFAULT NULL,
  `icon_image` varchar(255) DEFAULT NULL,
  `icon_image_alt` varchar(255) DEFAULT NULL,
  `last_check` int(11) DEFAULT NULL,
  `last_hard_state` smallint(6) DEFAULT NULL,
  `last_hard_state_change` int(11) DEFAULT NULL,
  `last_notification` int(11) DEFAULT NULL,
  `last_state_change` int(11) DEFAULT NULL,
  `last_time_down` int(11) DEFAULT NULL,
  `last_time_unreachable` int(11) DEFAULT NULL,
  `last_time_up` int(11) DEFAULT NULL,
  `last_update` int(11) DEFAULT NULL,
  `latency` double DEFAULT NULL,
  `low_flap_threshold` double DEFAULT NULL,
  `max_check_attempts` smallint(6) DEFAULT NULL,
  `modified_attributes` int(11) DEFAULT NULL,
  `next_check` int(11) DEFAULT NULL,
  `next_host_notification` int(11) DEFAULT NULL,
  `no_more_notifications` tinyint(1) DEFAULT NULL,
  `notes` varchar(512) DEFAULT NULL,
  `notes_url` varchar(2048) DEFAULT NULL,
  `notification_interval` double DEFAULT NULL,
  `notification_number` bigint unsigned DEFAULT NULL,
  `notification_period` varchar(75) DEFAULT NULL,
  `notify` tinyint(1) DEFAULT NULL,
  `notify_on_down` tinyint(1) DEFAULT NULL,
  `notify_on_downtime` tinyint(1) DEFAULT NULL,
  `notify_on_flapping` tinyint(1) DEFAULT NULL,
  `notify_on_recovery` tinyint(1) DEFAULT NULL,
  `notify_on_unreachable` tinyint(1) DEFAULT NULL,
  `obsess_over_host` tinyint(1) DEFAULT NULL,
  `output` text,
  `passive_checks` tinyint(1) DEFAULT NULL,
  `percent_state_change` double DEFAULT NULL,
  `perfdata` text,
  `process_perfdata` tinyint(1) DEFAULT NULL,
  `retain_nonstatus_information` tinyint(1) DEFAULT NULL,
  `retain_status_information` tinyint(1) DEFAULT NULL,
  `retry_interval` double DEFAULT NULL,
  `scheduled_downtime_depth` smallint(6) DEFAULT NULL,
  `should_be_scheduled` tinyint(1) DEFAULT NULL,
  `stalk_on_down` tinyint(1) DEFAULT NULL,
  `stalk_on_unreachable` tinyint(1) DEFAULT NULL,
  `stalk_on_up` tinyint(1) DEFAULT NULL,
  `state` smallint(6) DEFAULT NULL,
  `state_type` smallint(6) DEFAULT NULL,
  `statusmap_image` varchar(255) DEFAULT NULL,
  `timezone` varchar(64) DEFAULT NULL,
  `real_state` smallint(6) DEFAULT NULL,
  UNIQUE KEY `host_id` (`host_id`),
  KEY `instance_id` (`instance_id`),
  KEY `host_name` (`name`),
  CONSTRAINT `hosts_ibfk_1` FOREIGN KEY (`instance_id`) REFERENCES `instances` (`instance_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Table structure for table `services`
--

DROP TABLE IF EXISTS `services`;
CREATE TABLE `services` (
  `host_id` int(11) NOT NULL,
  `description` varchar(255) NOT NULL,
  `service_id` int(11) NOT NULL,
  `acknowledged` tinyint(1) DEFAULT NULL,
  `acknowledgement_type` smallint(6) DEFAULT NULL,
  `action_url` varchar(2048) DEFAULT NULL,
  `active_checks` tinyint(1) DEFAULT NULL,
  `check_attempt` smallint(6) DEFAULT NULL,
  `check_command` text,
  `check_freshness` tinyint(1) DEFAULT NULL,
  `check_interval` double DEFAULT NULL,
  `check_period` varchar(75) DEFAULT NULL,
  `check_type` smallint(6) DEFAULT NULL,
  `checked` tinyint(1) DEFAULT NULL,
  `command_line` text,
  `default_active_checks` tinyint(1) DEFAULT NULL,
  `default_event_handler_enabled` tinyint(1) DEFAULT NULL,
  `default_failure_prediction` tinyint(1) DEFAULT NULL,
  `default_flap_detection` tinyint(1) DEFAULT NULL,
  `default_notify` tinyint(1) DEFAULT NULL,
  `default_passive_checks` tinyint(1) DEFAULT NULL,
  `default_process_perfdata` tinyint(1) DEFAULT NULL,
  `display_name` varchar(160) DEFAULT NULL,
  `enabled` tinyint(1) NOT NULL DEFAULT '1',
  `event_handler` varchar(255) DEFAULT NULL,
  `event_handler_enabled` tinyint(1) DEFAULT NULL,
  `execution_time` double DEFAULT NULL,
  `failure_prediction` tinyint(1) DEFAULT NULL,
  `failure_prediction_options` varchar(64) DEFAULT NULL,
  `first_notification_delay` double DEFAULT NULL,
  `flap_detection` tinyint(1) DEFAULT NULL,
  `flap_detection_on_critical` tinyint(1) DEFAULT NULL,
  `flap_detection_on_ok` tinyint(1) DEFAULT NULL,
  `flap_detection_on_unknown` tinyint(1) DEFAULT NULL,
  `flap_detection_on_warning` tinyint(1) DEFAULT NULL,
  `flapping` tinyint(1) DEFAULT NULL,
  `freshness_threshold` double DEFAULT NULL,
  `high_flap_threshold` double DEFAULT NULL,
  `icon_image` varchar(255) DEFAULT NULL,
  `icon_image_alt` varchar(255) DEFAULT NULL,
  `last_check` int(11) DEFAULT NULL,
  `last_hard_state` smallint(6) DEFAULT NULL,
  `last_hard_state_change` int(11) DEFAULT NULL,
  `last_notification` int(11) DEFAULT NULL,
  `last_state_change` int(11) DEFAULT NULL,
  `last_time_critical` int(11) DEFAULT NULL,
  `last_time_ok` int(11) DEFAULT NULL,
  `last_time_unknown` int(11) DEFAULT NULL,
  `last_time_warning` int(11) DEFAULT NULL,
  `last_update` int(11) DEFAULT NULL,
  `latency` double DEFAULT NULL,
  `low_flap_threshold` double DEFAULT NULL,
  `max_check_attempts` smallint(6) DEFAULT NULL,
  `modified_attributes` int(11) DEFAULT NULL,
  `next_check` int(11) DEFAULT NULL,
  `next_notification` int(11) DEFAULT NULL,
  `no_more_notifications` tinyint(1) DEFAULT NULL,
  `notes` varchar(512) DEFAULT NULL,
  `notes_url` varchar(2048) DEFAULT NULL,
  `notification_interval` double DEFAULT NULL,
  `notification_number` bigint unsigned DEFAULT NULL,
  `notification_period` varchar(75) DEFAULT NULL,
  `notify` tinyint(1) DEFAULT NULL,
  `notify_on_critical` tinyint(1) DEFAULT NULL,
  `notify_on_downtime` tinyint(1) DEFAULT NULL,
  `notify_on_flapping` tinyint(1) DEFAULT NULL,
  `notify_on_recovery` tinyint(1) DEFAULT NULL,
  `notify_on_unknown` tinyint(1) DEFAULT NULL,
  `notify_on_warning` tinyint(1) DEFAULT NULL,
  `obsess_over_service` tinyint(1) DEFAULT NULL,
  `output` text,
  `passive_checks` tinyint(1) DEFAULT NULL,
  `percent_state_change` double DEFAULT NULL,
  `perfdata` text,
  `process_perfdata` tinyint(1) DEFAULT NULL,
  `retain_nonstatus_information` tinyint(1) DEFAULT NULL,
  `retain_status_information` tinyint(1) DEFAULT NULL,
  `retry_interval` double DEFAULT NULL,
  `scheduled_downtime_depth` smallint(6) DEFAULT NULL,
  `should_be_scheduled` tinyint(1) DEFAULT NULL,
  `stalk_on_critical` tinyint(1) DEFAULT NULL,
  `stalk_on_ok` tinyint(1) DEFAULT NULL,
  `stalk_on_unknown` tinyint(1) DEFAULT NULL,
  `stalk_on_warning` tinyint(1) DEFAULT NULL,
  `state` smallint(6) DEFAULT NULL,
  `state_type` smallint(6) DEFAULT NULL,
  `volatile` tinyint(1) DEFAULT NULL,
  `real_state` smallint(6) DEFAULT NULL,
  UNIQUE KEY `host_id` (`host_id`,`service_id`),
  KEY `service_id` (`service_id`),
  KEY `service_description` (`description`),
  KEY `last_hard_state_change` (`last_hard_state_change`),
  CONSTRAINT `services_ibfk_1` FOREIGN KEY (`host_id`) REFERENCES `hosts` (`host_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Holds acknowledgedments information.
--
DROP TABLE IF EXISTS acknowledgements;
CREATE TABLE acknowledgements (
  acknowledgement_id int NOT NULL auto_increment,
  entry_time int NOT NULL,
  host_id int NOT NULL,
  service_id int default NULL,

  author varchar(64) default NULL,
  comment_data varchar(255) default NULL,
  deletion_time int default NULL,
  instance_id int default NULL,
  notify_contacts boolean default NULL,
  persistent_comment boolean default NULL,
  state smallint default NULL,
  sticky boolean default NULL,
  type smallint default NULL,

  PRIMARY KEY (acknowledgement_id),
  INDEX (host_id),
  INDEX (instance_id),
  INDEX (entry_time),
  UNIQUE (entry_time, host_id, service_id),
  FOREIGN KEY (host_id) REFERENCES hosts (host_id)
    ON DELETE CASCADE,
  FOREIGN KEY (instance_id) REFERENCES instances (instance_id)
    ON DELETE SET NULL
) ENGINE=InnoDB;

--
-- Table structure for table `centreon_acl`
--
DROP TABLE IF EXISTS centreon_acl;
CREATE TABLE `centreon_acl` (
  `group_id` int(11) NOT NULL,
  `host_id` int(11) NOT NULL,
  `service_id` int(11) DEFAULT NULL,
  UNIQUE KEY `group_id` (`group_id`,`host_id`,`service_id`),
  KEY `index1` (`host_id`,`service_id`,`group_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `comments`
--
DROP TABLE IF EXISTS comments;
CREATE TABLE `comments` (
  `comment_id` int(11) NOT NULL AUTO_INCREMENT,
  `entry_time` int(11) NOT NULL,
  `host_id` int(11) NOT NULL,
  `service_id` int(11) DEFAULT NULL,
  `author` varchar(64) DEFAULT NULL,
  `data` text,
  `deletion_time` int(11) DEFAULT NULL,
  `entry_type` smallint(6) DEFAULT NULL,
  `expire_time` int(11) DEFAULT NULL,
  `expires` tinyint(1) DEFAULT NULL,
  `instance_id` int(11) DEFAULT NULL,
  `internal_id` int(11) NOT NULL,
  `persistent` tinyint(1) DEFAULT NULL,
  `source` smallint(6) DEFAULT NULL,
  `type` smallint(6) DEFAULT NULL,
  PRIMARY KEY (`comment_id`),
  UNIQUE KEY `entry_time` (`entry_time`,`host_id`,`service_id`,`instance_id`,`internal_id`),
  KEY `internal_id` (`internal_id`),
  KEY `host_id` (`host_id`),
  KEY `instance_id` (`instance_id`),
  CONSTRAINT `comments_ibfk_1` FOREIGN KEY (`host_id`) REFERENCES `hosts` (`host_id`) ON DELETE CASCADE,
  CONSTRAINT `comments_ibfk_2` FOREIGN KEY (`instance_id`) REFERENCES `instances` (`instance_id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `config`
--
DROP TABLE IF EXISTS config;
CREATE TABLE `config` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `RRDdatabase_path` varchar(255) DEFAULT NULL,
  `RRDdatabase_status_path` varchar(255) DEFAULT NULL,
  `RRDdatabase_nagios_stats_path` varchar(255) DEFAULT NULL,
  `len_storage_rrd` int(11) DEFAULT NULL,
  `len_storage_mysql` int(11) DEFAULT NULL,
  `autodelete_rrd_db` enum('0','1') DEFAULT NULL,
  `sleep_time` int(11) DEFAULT '10',
  `purge_interval` int(11) DEFAULT '2',
  `storage_type` int(11) DEFAULT '2',
  `average` int(11) DEFAULT NULL,
  `archive_log` enum('0','1') NOT NULL DEFAULT '0',
  `archive_retention` int(11) DEFAULT '31',
  `reporting_retention` int(11) DEFAULT '365',
  `nagios_log_file` varchar(255) DEFAULT NULL,
  `last_line_read` int(11) DEFAULT '31',
  `audit_log_option` enum('0','1') NOT NULL DEFAULT '1',
  `len_storage_downtimes` int(11) DEFAULT NULL,
  `len_storage_comments` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=utf8;

INSERT INTO `config` (`RRDdatabase_path`, `RRDdatabase_status_path`, `RRDdatabase_nagios_stats_path`, `len_storage_rrd`, `len_storage_mysql`, `autodelete_rrd_db`, `sleep_time`, `purge_interval`, `storage_type`, `average`, `archive_log`, `archive_retention`, `reporting_retention`, `nagios_log_file`, `last_line_read`, `audit_log_option`, `len_storage_downtimes`, `len_storage_comments`) 
  VALUES ('/var/lib/centreon/metrics', '/var/lib/centreon/status', '/var/lib/centreon/nagios-perf', 180, 365, 1, 10, 360, 2, null, 1, 31, 365, "/var/log/centreon-engine/centengine.log.log", 0, 1, 0, 0);
--
-- Table structure for table `customvariables`
--

DROP TABLE IF EXISTS customvariables;
CREATE TABLE `customvariables` (
  `customvariable_id` int(11) NOT NULL AUTO_INCREMENT,
  `host_id` int(11) DEFAULT NULL,
  `name` varchar(255) DEFAULT NULL,
  `service_id` int(11) DEFAULT NULL,
  `default_value` varchar(255) DEFAULT NULL,
  `modified` tinyint(1) DEFAULT NULL,
  `type` smallint(6) DEFAULT NULL,
  `update_time` int(11) DEFAULT NULL,
  `value` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`customvariable_id`),
  UNIQUE KEY `host_id` (`host_id`,`name`,`service_id`)
) ENGINE=InnoDB AUTO_INCREMENT=926242 DEFAULT CHARSET=utf8;

--
-- Table structure for table `data_bin`
--

DROP TABLE IF EXISTS data_bin;
CREATE TABLE `data_bin` (
  `id_metric` int(11) DEFAULT NULL,
  `ctime` int(11) DEFAULT NULL,
  `value` float DEFAULT NULL,
  `status` enum('0','1','2','3','4') DEFAULT NULL,
  KEY `index_metric` (`id_metric`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Table structure for table `data_stats_daily`
--

DROP TABLE IF EXISTS data_stats_daily;
CREATE TABLE `data_stats_daily` (
  `data_stats_daily_id` int(11) NOT NULL AUTO_INCREMENT,
  `metric_id` int(11) DEFAULT NULL,
  `min` int(11) DEFAULT NULL,
  `max` int(11) DEFAULT NULL,
  `average` int(11) DEFAULT NULL,
  `count` int(11) DEFAULT NULL,
  `day_time` int(11) DEFAULT NULL,
  PRIMARY KEY (`data_stats_daily_id`),
  KEY `metric_id` (`metric_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Table structure for table `data_stats_monthly`
--

DROP TABLE IF EXISTS data_stats_monthly;
CREATE TABLE `data_stats_monthly` (
  `data_stats_monthly_id` int(11) NOT NULL AUTO_INCREMENT,
  `metric_id` int(11) DEFAULT NULL,
  `min` int(11) DEFAULT NULL,
  `max` int(11) DEFAULT NULL,
  `average` int(11) DEFAULT NULL,
  `count` int(11) DEFAULT NULL,
  `month_time` int(11) DEFAULT NULL,
  PRIMARY KEY (`data_stats_monthly_id`),
  KEY `metric_id` (`metric_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Table structure for table `data_stats_yearly`
--

DROP TABLE IF EXISTS `data_stats_yearly`;
CREATE TABLE `data_stats_yearly` (
  `data_stats_yearly_id` int(11) NOT NULL AUTO_INCREMENT,
  `metric_id` int(11) DEFAULT NULL,
  `min` int(11) DEFAULT NULL,
  `max` int(11) DEFAULT NULL,
  `average` int(11) DEFAULT NULL,
  `count` int(11) DEFAULT NULL,
  `year_time` int(11) DEFAULT NULL,
  PRIMARY KEY (`data_stats_yearly_id`),
  KEY `metric_id` (`metric_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `downtimes`
--



DROP TABLE IF EXISTS `downtimes`;
CREATE TABLE `downtimes` (
  `downtime_id` int(11) NOT NULL AUTO_INCREMENT,
  `entry_time` int(11) DEFAULT NULL,
  `host_id` int(11) NOT NULL,
  `service_id` int(11) DEFAULT NULL,
  `author` varchar(64) DEFAULT NULL,
  `cancelled` tinyint(1) DEFAULT NULL,
  `comment_data` text,
  `deletion_time` int(11) DEFAULT NULL,
  `duration` bigint unsigned DEFAULT NULL,
  `end_time` bigint unsigned DEFAULT NULL,
  `fixed` tinyint(1) DEFAULT NULL,
  `instance_id` int(11) DEFAULT NULL,
  `internal_id` int(11) DEFAULT NULL,
  `start_time` bigint unsigned DEFAULT NULL,
  `actual_start_time` bigint unsigned DEFAULT NULL,
  `actual_end_time` bigint unsigned DEFAULT NULL,
  `started` tinyint(1) DEFAULT NULL,
  `triggered_by` int(11) DEFAULT NULL,
  `type` smallint(6) DEFAULT NULL,
  PRIMARY KEY (`downtime_id`),
  UNIQUE KEY `entry_time` (`entry_time`,`instance_id`,`internal_id`),
  KEY `host_id` (`host_id`),
  KEY `instance_id` (`instance_id`),
  KEY `entry_time_2` (`entry_time`),
  KEY `downtimeManager_hostList` (`host_id`,`start_time`),
  CONSTRAINT `downtimes_ibfk_1` FOREIGN KEY (`host_id`) REFERENCES `hosts` (`host_id`) ON DELETE CASCADE,
  CONSTRAINT `downtimes_ibfk_2` FOREIGN KEY (`instance_id`) REFERENCES `instances` (`instance_id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `eventhandlers`
--

DROP TABLE IF EXISTS `eventhandlers`;
CREATE TABLE `eventhandlers` (
  `eventhandler_id` int(11) NOT NULL AUTO_INCREMENT,
  `host_id` int(11) DEFAULT NULL,
  `service_id` int(11) DEFAULT NULL,
  `start_time` int(11) DEFAULT NULL,
  `command_args` varchar(255) DEFAULT NULL,
  `command_line` varchar(255) DEFAULT NULL,
  `early_timeout` smallint(6) DEFAULT NULL,
  `end_time` int(11) DEFAULT NULL,
  `execution_time` double DEFAULT NULL,
  `output` varchar(255) DEFAULT NULL,
  `return_code` smallint(6) DEFAULT NULL,
  `state` smallint(6) DEFAULT NULL,
  `state_type` smallint(6) DEFAULT NULL,
  `timeout` smallint(6) DEFAULT NULL,
  `type` smallint(6) DEFAULT NULL,
  PRIMARY KEY (`eventhandler_id`),
  UNIQUE KEY `host_id` (`host_id`,`service_id`,`start_time`),
  CONSTRAINT `eventhandlers_ibfk_1` FOREIGN KEY (`host_id`) REFERENCES `hosts` (`host_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `flappingstatuses`
--

DROP TABLE IF EXISTS `flappingstatuses`;
CREATE TABLE `flappingstatuses` (
  `flappingstatus_id` int(11) NOT NULL AUTO_INCREMENT,
  `host_id` int(11) DEFAULT NULL,
  `service_id` int(11) DEFAULT NULL,
  `event_time` int(11) DEFAULT NULL,
  `comment_time` int(11) DEFAULT NULL,
  `event_type` smallint(6) DEFAULT NULL,
  `high_threshold` double DEFAULT NULL,
  `internal_comment_id` int(11) DEFAULT NULL,
  `low_threshold` double DEFAULT NULL,
  `percent_state_change` double DEFAULT NULL,
  `reason_type` smallint(6) DEFAULT NULL,
  `type` smallint(6) DEFAULT NULL,
  PRIMARY KEY (`flappingstatus_id`),
  UNIQUE KEY `host_id` (`host_id`,`service_id`,`event_time`),
  CONSTRAINT `flappingstatuses_ibfk_1` FOREIGN KEY (`host_id`) REFERENCES `hosts` (`host_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `hostgroups`
--

DROP TABLE IF EXISTS `hostgroups`;
CREATE TABLE `hostgroups` (
  `hostgroup_id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL,
  PRIMARY KEY (`hostgroup_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;




--
-- Table structure for table `hosts_hostgroups`
--

DROP TABLE IF EXISTS `hosts_hostgroups`;
CREATE TABLE `hosts_hostgroups` (
  `host_id` int(11) NOT NULL,
  `hostgroup_id` int(11) NOT NULL,
  UNIQUE KEY `host_id` (`host_id`,`hostgroup_id`),
  KEY `hostgroup_id` (`hostgroup_id`),
  CONSTRAINT `hosts_hostgroups_ibfk_1` FOREIGN KEY (`host_id`) REFERENCES `hosts` (`host_id`) ON DELETE CASCADE,
  CONSTRAINT `hosts_hostgroups_ibfk_2` FOREIGN KEY (`hostgroup_id`) REFERENCES `hostgroups` (`hostgroup_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `hosts_hosts_dependencies`
--

DROP TABLE IF EXISTS `hosts_hosts_dependencies`;
CREATE TABLE `hosts_hosts_dependencies` (
  `dependent_host_id` int(11) NOT NULL,
  `host_id` int(11) NOT NULL,
  `dependency_period` varchar(75) DEFAULT NULL,
  `execution_failure_options` varchar(15) DEFAULT NULL,
  `inherits_parent` tinyint(1) DEFAULT NULL,
  `notification_failure_options` varchar(15) DEFAULT NULL,
  UNIQUE KEY `dependent_host_id` (`dependent_host_id`,`host_id`),
  KEY `host_id` (`host_id`),
  CONSTRAINT `hosts_hosts_dependencies_ibfk_1` FOREIGN KEY (`dependent_host_id`) REFERENCES `hosts` (`host_id`) ON DELETE CASCADE,
  CONSTRAINT `hosts_hosts_dependencies_ibfk_2` FOREIGN KEY (`host_id`) REFERENCES `hosts` (`host_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `hosts_hosts_parents`
--

DROP TABLE IF EXISTS `hosts_hosts_parents`;
CREATE TABLE `hosts_hosts_parents` (
  `child_id` int(11) NOT NULL,
  `parent_id` int(11) NOT NULL,
  UNIQUE KEY `child_id` (`child_id`,`parent_id`),
  KEY `parent_id` (`parent_id`),
  CONSTRAINT `hosts_hosts_parents_ibfk_1` FOREIGN KEY (`child_id`) REFERENCES `hosts` (`host_id`) ON DELETE CASCADE,
  CONSTRAINT `hosts_hosts_parents_ibfk_2` FOREIGN KEY (`parent_id`) REFERENCES `hosts` (`host_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `hoststateevents`
--

DROP TABLE IF EXISTS `hoststateevents`;
CREATE TABLE `hoststateevents` (
  `hoststateevent_id` int(11) NOT NULL AUTO_INCREMENT,
  `end_time` int(11) DEFAULT NULL,
  `host_id` int(11) NOT NULL,
  `start_time` int(11) NOT NULL,
  `state` tinyint(11) NOT NULL,
  `last_update` tinyint(4) NOT NULL DEFAULT '0',
  `in_downtime` tinyint(4) NOT NULL,
  `ack_time` int(11) DEFAULT NULL,
  `in_ack` tinyint(4) DEFAULT '0',
  PRIMARY KEY (`hoststateevent_id`),
  UNIQUE KEY `host_id` (`host_id`,`start_time`),
  KEY `start_time` (`start_time`),
  KEY `end_time` (`end_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `index_data`
--

DROP TABLE IF EXISTS `index_data`;
CREATE TABLE `index_data` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `host_name` varchar(255) DEFAULT NULL,
  `host_id` int(11) DEFAULT NULL,
  `service_description` varchar(255) DEFAULT NULL,
  `service_id` int(11) DEFAULT NULL,
  `check_interval` int(11) DEFAULT NULL,
  `special` enum('0','1') DEFAULT '0',
  `hidden` enum('0','1') DEFAULT '0',
  `locked` enum('0','1') DEFAULT '0',
  `trashed` enum('0','1') DEFAULT '0',
  `must_be_rebuild` enum('0','1','2') DEFAULT '0',
  `storage_type` enum('0','1','2') DEFAULT '2',
  `to_delete` int(1) DEFAULT '0',
  `rrd_retention` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `host_service_unique_id` (`host_id`,`service_id`),
  KEY `host_name` (`host_name`),
  KEY `service_description` (`service_description`),
  KEY `host_id` (`host_id`),
  KEY `service_id` (`service_id`),
  KEY `must_be_rebuild` (`must_be_rebuild`),
  KEY `trashed` (`trashed`)
) ENGINE=InnoDB AUTO_INCREMENT=6223 DEFAULT CHARSET=utf8;



--
-- Table structure for table `issues`
--

DROP TABLE IF EXISTS `issues`;
CREATE TABLE `issues` (
  `issue_id` int(11) NOT NULL AUTO_INCREMENT,
  `host_id` int(11) DEFAULT NULL,
  `service_id` int(11) DEFAULT NULL,
  `start_time` int(11) NOT NULL,
  `ack_time` int(11) DEFAULT NULL,
  `end_time` int(11) DEFAULT NULL,
  PRIMARY KEY (`issue_id`),
  UNIQUE KEY `host_id` (`host_id`,`service_id`,`start_time`),
  KEY `start_time` (`start_time`),
  CONSTRAINT `issues_ibfk_1` FOREIGN KEY (`host_id`) REFERENCES `hosts` (`host_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `issues_issues_parents`
--

DROP TABLE IF EXISTS `issues_issues_parents`;
CREATE TABLE `issues_issues_parents` (
  `child_id` int(11) NOT NULL,
  `end_time` int(11) DEFAULT NULL,
  `start_time` int(11) NOT NULL,
  `parent_id` int(11) NOT NULL,
  KEY `child_id` (`child_id`),
  KEY `parent_id` (`parent_id`),
  CONSTRAINT `issues_issues_parents_ibfk_1` FOREIGN KEY (`child_id`) REFERENCES `issues` (`issue_id`) ON DELETE CASCADE,
  CONSTRAINT `issues_issues_parents_ibfk_2` FOREIGN KEY (`parent_id`) REFERENCES `issues` (`issue_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `log_action`
--

DROP TABLE IF EXISTS `log_action`;
CREATE TABLE `log_action` (
  `action_log_id` int(11) NOT NULL AUTO_INCREMENT,
  `action_log_date` int(11) NOT NULL,
  `object_type` varchar(255) NOT NULL,
  `object_id` int(11) NOT NULL,
  `object_name` varchar(255) NOT NULL,
  `action_type` varchar(255) NOT NULL,
  `log_contact_id` int(11) NOT NULL,
  PRIMARY KEY (`action_log_id`),
  KEY `log_contact_id` (`log_contact_id`),
  KEY `action_log_date` (`action_log_date`)
) ENGINE=InnoDB AUTO_INCREMENT=2894 DEFAULT CHARSET=utf8;


--
-- Table structure for table `log_action_modification`
--

DROP TABLE IF EXISTS `log_action_modification`;
CREATE TABLE `log_action_modification` (
  `modification_id` int(11) NOT NULL AUTO_INCREMENT,
  `field_name` varchar(255) NOT NULL,
  `field_value` text NOT NULL,
  `action_log_id` int(11) NOT NULL,
  PRIMARY KEY (`modification_id`),
  KEY `action_log_id` (`action_log_id`)
) ENGINE=InnoDB AUTO_INCREMENT=134198 DEFAULT CHARSET=utf8;


--
-- Table structure for table `log_archive_host`
--
DROP TABLE IF EXISTS `log_archive_host`;
CREATE TABLE `log_archive_host` (
  `log_id` int(11) NOT NULL AUTO_INCREMENT,
  `host_id` int(11) DEFAULT NULL,
  `UPTimeScheduled` int(11) DEFAULT NULL,
  `UPnbEvent` int(11) DEFAULT NULL,
  `UPTimeAverageAck` int(11) NOT NULL,
  `UPTimeAverageRecovery` int(11) NOT NULL,
  `DOWNTimeScheduled` int(11) DEFAULT NULL,
  `DOWNnbEvent` int(11) DEFAULT NULL,
  `DOWNTimeAverageAck` int(11) NOT NULL,
  `DOWNTimeAverageRecovery` int(11) NOT NULL,
  `UNREACHABLETimeScheduled` int(11) DEFAULT NULL,
  `UNREACHABLEnbEvent` int(11) DEFAULT NULL,
  `UNREACHABLETimeAverageAck` int(11) NOT NULL,
  `UNREACHABLETimeAverageRecovery` int(11) NOT NULL,
  `UNDETERMINEDTimeScheduled` int(11) DEFAULT NULL,
  `MaintenanceTime` int(11) DEFAULT '0',
  `date_end` int(11) DEFAULT NULL,
  `date_start` int(11) DEFAULT NULL,
  KEY `log_id` (`log_id`),
  KEY `host_index` (`host_id`),
  KEY `date_end_index` (`date_end`),
  KEY `date_start_index` (`date_start`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `log_archive_last_status`
--

DROP TABLE IF EXISTS `log_archive_last_status`;
CREATE TABLE `log_archive_last_status` (
  `host_id` int(11) DEFAULT NULL,
  `service_id` int(11) DEFAULT NULL,
  `host_name` varchar(255) DEFAULT NULL,
  `service_description` varchar(255) DEFAULT NULL,
  `status` varchar(255) DEFAULT NULL,
  `ctime` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `log_archive_service`
--

DROP TABLE IF EXISTS `log_archive_service`;
CREATE TABLE `log_archive_service` (
  `log_id` int(11) NOT NULL AUTO_INCREMENT,
  `host_id` int(11) NOT NULL DEFAULT '0',
  `service_id` int(11) NOT NULL DEFAULT '0',
  `OKTimeScheduled` int(11) NOT NULL DEFAULT '0',
  `OKnbEvent` int(11) NOT NULL DEFAULT '0',
  `OKTimeAverageAck` int(11) NOT NULL,
  `OKTimeAverageRecovery` int(11) NOT NULL,
  `WARNINGTimeScheduled` int(11) NOT NULL DEFAULT '0',
  `WARNINGnbEvent` int(11) NOT NULL DEFAULT '0',
  `WARNINGTimeAverageAck` int(11) NOT NULL,
  `WARNINGTimeAverageRecovery` int(11) NOT NULL,
  `UNKNOWNTimeScheduled` int(11) NOT NULL DEFAULT '0',
  `UNKNOWNnbEvent` int(11) NOT NULL DEFAULT '0',
  `UNKNOWNTimeAverageAck` int(11) NOT NULL,
  `UNKNOWNTimeAverageRecovery` int(11) NOT NULL,
  `CRITICALTimeScheduled` int(11) NOT NULL DEFAULT '0',
  `CRITICALnbEvent` int(11) NOT NULL DEFAULT '0',
  `CRITICALTimeAverageAck` int(11) NOT NULL,
  `CRITICALTimeAverageRecovery` int(11) NOT NULL,
  `UNDETERMINEDTimeScheduled` int(11) NOT NULL DEFAULT '0',
  `MaintenanceTime` int(11) DEFAULT '0',
  `date_start` int(11) DEFAULT NULL,
  `date_end` int(11) DEFAULT NULL,
  KEY `log_id` (`log_id`),
  KEY `host_index` (`host_id`),
  KEY `service_index` (`service_id`),
  KEY `date_end_index` (`date_end`),
  KEY `date_start_index` (`date_start`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Table structure for table `log_traps`
--

DROP TABLE IF EXISTS `log_traps`;
CREATE TABLE `log_traps` (
  `trap_id` int(11) NOT NULL AUTO_INCREMENT,
  `trap_time` int(11) DEFAULT NULL,
  `timeout` enum('0','1') DEFAULT NULL,
  `host_name` varchar(255) DEFAULT NULL,
  `ip_address` varchar(255) DEFAULT NULL,
  `agent_host_name` varchar(255) DEFAULT NULL,
  `agent_ip_address` varchar(255) DEFAULT NULL,
  `trap_oid` varchar(512) DEFAULT NULL,
  `trap_name` varchar(255) DEFAULT NULL,
  `vendor` varchar(255) DEFAULT NULL,
  `status` int(11) DEFAULT NULL,
  `severity_id` int(11) DEFAULT NULL,
  `severity_name` varchar(255) DEFAULT NULL,
  `output_message` varchar(2048) DEFAULT NULL,
  KEY `trap_id` (`trap_id`),
  KEY `trap_time` (`trap_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `log_traps_args`
--

DROP TABLE IF EXISTS `log_traps_args`;
CREATE TABLE `log_traps_args` (
  `fk_log_traps` int(11) NOT NULL,
  `arg_number` int(11) DEFAULT NULL,
  `arg_oid` varchar(255) DEFAULT NULL,
  `arg_value` varchar(255) DEFAULT NULL,
  `trap_time` int(11) DEFAULT NULL,
  KEY `fk_log_traps` (`fk_log_traps`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `logs`
--

DROP TABLE IF EXISTS `logs`;
CREATE TABLE `logs` (
  `log_id` int(11) NOT NULL AUTO_INCREMENT,
  `ctime` int(11) DEFAULT NULL,
  `host_id` int(11) DEFAULT NULL,
  `host_name` varchar(255) DEFAULT NULL,
  `instance_name` varchar(255) NOT NULL,
  `issue_id` int(11) DEFAULT NULL,
  `msg_type` tinyint(4) DEFAULT NULL,
  `notification_cmd` varchar(255) DEFAULT NULL,
  `notification_contact` varchar(255) DEFAULT NULL,
  `output` text,
  `retry` int(11) DEFAULT NULL,
  `service_description` varchar(255) DEFAULT NULL,
  `service_id` int(11) DEFAULT NULL,
  `status` tinyint(4) DEFAULT NULL,
  `type` smallint(6) DEFAULT NULL,
  KEY `log_id` (`log_id`),
  KEY `host_name` (`host_name`(64)),
  KEY `service_description` (`service_description`(64)),
  KEY `status` (`status`),
  KEY `instance_name` (`instance_name`),
  KEY `ctime` (`ctime`),
  KEY `rq1` (`host_id`,`service_id`,`msg_type`,`status`,`ctime`),
  KEY `rq2` (`host_id`,`msg_type`,`status`,`ctime`)
) ENGINE=InnoDB AUTO_INCREMENT=1129214 DEFAULT CHARSET=utf8;

--
-- Table structure for table `metrics`
--

DROP TABLE IF EXISTS `metrics`;
CREATE TABLE `metrics` (
  `metric_id` int(11) NOT NULL AUTO_INCREMENT,
  `index_id` bigint unsigned DEFAULT NULL,
  `metric_name` varchar(1021) CHARACTER SET utf8 COLLATE utf8_bin DEFAULT NULL,
  `data_source_type` enum('0','1','2','3') DEFAULT NULL,
  `unit_name` varchar(32) DEFAULT NULL,
  `current_value` float DEFAULT NULL,
  `warn` float DEFAULT NULL,
  `warn_low` float DEFAULT NULL,
  `warn_threshold_mode` tinyint(1) DEFAULT NULL,
  `crit` float DEFAULT NULL,
  `crit_low` float DEFAULT NULL,
  `crit_threshold_mode` tinyint(1) DEFAULT NULL,
  `hidden` enum('0','1') DEFAULT '0',
  `min` float DEFAULT NULL,
  `max` float DEFAULT NULL,
  `locked` enum('0','1') DEFAULT NULL,
  `to_delete` int(1) DEFAULT '0',
  PRIMARY KEY (`metric_id`),
  UNIQUE KEY `index_id` (`index_id`,`metric_name`),
  KEY `index` (`index_id`)
) ENGINE=InnoDB AUTO_INCREMENT=5426 DEFAULT CHARSET=utf8;


--
-- Table structure for table `modules`
--

DROP TABLE IF EXISTS `modules`;
CREATE TABLE `modules` (
  `module_id` int(11) NOT NULL AUTO_INCREMENT,
  `instance_id` int(11) NOT NULL,
  `args` varchar(255) DEFAULT NULL,
  `filename` varchar(255) DEFAULT NULL,
  `loaded` tinyint(1) DEFAULT NULL,
  `should_be_loaded` tinyint(1) DEFAULT NULL,
  PRIMARY KEY (`module_id`),
  KEY `instance_id` (`instance_id`),
  CONSTRAINT `modules_ibfk_1` FOREIGN KEY (`instance_id`) REFERENCES `instances` (`instance_id`) ON DELETE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=297 DEFAULT CHARSET=utf8;


--
-- Table structure for table `nagios_stats`
--

DROP TABLE IF EXISTS `nagios_stats`;
CREATE TABLE `nagios_stats` (
  `instance_id` int(11) NOT NULL,
  `stat_key` varchar(255) NOT NULL,
  `stat_value` varchar(255) NOT NULL,
  `stat_label` varchar(255) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Table structure for table `notifications`
--

DROP TABLE IF EXISTS `notifications`;
CREATE TABLE `notifications` (
  `notification_id` int(11) NOT NULL AUTO_INCREMENT,
  `host_id` int(11) DEFAULT NULL,
  `service_id` int(11) DEFAULT NULL,
  `start_time` int(11) DEFAULT NULL,
  `ack_author` varchar(255) DEFAULT NULL,
  `ack_data` text,
  `command_name` varchar(255) DEFAULT NULL,
  `contact_name` varchar(255) DEFAULT NULL,
  `contacts_notified` tinyint(1) DEFAULT NULL,
  `end_time` int(11) DEFAULT NULL,
  `escalated` tinyint(1) DEFAULT NULL,
  `output` text,
  `reason_type` int(11) DEFAULT NULL,
  `state` int(11) DEFAULT NULL,
  `type` int(11) DEFAULT NULL,
  PRIMARY KEY (`notification_id`),
  UNIQUE KEY `host_id` (`host_id`,`service_id`,`start_time`),
  CONSTRAINT `notifications_ibfk_1` FOREIGN KEY (`host_id`) REFERENCES `hosts` (`host_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Table structure for table `schemaversion`
--

DROP TABLE IF EXISTS `schemaversion`;
CREATE TABLE `schemaversion` (
  `software` varchar(128) NOT NULL,
  `version` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Table structure for table `servicegroups`
--

DROP TABLE IF EXISTS `servicegroups`;
CREATE TABLE `servicegroups` (
  `servicegroup_id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(255) NOT NULL,
  PRIMARY KEY (`servicegroup_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


--
-- Table structure for table `services_servicegroups`
--

DROP TABLE IF EXISTS `services_servicegroups`;
CREATE TABLE `services_servicegroups` (
  `host_id` int(11) NOT NULL,
  `service_id` int(11) NOT NULL,
  `servicegroup_id` int(11) NOT NULL,
  UNIQUE KEY `host_id` (`host_id`,`service_id`,`servicegroup_id`),
  KEY `servicegroup_id` (`servicegroup_id`),
  CONSTRAINT `services_servicegroups_ibfk_1` FOREIGN KEY (`host_id`) REFERENCES `hosts` (`host_id`) ON DELETE CASCADE,
  CONSTRAINT `services_servicegroups_ibfk_2` FOREIGN KEY (`servicegroup_id`) REFERENCES `servicegroups` (`servicegroup_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Table structure for table `services_services_dependencies`
--

DROP TABLE IF EXISTS `services_services_dependencies`;
CREATE TABLE `services_services_dependencies` (
  `dependent_host_id` int(11) NOT NULL,
  `dependent_service_id` int(11) NOT NULL,
  `host_id` int(11) NOT NULL,
  `service_id` int(11) NOT NULL,
  `dependency_period` varchar(75) DEFAULT NULL,
  `execution_failure_options` varchar(15) DEFAULT NULL,
  `inherits_parent` tinyint(1) DEFAULT NULL,
  `notification_failure_options` varchar(15) DEFAULT NULL,
  UNIQUE KEY `dependent_host_id` (`dependent_host_id`,`dependent_service_id`,`host_id`,`service_id`),
  KEY `host_id` (`host_id`),
  CONSTRAINT `services_services_dependencies_ibfk_1` FOREIGN KEY (`dependent_host_id`) REFERENCES `hosts` (`host_id`) ON DELETE CASCADE,
  CONSTRAINT `services_services_dependencies_ibfk_2` FOREIGN KEY (`host_id`) REFERENCES `hosts` (`host_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Table structure for table `servicestateevents`
--

DROP TABLE IF EXISTS `servicestateevents`;
CREATE TABLE `servicestateevents` (
  `servicestateevent_id` int(11) NOT NULL AUTO_INCREMENT,
  `end_time` int(11) DEFAULT NULL,
  `host_id` int(11) NOT NULL,
  `service_id` int(11) DEFAULT NULL,
  `start_time` int(11) NOT NULL,
  `state` tinyint(11) NOT NULL,
  `last_update` tinyint(4) NOT NULL DEFAULT '0',
  `in_downtime` tinyint(4) NOT NULL,
  `ack_time` int(11) DEFAULT NULL,
  `in_ack` tinyint(4) DEFAULT '0',
  PRIMARY KEY (`servicestateevent_id`),
  UNIQUE KEY `host_id` (`host_id`,`service_id`,`start_time`),
  KEY `start_time` (`start_time`),
  KEY `end_time` (`end_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Table structure for table `severities`
--
DROP TABLE IF EXISTS `severities`;
CREATE TABLE `severities` (
  `severity_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `id` bigint(20) unsigned NOT NULL,
  `type` tinyint(4) unsigned NOT NULL COMMENT '0=service, 1=host',
  `name` varchar(255) NOT NULL,
  `level` int(11) unsigned NOT NULL,
  `icon_id` bigint(20) unsigned NOT NULL,
  PRIMARY KEY (`severity_id`),
  UNIQUE KEY `severities_id_type_uindex` (`id`,`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


--
-- Table structure for table `tags`
--
DROP TABLE IF EXISTS `tags`;
CREATE TABLE `tags` (
  `tag_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `id` bigint(20) unsigned NOT NULL,
  `type` tinyint(3) unsigned NOT NULL COMMENT '0=servicegroup, 1=hostgroup, 2=servicecategory, 3=hostcategory',
  `name` varchar(255) NOT NULL,
  PRIMARY KEY (`tag_id`),
  UNIQUE KEY `tags_id_type_uindex` (`id`,`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Table structure for table `resources_tags`
--
DROP TABLE IF EXISTS `resources_tags`;
CREATE TABLE `resources_tags` (
  `tag_id` bigint(20) unsigned NOT NULL,
  `resource_id` bigint(20) unsigned NOT NULL,
  KEY `resources_tags_resources_resource_id_fk` (`resource_id`),
  KEY `resources_tags_tag_id_fk` (`tag_id`),
  CONSTRAINT `resources_tags_resources_resource_id_fk` FOREIGN KEY (`resource_id`) REFERENCES `resources` (`resource_id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `resources_tags_tag_id_fk` FOREIGN KEY (`tag_id`) REFERENCES `tags` (`tag_id`) ON DELETE CASCADE ON UPDATE CASCADE,
  UNIQUE KEY `resources_tags_unique` (`tag_id`,`resource_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Table structure for table `resources`
--
DROP TABLE IF EXISTS `resources`;
CREATE TABLE `resources` (
  `resource_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `id` bigint(20) unsigned NOT NULL,
  `parent_id` bigint(20) unsigned NOT NULL,
  `internal_id` bigint(20) unsigned DEFAULT NULL COMMENT 'id of linked metaservice or business-activity',
  `type` tinyint(3) unsigned NOT NULL COMMENT '0=service, 1=host, 2=metaservice, 3=business-activity',
  `status` tinyint(3) unsigned DEFAULT NULL COMMENT 'service: 0=OK, 1=WARNING, 2=CRITICAL, 3=UNKNOWN, 4=PENDING\nhost: 0=UP, 1=DOWN, 2=UNREACHABLE, 4=PENDING',
  `status_ordered` tinyint(3) unsigned DEFAULT NULL COMMENT '0=OK=UP\n1=PENDING\n2=UNKNOWN=UNREACHABLE\n3=WARNING\n4=CRITICAL=DOWN',
  `last_status_change` bigint(20) unsigned DEFAULT NULL COMMENT 'the last status change timestamp',
  `in_downtime` tinyint(1) NOT NULL DEFAULT 0 COMMENT '0=false, 1=true',
  `acknowledged` tinyint(1) NOT NULL DEFAULT 0 COMMENT '0=false, 1=true',
  `status_confirmed` tinyint(1) DEFAULT NULL COMMENT '0=FALSE=SOFT\n1=TRUE=HARD',
  `check_attempts` smallint unsigned DEFAULT NULL,
  `max_check_attempts` smallint unsigned DEFAULT NULL,
  `poller_id` bigint(20) unsigned NOT NULL,
  `severity_id` bigint(20) unsigned DEFAULT NULL,
  `name` varchar(255) DEFAULT NULL,
  `alias` varchar(255) DEFAULT NULL,
  `address` varchar(255) DEFAULT NULL,
  `parent_name` varchar(255) DEFAULT NULL,
  `icon_id` bigint(20) unsigned DEFAULT NULL,
  `notes_url` varchar(2048) DEFAULT NULL,
  `notes` varchar(512) DEFAULT NULL,
  `action_url` varchar(2048) DEFAULT NULL,
  `has_graph` tinyint(1) NOT NULL DEFAULT 0 COMMENT '0=false, 1=true',
  `notifications_enabled` tinyint(1) NOT NULL DEFAULT 0 COMMENT '0=false, 1=true',
  `passive_checks_enabled` tinyint(1) NOT NULL DEFAULT 0 COMMENT '0=false, 1=true',
  `active_checks_enabled` tinyint(1) NOT NULL DEFAULT 0 COMMENT '0=false, 1=true',
  `last_check_type` tinyint(3) unsigned NOT NULL DEFAULT 0 COMMENT '0=active check, 1=passive check',
  `last_check` bigint(20) unsigned DEFAULT NULL COMMENT 'the last check timestamp',
  `output` text DEFAULT NULL,
  `enabled` tinyint(1) NOT NULL DEFAULT 1 COMMENT '0=false, 1=true',
  `flapping` tinyint(1) DEFAULT NULL,
  `percent_state_change` double DEFAULT NULL,
  PRIMARY KEY (`resource_id`),
  UNIQUE KEY `resources_id_parent_id_type_uindex` (`id`,`parent_id`,`type`),
  KEY `resources_severities_severity_id_fk` (`severity_id`),
  CONSTRAINT `resources_severities_severity_id_fk` FOREIGN KEY (`severity_id`) REFERENCES `severities` (`severity_id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- BA / Group relations.
--
DROP TABLE IF EXISTS mod_bam_bagroup_ba_relation;
CREATE TABLE mod_bam_bagroup_ba_relation (
  id_bgr int NOT NULL auto_increment,
  id_ba int NOT NULL,
  id_ba_group int NOT NULL,

  PRIMARY KEY (id_bgr),
  FOREIGN KEY (id_ba) REFERENCES mod_bam (ba_id)
    ON DELETE CASCADE,
  FOREIGN KEY (id_ba_group) REFERENCES mod_bam_ba_groups (id_ba_group)
    ON DELETE CASCADE
) ENGINE=InnoDB CHARACTER SET utf8;
--
-- Impacts of KPI / boolean expressions.
--
DROP TABLE IF EXISTS mod_bam_impacts;
CREATE TABLE mod_bam_impacts (
  id_impact int NOT NULL auto_increment,
  impact float NOT NULL,

  PRIMARY KEY (id_impact)
) ENGINE=InnoDB CHARACTER SET utf8;
--
-- Key Performance Indicators.
--
DROP TABLE IF EXISTS mod_bam_kpi;
CREATE TABLE mod_bam_kpi (
  kpi_id int NOT NULL auto_increment,

  state_type enum('0','1') default NULL,
  kpi_type enum('0','1','2','3') NOT NULL default '0',
  host_id int default NULL,
  service_id int default NULL,
  id_indicator_ba int default NULL,
  id_ba int default NULL,
  meta_id int default NULL,
  boolean_id int default NULL,
  current_status smallint default NULL,
  last_level float default NULL,
  downtime float default NULL,
  acknowledged float default NULL,
  config_type enum('0', '1'),
  drop_warning float default NULL,
  drop_warning_impact_id int default NULL,
  drop_critical float default NULL,
  drop_critical_impact_id int default NULL,
  drop_unknown float default NULL,
  drop_unknown_impact_id int default NULL,
  activate enum('0','1') default '0',
  ignore_downtime enum('0','1') default '0',
  ignore_acknowledged enum('0','1') default '0',
  last_state_change int default NULL,
  in_downtime boolean default NULL,
  last_impact float default NULL,
  valid boolean NOT NULL default 1,

  PRIMARY KEY (kpi_id),
  FOREIGN KEY (id_indicator_ba) REFERENCES mod_bam (ba_id)
    ON DELETE CASCADE,
  FOREIGN KEY (id_ba) REFERENCES mod_bam (ba_id)
    ON DELETE CASCADE,
  FOREIGN KEY (drop_warning_impact_id) REFERENCES mod_bam_impacts (id_impact)
    ON DELETE RESTRICT,
  FOREIGN KEY (drop_critical_impact_id) REFERENCES mod_bam_impacts (id_impact)
    ON DELETE RESTRICT,
  FOREIGN KEY (drop_unknown_impact_id) REFERENCES mod_bam_impacts (id_impact)
    ON DELETE RESTRICT
) ENGINE=InnoDB CHARACTER SET utf8;
--
-- BA/poller relation table.
--
DROP TABLE IF EXISTS mod_bam_poller_relations;
CREATE TABLE mod_bam_poller_relations (
  ba_id int NOT NULL,
  poller_id int NOT NULL,

  FOREIGN KEY (ba_id) REFERENCES mod_bam (ba_id)
    ON DELETE CASCADE
) ENGINE=InnoDB CHARACTER SET utf8;
--
-- BA / Timeperiod relations.
--
DROP TABLE IF EXISTS mod_bam_relations_ba_timeperiods;
CREATE TABLE mod_bam_relations_ba_timeperiods (
  ba_id int NOT NULL,
  tp_id int NOT NULL,

  FOREIGN KEY (ba_id) REFERENCES mod_bam (ba_id)
    ON DELETE CASCADE,
  FOREIGN KEY (tp_id) REFERENCES timeperiod (tp_id)
    ON DELETE CASCADE
) ENGINE=InnoDB CHARACTER SET utf8;

-- 
-- Structure de la table `mod_bam_reporting`
-- 
DROP TABLE IF EXISTS mod_bam_reporting;
CREATE TABLE `mod_bam_reporting` (
  `log_id` int(11) NOT NULL auto_increment,
  `host_name` varchar(255) NOT NULL default '0',
  `service_description` varchar(255) NOT NULL default '0',
  `OKTimeScheduled` int(11) NOT NULL default '0',
  `OKnbEvent` int(11) NOT NULL default '0',
  `OKTimeAverageAck` int(11) NOT NULL,
  `OKTimeAverageRecovery` int(11) NOT NULL,
  `WARNINGTimeScheduled` int(11) NOT NULL default '0',
  `WARNINGnbEvent` int(11) NOT NULL default '0',
  `WARNINGTimeAverageAck` int(11) NOT NULL,
  `WARNINGTimeAverageRecovery` int(11) NOT NULL,
  `UNKNOWNTimeScheduled` int(11) NOT NULL default '0',
  `UNKNOWNnbEvent` int(11) NOT NULL default '0',
  `UNKNOWNTimeAverageAck` int(11) NOT NULL,
  `UNKNOWNTimeAverageRecovery` int(11) NOT NULL,
  `CRITICALTimeScheduled` int(11) NOT NULL default '0',
  `CRITICALnbEvent` int(11) NOT NULL default '0',
  `CRITICALTimeAverageAck` int(11) NOT NULL,
  `CRITICALTimeAverageRecovery` int(11) NOT NULL,
  `UNDETERMINEDTimeScheduled` int(11) NOT NULL default '0',
  `date_start` int(11) default NULL,
  `date_end` int(11) default NULL,
  PRIMARY KEY  (`log_id`),
  KEY `date_end_index` (`date_end`),
  KEY `date_start_index` (`date_start`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- 
-- Structure de la table `mod_bam_reporting_status`
-- 
DROP TABLE IF EXISTS mod_bam_reporting_status;
CREATE TABLE `mod_bam_reporting_status` (
  `id` int(11) NOT NULL,
  `host_name` varchar(255) default NULL,
  `service_description` varchar(255) default NULL,
  `status` varchar(255) default NULL,
  `ctime` int(11) default NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- 
-- Structure de la table `mod_bam_kpi_logs`
-- 

DROP TABLE IF EXISTS mod_bam_kpi_logs;
CREATE TABLE `mod_bam_kpi_logs` (
  `kpi_id` int(11),
  `boolean_id` int(11),
  `ba_id` int(11),
  `status` smallint(6) NOT NULL,
  `ctime` int(11) NOT NULL,
  `output` varchar(255) NOT NULL,
  `kpi_name` varchar(255) NOT NULL,
  `kpi_type` enum('0', '1', '2', '3') NOT NULL,
  `impact` float NOT NULL default '0',
  `in_downtime` enum('0', '1') NOT NULL default '0',
  `downtime_flag` tinyint(1) NOT NULL default 0,
  `perfdata` text
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Business Views.
--
DROP TABLE IF EXISTS mod_bam_reporting_bv;
CREATE TABLE mod_bam_reporting_bv (
  bv_id int NOT NULL auto_increment,
  bv_name varchar(255) default NULL,

  bv_description text default NULL,

  PRIMARY KEY (bv_id),
  UNIQUE (bv_name)
) ENGINE=InnoDB CHARACTER SET utf8;

-- 
-- Structure de la table `mod_bam_logs`
-- 

DROP TABLE IF EXISTS mod_bam_logs;
CREATE TABLE `mod_bam_logs` (
  `status` varchar(255) NOT NULL,
  `level` float NOT NULL,
  `warning_thres` float NOT NULL,
  `critical_thres` float NOT NULL,
  `status_change_flag` enum('0','1') NOT NULL default '0',
  `ctime` int(11) NOT NULL,
  `ba_id` int(11) NOT NULL,
  `in_downtime` tinyint(1) NOT NULL default 0,
  `downtime_flag` tinyint(1) NOT NULL default 0,
  KEY `ba_id` (`ba_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Business Activities.
--
DROP TABLE IF EXISTS mod_bam_reporting_ba;
CREATE TABLE mod_bam_reporting_ba (
  ba_id int NOT NULL,
  ba_name varchar(254) default NULL,
  ba_description text default NULL,
  sla_month_percent_crit float default NULL,
  sla_month_percent_warn float default NULL,
  sla_month_duration_crit int default NULL,
  sla_month_duration_warn int default NULL,

  PRIMARY KEY (ba_id),
  UNIQUE (ba_name)
) ENGINE=InnoDB CHARACTER SET utf8;

--
-- Key Performance Indicators.
--
DROP TABLE IF EXISTS mod_bam_reporting_kpi;
CREATE TABLE mod_bam_reporting_kpi (
  kpi_id int NOT NULL,
  kpi_name varchar(255) default NULL,

  ba_id int default NULL,
  ba_name varchar(254) default NULL,
  host_id int default NULL,
  host_name varchar(255) default NULL,
  service_id int default NULL,
  service_description varchar(255) default NULL,
  kpi_ba_id int default NULL,
  kpi_ba_name varchar(254) default NULL,
  meta_service_id int default NULL,
  meta_service_name varchar(254) default NULL,
  boolean_id int default NULL,
  boolean_name varchar(255) default NULL,
  impact_warning float default NULL,
  impact_critical float default NULL,
  impact_unknown float default NULL,

  PRIMARY KEY (kpi_id),
  FOREIGN KEY (ba_id) REFERENCES mod_bam_reporting_ba (ba_id)
    ON DELETE CASCADE,
  FOREIGN KEY (kpi_ba_id) REFERENCES mod_bam_reporting_ba (ba_id)
    ON DELETE CASCADE
) ENGINE=InnoDB CHARACTER SET utf8;

--
-- Relations between BA and BV.
--
DROP TABLE IF EXISTS mod_bam_reporting_relations_ba_bv;
CREATE TABLE mod_bam_reporting_relations_ba_bv (
  ba_bv_id int NOT NULL auto_increment,
  bv_id int NOT NULL,
  ba_id int NOT NULL,

  PRIMARY KEY (ba_bv_id),
  FOREIGN KEY (bv_id) REFERENCES mod_bam_reporting_bv (bv_id)
    ON DELETE CASCADE,
  FOREIGN KEY (ba_id) REFERENCES mod_bam_reporting_ba (ba_id)
    ON DELETE CASCADE
) ENGINE=InnoDB CHARACTER SET utf8;

--
-- BA events.
--
DROP TABLE IF EXISTS mod_bam_reporting_ba_events;
CREATE TABLE mod_bam_reporting_ba_events (
  ba_event_id int NOT NULL auto_increment,
  ba_id int NOT NULL,
  start_time int NOT NULL,

  first_level double default NULL,
  end_time int default NULL,
  status tinyint default NULL,
  in_downtime boolean default NULL,

  KEY `ba_id_start_time_index` (`ba_id`, `start_time`),
  KEY `ba_id_end_time_index` (`ba_id`, `end_time`),

  PRIMARY KEY (ba_event_id)
) ENGINE=InnoDB CHARACTER SET utf8;

--
-- KPI events.
--
DROP TABLE IF EXISTS mod_bam_reporting_kpi_events;
CREATE TABLE mod_bam_reporting_kpi_events (
  kpi_event_id int NOT NULL auto_increment,
  kpi_id int NOT NULL,
  start_time int NOT NULL,

  end_time int default NULL,
  status tinyint default NULL,
  in_downtime boolean default NULL,
  impact_level tinyint default NULL,
  first_output text default NULL,
  first_perfdata varchar(45) default NULL,

  KEY `kpi_id_start_time_index` (`kpi_id`, `start_time`),

  PRIMARY KEY (kpi_event_id)
) ENGINE=InnoDB CHARACTER SET utf8;

--
-- Relations between BA events and KPI events.
--
DROP TABLE IF EXISTS mod_bam_reporting_relations_ba_kpi_events;
CREATE TABLE mod_bam_reporting_relations_ba_kpi_events (
  relation_id BIGINT NOT NULL auto_increment,
  ba_event_id int NOT NULL,
  kpi_event_id int NOT NULL,

  PRIMARY KEY (relation_id),
  FOREIGN KEY (ba_event_id) REFERENCES mod_bam_reporting_ba_events (ba_event_id)
    ON DELETE CASCADE,
  FOREIGN KEY (kpi_event_id) REFERENCES mod_bam_reporting_kpi_events (kpi_event_id)
    ON DELETE CASCADE
) ENGINE=InnoDB CHARACTER SET utf8;

--
-- BA availabilities.
--
DROP TABLE IF EXISTS mod_bam_reporting_ba_availabilities;
CREATE TABLE mod_bam_reporting_ba_availabilities (
  ba_id int NOT NULL,
  time_id int NOT NULL,
  timeperiod_id int NOT NULL,

  available int default NULL,
  unavailable int default NULL,
  degraded int default NULL,
  unknown int default NULL,
  downtime int default NULL,
  alert_unavailable_opened int default NULL,
  alert_degraded_opened int default NULL,
  alert_unknown_opened int default NULL,
  nb_downtime int default NULL,
  timeperiod_is_default boolean default NULL,

  UNIQUE (ba_id, time_id, timeperiod_id)
) ENGINE=InnoDB CHARACTER SET utf8;
--
-- BA events durations.
--
DROP TABLE IF EXISTS mod_bam_reporting_ba_events_durations;
CREATE TABLE mod_bam_reporting_ba_events_durations (
  ba_event_id int NOT NULL,
  timeperiod_id int NOT NULL,

  start_time int default NULL,
  end_time int default NULL,
  duration int default NULL,
  sla_duration int default NULL,
  timeperiod_is_default boolean default NULL,

  UNIQUE (ba_event_id, timeperiod_id),
  FOREIGN KEY (ba_event_id) REFERENCES mod_bam_reporting_ba_events (ba_event_id)
    ON DELETE CASCADE,
  KEY (end_time, start_time)
) ENGINE=InnoDB CHARACTER SET utf8;

--
-- BA/timeperiods relations.
--
DROP TABLE IF EXISTS mod_bam_reporting_relations_ba_timeperiods;
CREATE TABLE mod_bam_reporting_relations_ba_timeperiods (
  ba_id int default NULL,
  timeperiod_id int default NULL,
  is_default boolean default NULL,

  FOREIGN KEY (ba_id) REFERENCES mod_bam_reporting_ba (ba_id)
    ON DELETE CASCADE,
  FOREIGN KEY (timeperiod_id) REFERENCES mod_bam_reporting_timeperiods (timeperiod_id)
    ON DELETE CASCADE
) ENGINE=InnoDB CHARACTER SET utf8;
--
-- Timeperiods exceptions.
--
DROP TABLE IF EXISTS mod_bam_reporting_timeperiods_exceptions;
CREATE TABLE mod_bam_reporting_timeperiods_exceptions (
  timeperiod_id int NOT NULL,
  daterange varchar(255) NOT NULL,
  timerange varchar(255) NOT NULL,

  FOREIGN KEY (timeperiod_id) REFERENCES mod_bam_reporting_timeperiods (timeperiod_id)
    ON DELETE CASCADE
) ENGINE=InnoDB CHARACTER SET utf8;
--
-- Timeperiods exclusions.
--
DROP TABLE IF EXISTS mod_bam_reporting_timeperiods_exclusions;
CREATE TABLE mod_bam_reporting_timeperiods_exclusions (
  timeperiod_id int NOT NULL,
  excluded_timeperiod_id int NOT NULL,

  FOREIGN KEY (timeperiod_id) REFERENCES mod_bam_reporting_timeperiods (timeperiod_id)
    ON DELETE CASCADE,
  FOREIGN KEY (excluded_timeperiod_id) REFERENCES mod_bam_reporting_timeperiods (timeperiod_id)
    ON DELETE CASCADE
) ENGINE=InnoDB CHARACTER SET utf8;

--
-- Timeperiods.
--
DROP TABLE IF EXISTS mod_bam_reporting_timeperiods;
CREATE TABLE mod_bam_reporting_timeperiods (
  timeperiod_id int NOT NULL,
  name varchar(200) default NULL,
  sunday varchar(2048) default NULL,
  monday varchar(2048) default NULL,
  tuesday varchar(2048) default NULL,
  wednesday varchar(2048) default NULL,
  thursday varchar(2048) default NULL,
  friday varchar(2048) default NULL,
  saturday varchar(2048) default NULL,

  PRIMARY KEY (timeperiod_id)
) ENGINE=InnoDB CHARACTER SET utf8;

SET FOREIGN_KEY_CHECKS=1;

--
-- Agent CEIP
--
DROP TABLE IF EXISTS agent_information;
CREATE TABLE `centreon_storage`.`agent_information` (
  `poller_id` BIGINT(20) NOT NULL,
  `enabled` TINYINT(1) NOT NULL DEFAULT 1,
  `infos` JSON NOT NULL,

  PRIMARY KEY (`poller_id`),
  KEY(enabled)
) ENGINE=InnoDB CHARACTER SET utf8;

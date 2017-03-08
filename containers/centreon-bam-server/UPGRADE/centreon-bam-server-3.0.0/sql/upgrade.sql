-- #7554
ALTER TABLE mod_bam DROP FOREIGN KEY mod_bam_ibfk_2;
ALTER TABLE mod_bam DROP COLUMN id_check_period;

-- #7555
ALTER TABLE mod_bam ADD COLUMN must_be_rebuild enum('0','1','2') DEFAULT '0' AFTER acknowledged;
ALTER TABLE mod_bam ADD COLUMN last_state_change int default NULL AFTER must_be_rebuild;
ALTER TABLE mod_bam ADD COLUMN current_status tinyint default NULL AFTER last_state_change;
ALTER TABLE mod_bam ADD COLUMN in_downtime boolean default NULL AFTER current_status;


-- #7552
CREATE TABLE IF NOT EXISTS mod_bam_relations_ba_timeperiods (
	`ba_id` int(11) NOT NULL,
	`tp_id` int(11) NOT NULL,
	KEY `ba_id` (`ba_id`),
	KEY `tp_id` (`tp_id`),
	CONSTRAINT `mod_bam_relations_ba_timeperiods_ibfk_1` FOREIGN KEY (`ba_id`) REFERENCES `mod_bam` (`ba_id`) ON DELETE CASCADE,
	CONSTRAINT `mod_bam_relations_ba_timeperiods_ibfk_2` FOREIGN KEY (`tp_id`) REFERENCES `timeperiod` (`tp_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- #7563
ALTER TABLE mod_bam DROP COLUMN sla_type;
ALTER TABLE mod_bam DROP COLUMN sla_warning;
ALTER TABLE mod_bam DROP COLUMN sla_critical;
ALTER TABLE mod_bam ADD COLUMN sla_month_percent_warn float default NULL AFTER level_c;
ALTER TABLE mod_bam ADD COLUMN sla_month_percent_crit float default NULL AFTER sla_month_percent_warn;
ALTER TABLE mod_bam ADD COLUMN sla_month_duration_warn float default NULL AFTER sla_month_percent_crit;
ALTER TABLE mod_bam ADD COLUMN sla_month_duration_crit float default NULL AFTER sla_month_duration_warn;

-- #7556
ALTER TABLE mod_bam_kpi ADD COLUMN boolean_id int(11) default NULL AFTER meta_id;
ALTER TABLE mod_bam_kpi ADD CONSTRAINT `mod_bam_kpi_ibfk_8` FOREIGN KEY (`boolean_id`) REFERENCES `mod_bam_boolean` (`boolean_id`);
ALTER TABLE mod_bam_kpi MODIFY COLUMN kpi_type enum('0','1','2','3');
INSERT INTO mod_bam_kpi (id_ba, boolean_id, config_type, drop_critical, drop_critical_impact_id, current_status, kpi_type)
		(
          SELECT rel.ba_id, b.boolean_id, b.config_type, impact, impact_id, current_state, '3'
  		  FROM mod_bam_boolean b, mod_bam_bool_rel rel
		  WHERE b.boolean_id = rel.boolean_id
        );

UPDATE mod_bam_kpi SET drop_warning_impact_id = (SELECT id_impact FROM mod_bam_impacts WHERE code = mod_bam_kpi.drop_warning_impact_id);
UPDATE mod_bam_kpi SET drop_critical_impact_id = (SELECT id_impact FROM mod_bam_impacts WHERE code = mod_bam_kpi.drop_critical_impact_id);
UPDATE mod_bam_kpi SET drop_unknown_impact_id = (SELECT id_impact FROM mod_bam_impacts WHERE code = mod_bam_kpi.drop_unknown_impact_id);

ALTER TABLE mod_bam_kpi ADD COLUMN last_impact float default NULL AFTER last_level;

ALTER TABLE mod_bam_boolean DROP COLUMN config_type;
ALTER TABLE mod_bam_boolean DROP COLUMN impact;
ALTER TABLE mod_bam_boolean DROP COLUMN impact_id;
ALTER TABLE mod_bam_boolean DROP COLUMN current_state;
ALTER TABLE mod_bam_kpi ADD COLUMN last_state_change int default NULL AFTER ignore_acknowledged;
ALTER TABLE mod_bam_kpi ADD COLUMN in_downtime boolean default NULL AFTER last_state_change;

DROP TABLE mod_bam_bool_rel;

INSERT INTO `topology` (`topology_name`, `topology_icone`, `topology_parent`, `topology_page`, `topology_order`, `topology_group`, `topology_url`, `topology_url_opt`, `topology_popup`, `topology_modules`, `topology_show`) 
VALUES  ('Boolean Rules', './modules/centreon-bam-server/core/common/images/colors.gif', '207', '20711', '40', '2', './modules/centreon-bam-server/core/configuration/boolean/configuration_boolean.php', NULL, NULL, '1', '1');


-- #7557
--
-- Business Views.
--
CREATE TABLE @DB_CENTSTORAGE@.mod_bam_reporting_bv (
  bv_id int NOT NULL auto_increment,
  bv_name varchar(45) default NULL,

  bv_description text default NULL,

  PRIMARY KEY (bv_id),
  UNIQUE (bv_name)
) ENGINE=InnoDB CHARACTER SET utf8;

--
-- Business Activities.
--
CREATE TABLE @DB_CENTSTORAGE@.mod_bam_reporting_ba (
  ba_id int NOT NULL,
  ba_name varchar(45) default NULL,

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
CREATE TABLE @DB_CENTSTORAGE@.mod_bam_reporting_kpi (
  kpi_id int NOT NULL,
  kpi_name varchar(45) default NULL,

  ba_id int default NULL,
  ba_name varchar(45) default NULL,
  host_id int default NULL,
  host_name varchar(45) default NULL,
  service_id int default NULL,
  service_description varchar(45) default NULL,
  kpi_ba_id int default NULL,
  kpi_ba_name varchar(45) default NULL,
  meta_service_id int default NULL,
  meta_service_name varchar(45),
  boolean_id int default NULL,
  boolean_name varchar(45),
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
CREATE TABLE @DB_CENTSTORAGE@.mod_bam_reporting_relations_ba_bv (
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
CREATE TABLE @DB_CENTSTORAGE@.mod_bam_reporting_ba_events (
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
CREATE TABLE @DB_CENTSTORAGE@.mod_bam_reporting_kpi_events (
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
CREATE TABLE @DB_CENTSTORAGE@.mod_bam_reporting_relations_ba_kpi_events (
  ba_event_id int NOT NULL,
  kpi_event_id int NOT NULL,

  FOREIGN KEY (ba_event_id) REFERENCES mod_bam_reporting_ba_events (ba_event_id)
    ON DELETE CASCADE,
  FOREIGN KEY (kpi_event_id) REFERENCES mod_bam_reporting_kpi_events (kpi_event_id)
    ON DELETE CASCADE
) ENGINE=InnoDB CHARACTER SET utf8;

--
-- Timeperiods.
--
CREATE TABLE @DB_CENTSTORAGE@.mod_bam_reporting_timeperiods (
  timeperiod_id int NOT NULL,
  name varchar(200) default NULL,
  sunday varchar(200) default NULL,
  monday varchar(200) default NULL,
  tuesday varchar(200) default NULL,
  wednesday varchar(200) default NULL,
  thursday varchar(200) default NULL,
  friday varchar(200) default NULL,
  saturday varchar(200) default NULL,

  PRIMARY KEY (timeperiod_id)
) ENGINE=InnoDB CHARACTER SET utf8;

--
-- Timeperiods exceptions.
--
CREATE TABLE @DB_CENTSTORAGE@.mod_bam_reporting_timeperiods_exceptions (
  timeperiod_id int NOT NULL,
  daterange varchar(255) NOT NULL,
  timerange varchar(255) NOT NULL,

  FOREIGN KEY (timeperiod_id) REFERENCES mod_bam_reporting_timeperiods (timeperiod_id)
    ON DELETE CASCADE
) ENGINE=InnoDB CHARACTER SET utf8;

--
-- Timeperiods exclusions.
--
CREATE TABLE @DB_CENTSTORAGE@.mod_bam_reporting_timeperiods_exclusions (
  timeperiod_id int NOT NULL,
  excluded_timeperiod_id int NOT NULL,

  FOREIGN KEY (timeperiod_id) REFERENCES mod_bam_reporting_timeperiods (timeperiod_id)
    ON DELETE CASCADE,
  FOREIGN KEY (excluded_timeperiod_id) REFERENCES mod_bam_reporting_timeperiods (timeperiod_id)
    ON DELETE CASCADE
) ENGINE=InnoDB CHARACTER SET utf8;

--
-- BA/timeperiods relations.
--
CREATE TABLE @DB_CENTSTORAGE@.mod_bam_reporting_relations_ba_timeperiods (
  ba_id int default NULL,
  timeperiod_id int default NULL,
  is_default boolean default NULL,

  FOREIGN KEY (ba_id) REFERENCES mod_bam_reporting_ba (ba_id)
    ON DELETE CASCADE,
  FOREIGN KEY (timeperiod_id) REFERENCES mod_bam_reporting_timeperiods (timeperiod_id)
    ON DELETE CASCADE
) ENGINE=InnoDB CHARACTER SET utf8;

--
-- BA events durations.
--
CREATE TABLE @DB_CENTSTORAGE@.mod_bam_reporting_ba_events_durations (
  ba_event_id int NOT NULL,
  timeperiod_id int NOT NULL,

  start_time int default NULL,
  end_time int default NULL,
  duration int default NULL,
  sla_duration int default NULL,
  timeperiod_is_default boolean default NULL,

  KEY `end_time_start_time_index` (`end_time`, `start_time`),

  UNIQUE (ba_event_id, timeperiod_id),
  FOREIGN KEY (ba_event_id) REFERENCES mod_bam_reporting_ba_events (ba_event_id)
    ON DELETE CASCADE
) ENGINE=InnoDB CHARACTER SET utf8;

--
-- BA availabilities.
--
CREATE TABLE @DB_CENTSTORAGE@.mod_bam_reporting_ba_availabilities (
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

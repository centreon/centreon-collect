TRUNCATE @DB_CENTSTORAGE@.`mod_bam_kpi_logs`;

ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_logs` ADD COLUMN in_downtime tinyint(1) NOT NULL default 0;
ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_logs` ADD COLUMN downtime_flag tinyint(1) NOT NULL default 0;
ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_logs` ADD INDEX ba_id (`ba_id`);

ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_logs` DROP COLUMN log_id;

ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_kpi_logs` DROP COLUMN kpi_log_id;
ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_kpi_logs` DROP COLUMN log_id;

ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_kpi_logs` ADD COLUMN kpi_id int(11) FIRST;
ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_kpi_logs` ADD COLUMN boolean_id int(11) AFTER kpi_id;
ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_kpi_logs` ADD COLUMN ba_id int(11) AFTER kpi_id;
ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_kpi_logs` ADD COLUMN downtime_flag tinyint(1) NOT NULL default 0;
ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_kpi_logs` ADD COLUMN perfdata text;

ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_kpi_logs`
  ADD CONSTRAINT `mod_bam_kpi_log_fk` FOREIGN KEY (`ba_id`) REFERENCES @DB_CENTSTORAGE@.`mod_bam_logs` (`ba_id`) ON DELETE CASCADE;

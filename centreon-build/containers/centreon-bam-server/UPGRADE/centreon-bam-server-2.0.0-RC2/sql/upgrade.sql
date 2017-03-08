ALTER TABLE `mod_bam` MODIFY downtime int(11);

CREATE TABLE @DB_CENTSTORAGE@.`mod_bam_kpi_logs` (
  `kpi_log_id` int(11) NOT NULL auto_increment,
  `status` smallint(6) NOT NULL,
  `ctime` int(11) NOT NULL,
  `output` varchar(255) NOT NULL,
  `kpi_name` varchar(255) NOT NULL,
  `kpi_type` enum('0', '1', '2') NOT NULL,
  `log_id` int(11) NOT NULL,
  PRIMARY KEY  (`kpi_log_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `data_bin` (
  `id_metric` int(11) DEFAULT NULL,
  `ctime` int(11) DEFAULT NULL,
  `value` float DEFAULT NULL,
  `status` enum('0','1','2','3','4') DEFAULT NULL,
  KEY `index_metric` (`id_metric`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

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
  KEY (`log_id`),
  KEY `host_index` (`host_id`),
  KEY `service_index` (`service_id`),
  KEY `date_end_index` (`date_end`),
  KEY `date_start_index` (`date_start`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
    
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

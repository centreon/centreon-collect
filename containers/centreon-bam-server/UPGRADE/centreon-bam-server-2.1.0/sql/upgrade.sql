DELETE FROM `topology` WHERE topology_page = '30102' AND topology_name = 'Reporting';
DELETE FROM `topology` WHERE topology_page = '30105' AND topology_name = 'Business Activity';
DELETE FROM `topology` WHERE topology_page = '30109' AND topology_name = 'Performances';

INSERT INTO `topology` (`topology_id`, `topology_name`, `topology_icone`, `topology_parent`, `topology_page`, `topology_order`, `topology_group`, `topology_url`, `topology_url_opt`, `topology_popup`, `topology_modules`, `topology_show`) VALUES
('', 'Reporting', './modules/centreon-bam-server/core/common/images/reporting.gif', '207', '20702', '20', '1', './modules/centreon-bam-server/core/reporting/reporting.php', NULL, NULL, '1', '1'),
('', 'Business Activity', './modules/centreon-bam-server/core/common/images/tool.gif', '207', '20705', '20', '2', './modules/centreon-bam-server/core/configuration/ba/configuration_ba.php', NULL, NULL, '1', '1'),
('', 'Performances', './modules/centreon-bam-server/core/common/images/line-chart.gif', '207', '20709', '30', '1', './modules/centreon-bam-server/core/rrdgraph/rrdgraph.php', NULL, NULL, '1', '1');

INSERT INTO `topology_JS` (`id_t_js`, `id_page`, `o`, `PathName_js`, `Init`) VALUES 
(NULL , '20705', NULL , './include/common/javascript/changetab.js ', 'initChangeTab'),
(NULL , '20709', NULL , './include/common/javascript/codebase/dhtmlxtree.js', NULL),
(NULL , '20709', NULL , './include/common/javascript/codebase/dhtmlxcommon.js', NULL),
(NULL , '20702', NULL , './include/common/javascript/Timeline/src/main/webapp/api/timeline-api.js', 'initTimeline');


UPDATE `topology` SET topology_parent = '2', topology_page = '207', topology_order = '30', topology_url = './modules/centreon-bam-server/core/dashboard/dashboard.php' WHERE topology_name = 'Business Activity' AND topology_page = '301';

UPDATE `topology` SET topology_parent = '207' WHERE topology_name = 'Views' AND topology_parent = '301';
UPDATE `topology` SET topology_parent = '207', topology_page = '20701' WHERE topology_name = 'Monitoring' AND topology_parent = '301';
UPDATE `topology` SET topology_parent = '207', topology_page = '20703' WHERE topology_name = 'Logs' AND topology_parent = '301';

UPDATE `topology` SET topology_parent = '207' WHERE topology_name = 'Management' AND topology_parent = '301';
UPDATE `topology` SET topology_parent = '207', topology_page = '20704' WHERE topology_name = 'Business Views' AND topology_parent = '301';
UPDATE `topology` SET topology_parent = '207', topology_page = '20706' WHERE topology_name = 'Indicators' AND topology_parent = '301';

UPDATE `topology` SET topology_parent = '207' WHERE topology_name = 'Options' AND topology_parent = '301';
UPDATE `topology` SET topology_parent = '207', topology_page = '20707' WHERE topology_name = 'Default Settings' AND topology_parent = '301';
UPDATE `topology` SET topology_parent = '207', topology_page = '20708' WHERE topology_name = 'User Settings' AND topology_parent = '301';

INSERT INTO `topology` (`topology_id`, `topology_name`, `topology_icone`, `topology_parent`, `topology_page`, `topology_order`, `topology_group`, `topology_url`, `topology_url_opt`, `topology_popup`, `topology_modules`, `topology_show`) VALUES
('', 'Help', NULL, '207', NULL, NULL, '4', NULL, NULL, '0', '1', '1'),
('', 'Troubleshooter', './modules/centreon-bam-server/core/common/images/help.gif', '207', '20710', '10', '4', './modules/centreon-bam-server/core/help/troubleshooter/troubleshooter.php', NULL, NULL, '1', '1');


ALTER TABLE `mod_bam_kpi` MODIFY last_level float;
ALTER TABLE `mod_bam_kpi` MODIFY downtime float;
ALTER TABLE `mod_bam_kpi` MODIFY drop_warning float;
ALTER TABLE `mod_bam_kpi` MODIFY drop_critical float;
ALTER TABLE `mod_bam_kpi` MODIFY drop_unknown float;

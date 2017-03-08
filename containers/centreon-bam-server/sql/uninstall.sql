DROP TABLE `mod_bam_relations_ba_timeperiods`;
DROP TABLE `mod_bam_acl`;
DROP TABLE `mod_bam_bagroup_ba_relation`;
DROP TABLE `mod_bam_cg_relation`;
DROP TABLE `mod_bam_dep_child_relation`;
DROP TABLE `mod_bam_dep_parent_relation`;
DROP TABLE `mod_bam_escal_relation`;
DROP TABLE `mod_bam_kpi`;
DROP TABLE `mod_bam_boolean`;
DROP TABLE `mod_bam_ba_groups`;
DROP TABLE `mod_bam_user_preferences`;
DROP TABLE `mod_bam_impacts`;
DROP TABLE `mod_bam_user_overview_relation`;
DROP TABLE `mod_bam_poller_relations`;
DROP TABLE `mod_bam`;

DROP TABLE @DB_CENTSTORAGE@.`mod_bam_reporting`;
DROP TABLE @DB_CENTSTORAGE@.`mod_bam_reporting_status`;
DROP TABLE @DB_CENTSTORAGE@.`mod_bam_kpi_logs`;
DROP TABLE @DB_CENTSTORAGE@.`mod_bam_logs`;

DELETE FROM @DB_CENTSTORAGE@.`index_data` WHERE host_name LIKE '_Module_BAM%';
DELETE FROM host WHERE host_name LIKE '_Module_BAM%';
DELETE FROM @DB_CENTSTORAGE@.hosts WHERE name LIKE '_Module_BAM%';

DROP TABLE @DB_CENTSTORAGE@.mod_bam_reporting_relations_ba_timeperiods;
DROP TABLE @DB_CENTSTORAGE@.mod_bam_reporting_timeperiods_exclusions;
DROP TABLE @DB_CENTSTORAGE@.mod_bam_reporting_timeperiods_exceptions;
DROP TABLE @DB_CENTSTORAGE@.mod_bam_reporting_timeperiods;
DROP TABLE @DB_CENTSTORAGE@.mod_bam_reporting_ba_availabilities;
DROP TABLE @DB_CENTSTORAGE@.mod_bam_reporting_ba_events_durations;
DROP TABLE @DB_CENTSTORAGE@.mod_bam_reporting_relations_ba_kpi_events;
DROP TABLE @DB_CENTSTORAGE@.mod_bam_reporting_kpi_events;
DROP TABLE @DB_CENTSTORAGE@.mod_bam_reporting_ba_events;
DROP TABLE @DB_CENTSTORAGE@.mod_bam_reporting_relations_ba_bv;
DROP TABLE @DB_CENTSTORAGE@.mod_bam_reporting_kpi;
DROP TABLE @DB_CENTSTORAGE@.mod_bam_reporting_ba;
DROP TABLE @DB_CENTSTORAGE@.mod_bam_reporting_bv;


DELETE FROM topology WHERE topology_page = '207';
DELETE FROM topology WHERE topology_page = '20701';
DELETE FROM topology WHERE topology_page = '20702';
DELETE FROM topology WHERE topology_page = '20703';
DELETE FROM topology WHERE topology_page = '20704';
DELETE FROM topology WHERE topology_page = '20705';
DELETE FROM topology WHERE topology_page = '20706';
DELETE FROM topology WHERE topology_page = '20707';
DELETE FROM topology WHERE topology_page = '20708';
DELETE FROM topology WHERE topology_page = '20710';
DELETE FROM topology WHERE topology_page = '20711';
DELETE FROM topology WHERE topology_parent = '207';

DELETE FROM topology WHERE topology_page = '626';
DELETE FROM topology WHERE topology_page = '62604';
DELETE FROM topology WHERE topology_page = '62605';
DELETE FROM topology WHERE topology_page = '62606';
DELETE FROM topology WHERE topology_page = '62607';
DELETE FROM topology WHERE topology_page = '62608';
DELETE FROM topology WHERE topology_page = '62610';
DELETE FROM topology WHERE topology_page = '62611';
DELETE FROM topology WHERE topology_parent = '626';

DELETE FROM topology_JS WHERE id_page = '20701';
DELETE FROM topology_JS WHERE id_page = '20702';
DELETE FROM topology_JS WHERE id_page = '20705'; 
DELETE FROM topology_JS WHERE id_page = '62605';

DELETE FROM `giv_graphs_template` WHERE name = 'Centreon BAM';
DELETE FROM `giv_components_template` WHERE name = 'BA_Level';
DELETE FROM `giv_components_template` WHERE name = 'BA_Downtime';
DELETE FROM `giv_components_template` WHERE name = 'BA_Acknowledgement';

DELETE FROM command WHERE command_name = 'bam-notify-by-email';

DROP VIEW IF EXISTS `mod_bam_view_kpi`;


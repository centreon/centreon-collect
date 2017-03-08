INSERT INTO `widget_parameters_field_type` (`ft_typename`, `is_connector`) VALUES
  ('ba', '1'),
  ('bv', '1');
  
-- Create table for relation between BA and Poller
CREATE TABLE mod_bam_poller_relations (
  ba_id int NOT NULL,
  poller_id int NOT NULL,
  FOREIGN KEY (ba_id) REFERENCES mod_bam (ba_id)
    ON DELETE CASCADE,
  FOREIGN KEY (poller_id) REFERENCES nagios_server (id)
    ON DELETE CASCADE
) ENGINE=InnoDB CHARACTER SET utf8;

-- Add field valid for kpi
ALTER TABLE mod_bam_kpi ADD COLUMN valid BOOLEAN NOT NULL DEFAULT 1;

-- Add localhost poller for all BA in relation
INSERT INTO mod_bam_poller_relations(ba_id, poller_id)
  SELECT b.ba_id, ns.id
    FROM mod_bam b, nagios_server ns
    WHERE ns.localhost = '1';

-- TOPOLOGY AND TOPOLOGY_JS
DELETE FROM `topology_JS` WHERE `id_page`=20705;

INSERT INTO `topology` (`topology_id`, `topology_name`, `topology_icone`, `topology_parent`, `topology_page`, `topology_order`, `topology_group`, `topology_url`, `topology_url_opt`, `topology_popup`, `topology_modules`, `topology_show`) VALUES
('', 'Business Activity', NULL, '6', '626', '20', '1', './modules/centreon-bam-server/core/configuration/group/configuration_ba_group.php', NULL, '0', '1', '1');

UPDATE `topology` SET `topology_parent`='626', `topology_group`='1' WHERE `topology_name`='Management' AND `topology_parent`='207';
UPDATE `topology` SET `topology_parent`='626', `topology_page`='62604', `topology_group`='1' WHERE `topology_name`='Business Views' AND `topology_page`='20704';
UPDATE `topology` SET `topology_parent`='626', `topology_page`='62605', `topology_group`='1' WHERE `topology_name`='Business Activity' AND `topology_page`='20705';
UPDATE `topology` SET `topology_parent`='626', `topology_page`='62606', `topology_group`='1' WHERE `topology_name`='Indicators' AND `topology_page`='20706';
UPDATE `topology` SET `topology_parent`='626', `topology_page`='62611', `topology_group`='1' WHERE `topology_name`='Boolean Rules' AND `topology_page`='20711';

UPDATE `topology` SET `topology_parent`='626', `topology_group`='2' WHERE `topology_name`='Options' AND `topology_parent`='207';
UPDATE `topology` SET `topology_parent`='626', `topology_page`='62607', `topology_group`='2' WHERE `topology_name`='Default Settings' AND `topology_page`='20707';
UPDATE `topology` SET `topology_parent`='626', `topology_page`='62608', `topology_group`='2' WHERE `topology_name`='User Settings' AND `topology_page`='20708';

UPDATE `topology` SET `topology_parent`='626', `topology_group`='3' WHERE `topology_name`='Help' AND `topology_parent`='207';
UPDATE `topology` SET `topology_parent`='626', `topology_page`='62610', `topology_group`='3' WHERE `topology_name`='Troubleshooter' AND `topology_page`='20710';

INSERT INTO `topology_JS` (`id_t_js`, `id_page`, `o`, `PathName_js`, `Init`) VALUES
(NULL , '62605', NULL , './include/common/javascript/changetab.js ', 'initChangeTab');


INSERT INTO `command` (`command_name`,`command_line`,`command_type`,`command_example`,`graph_id`) VALUES
('bam-notify-by-email',
'/usr/bin/printf \"%b\" \"***** Centreon BAM *****\\n\\nNotification Type: \$NOTIFICATIONTYPE\$\\n\\nBusiness Activity: \$SERVICEDISPLAYNAME\$\\nState: \$SERVICESTATE\$\\n\\nDate: \$DATE\$ \$TIME\$\\n\\nAdditional Info:\\n\\n\$SERVICEOUTPUT\$\" | \/usr\/bin\/mail -s \"** \$NOTIFICATIONTYPE\$ - \$SERVICEDISPLAYNAME\$ is \$SERVICESTATE\$ **\" \$CONTACTEMAIL\$',
1,'',0);


ALTER TABLE mod_bam_dep_child_relation ADD CONSTRAINT fk_id_ba_child FOREIGN KEY (`id_ba`) REFERENCES mod_bam(`ba_id`) ON DELETE CASCADE ON UPDATE NO ACTION;
ALTER TABLE mod_bam_dep_child_relation ADD CONSTRAINT fk_id_dep_child FOREIGN KEY (`id_dep`) REFERENCES dependency(`dep_id`) ON DELETE CASCADE ON UPDATE NO ACTION;



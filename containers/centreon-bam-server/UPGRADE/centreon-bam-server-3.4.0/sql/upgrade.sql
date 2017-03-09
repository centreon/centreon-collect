INSERT INTO `topology_JS` (`id_page`, `o`, `PathName_js`, `Init`) VALUES (62605,NULL,'./include/common/javascript/commandGetArgs/cmdGetExample.js',NULL);

INSERT INTO `topology_JS` (`id_page`, `o`, `PathName_js`, `Init`) VALUES (207,'d','./include/common/javascript/changetab.js','initChangeTab');
INSERT INTO `topology_JS` (`id_page`, `o`, `PathName_js`, `Init`) VALUES (20701,'d','./include/common/javascript/changetab.js','initChangeTab');

ALTER TABLE `mod_bam` ADD `event_handler_enabled` enum('0', '1') DEFAULT '0' AFTER `icon_id`;
ALTER TABLE `mod_bam` ADD `event_handler_command` int(11) DEFAULT NULL AFTER `event_handler_enabled`;
ALTER TABLE `mod_bam` ADD `event_handler_args` varchar(254) DEFAULT NULL AFTER `event_handler_command`;

ALTER TABLE `mod_bam`
  ADD CONSTRAINT FOREIGN KEY (`event_handler_command`) REFERENCES `command` (`cmd_id`) ON DELETE SET NULL,
  ADD CONSTRAINT FOREIGN KEY (`event_handler_args`) REFERENCES `command` (`cmd_id`) ON DELETE SET NULL;
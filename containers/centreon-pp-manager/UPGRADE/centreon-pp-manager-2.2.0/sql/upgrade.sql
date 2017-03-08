ALTER TABLE `mod_ppm_pluginpack` ADD `complete` bool DEFAULT true AFTER `icon`;
-- Creation of a new table with the categories for the ppm
CREATE TABLE mod_ppm_categories
(
`id` INT PRIMARY KEY NOT NULL AUTO_INCREMENT,
`name` VARCHAR(100) NOT NULL,
`parent_id` INT(11) DEFAULT NULL,
`icon` VARCHAR(100) NULL,
`priority` INT(11) NULL,
KEY `mpc_index` (`parent_id`),
CONSTRAINT `mpc_fk` FOREIGN KEY (`parent_id`) REFERENCES `mod_ppm_categories` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--Insert in mod_ppm_categories the categories
INSERT INTO mod_ppm_categories (`name`, `parent_id`, `icon`, `priority`)
VALUES
('Sensor', NULL, 'sensor.png', 1),
('Printer', NULL, 'printer.png', 2),
('Network', NULL, 'network.png', 3),
('Hardware-server', NULL, 'hardware-server.png', 4),
('Operating System', (SELECT ppmc.`id` FROM mod_ppm_categories ppmc WHERE ppmc.`name` = 'Hardware-server'), 'operating-system.png', 5),
('Database', (SELECT ppmc.`id` FROM mod_ppm_categories ppmc WHERE ppmc.`name` = 'Hardware-server'), 'database.png', 6),
('Centreon', (SELECT ppmc.`id` FROM mod_ppm_categories ppmc WHERE ppmc.`name` = 'Hardware-server'), 'centreon.png', 7),
('Applications', (SELECT ppmc.`id` FROM mod_ppm_categories ppmc WHERE ppmc.`name` = 'Hardware-server'), 'applications.png', 8),
('Storage', NULL, 'storage.png', 9),
('ToIp-VoIP', NULL, 'toip-voip.png', 10),
('UPS-PDU', NULL, 'ups-pdu.png', 11),
('Protocol', NULL, 'protocol.png', 12),
('Cloud', NULL, 'cloud.png', 13),
('Virtualization', NULL, 'virtualization.png', 14);

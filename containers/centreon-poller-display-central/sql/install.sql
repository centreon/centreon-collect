CREATE TABLE IF NOT EXISTS mod_poller_display_server_relations (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`nagios_server_id` int(11) NOT NULL,
	KEY `nagios_server_id` (`nagios_server_id`),
	PRIMARY KEY (`id`),
	CONSTRAINT `mod_poller_display_server_relations_ibfk_1` FOREIGN KEY (`nagios_server_id`) REFERENCES `nagios_server` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT INTO topology (topology_name, topology_parent, topology_page, topology_order, topology_group, topology_url, topology_popup, topology_modules, topology_show)
VALUES ('Poller display', 609, 60928, 60, 1, './modules/centreon-poller-display-central/core/configuration/form.php', '0', '1', '1');
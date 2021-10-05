DROP TABLE IF EXISTS `topology`;
CREATE TABLE `topology` (
  `topology_id` int(11),
  `topology_name` varchar(255) DEFAULT NULL,
  `topology_parent` int(11) DEFAULT NULL,
  `topology_page` int(11) DEFAULT NULL,
  `topology_order` int(11) DEFAULT NULL,
  `topology_group` int(11) DEFAULT NULL,
  `topology_url` varchar(255) DEFAULT NULL,
  `topology_url_opt` varchar(255) DEFAULT NULL,
  `topology_popup` TEXT DEFAULT NULL,
  `topology_modules` TEXT DEFAULT NULL,
  `topology_show` TEXT DEFAULT '1',
  `topology_style_class` varchar(255) DEFAULT NULL,
  `topology_style_id` varchar(255) DEFAULT NULL,
  `topology_OnClick` varchar(255) DEFAULT NULL,
  `readonly` TEXT NOT NULL DEFAULT '1',
  PRIMARY KEY (`topology_id`)
);

DROP TABLE IF EXISTS `topology_JS`;
CREATE TABLE `topology_JS` (
  `id_t_js` int(11),
  `id_page` int(11) DEFAULT NULL,
  `o` varchar(12) DEFAULT NULL,
  `PathName_js` text,
  `Init` text,
  PRIMARY KEY (`id_t_js`),
  CONSTRAINT `topology_JS_ibfk_1` FOREIGN KEY (`id_page`) REFERENCES `topology` (`topology_page`) ON DELETE CASCADE ON UPDATE CASCADE
);

DROP TABLE IF EXISTS `cb_field`;
CREATE TABLE `cb_field` (
  `cb_field_id` int(11),
  `fieldname` varchar(100) NOT NULL,
  `displayname` varchar(100) NOT NULL,
  `description` varchar(255) DEFAULT NULL,
  `fieldtype` varchar(255) NOT NULL DEFAULT 'text',
  `external` varchar(255) DEFAULT NULL,
  `cb_fieldgroup_id` INT DEFAULT NULL,
  PRIMARY KEY (`cb_field_id`)
);

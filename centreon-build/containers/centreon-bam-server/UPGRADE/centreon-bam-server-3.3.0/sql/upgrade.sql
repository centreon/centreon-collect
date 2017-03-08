INSERT INTO `widget_parameters_field_type` (`ft_typename`, `is_connector`)
SELECT * FROM (SELECT 'ba', '1') AS tmp
WHERE NOT EXISTS (SELECT `ft_typename` FROM `widget_parameters_field_type` WHERE `ft_typename` = 'ba' ) LIMIT 1;

INSERT INTO `widget_parameters_field_type` (`ft_typename`, `is_connector`)
SELECT * FROM (SELECT 'bv', '1') AS tmp
WHERE NOT EXISTS (SELECT `ft_typename` FROM `widget_parameters_field_type` WHERE `ft_typename` = 'bv' ) LIMIT 1;

INSERT INTO `mod_bam_user_preferences` VALUES (NULL, 'kpi_boolean_drop', '100');

UPDATE `command` SET `enable_shell` = 1 WHERE `command_name` = 'bam-notify-by-email';


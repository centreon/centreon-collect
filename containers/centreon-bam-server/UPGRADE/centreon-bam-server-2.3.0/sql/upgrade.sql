--
-- Structure de la table `mod_bam_impacts`
--
CREATE TABLE IF NOT EXISTS `mod_bam_impacts` (
  `id_impact` INT( 11 ) NOT NULL AUTO_INCREMENT PRIMARY KEY ,
  `code` TINYINT( 4 ) NOT NULL ,
  `impact` FLOAT NOT NULL,
  `color` VARCHAR( 7 ) default NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

ALTER TABLE `mod_bam_kpi` ADD `config_type` ENUM('0', '1') AFTER `comment`;
ALTER TABLE `mod_bam_kpi` ADD `drop_warning_impact_id` INT(11) AFTER `drop_warning`;
ALTER TABLE `mod_bam_kpi` ADD `drop_critical_impact_id` INT(11) AFTER `drop_critical`;
ALTER TABLE `mod_bam_kpi` ADD `drop_unknown_impact_id` INT(11) AFTER `drop_unknown`;
ALTER TABLE `mod_bam_kpi` ADD `ignore_downtime` ENUM('0', '1') DEFAULT '0';
ALTER TABLE `mod_bam_kpi` ADD `ignore_acknowledged` ENUM('0', '1') DEFAULT '0';
ALTER TABLE `mod_bam_kpi` ADD `acknowledged` FLOAT DEFAULT NULL AFTER `downtime`;
ALTER TABLE `mod_bam` ADD `acknowledged` FLOAT NOT NULL DEFAULT '0' AFTER `downtime`;

UPDATE `mod_bam_kpi` SET config_type = '1';

--
-- Contenu de la table `mod_bam_impacts`
--
INSERT INTO `mod_bam_impacts` (`code`, `impact`, `color`) VALUES
(0, 0, '#ffffff'),
(1, 5, '#ffeebb'),
(2, 25, '#ffcc77'),
(3, 50, '#ff8833'),
(4, 75, '#ff5511'),
(5, 100, '#ff0000');
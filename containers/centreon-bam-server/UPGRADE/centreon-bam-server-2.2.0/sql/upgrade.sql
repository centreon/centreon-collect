ALTER TABLE mod_bam ADD icon_id INT(11) AFTER graph_id;
ALTER TABLE mod_bam ADD id_reporting_period INT(11) AFTER id_check_period;
ALTER TABLE mod_bam ADD graph_style VARCHAR(254) AFTER icon_id;

ALTER TABLE `mod_bam` ADD CONSTRAINT `mod_bam_ibfk_4` FOREIGN KEY (`icon_id`) REFERENCES `view_img` (`img_id`) ON DELETE SET NULL;
ALTER TABLE `mod_bam` ADD CONSTRAINT `mod_bam_ibfk_5` FOREIGN KEY (`id_reporting_period`) REFERENCES `timeperiod` (`tp_id`) ON DELETE SET NULL;

ALTER TABLE `mod_bam` MODIFY downtime float;
ALTER TABLE `mod_bam` MODIFY level_w float;
ALTER TABLE `mod_bam` MODIFY level_c float;
ALTER TABLE `mod_bam` DROP COLUMN visible;

-- 
-- Structure de la table `mod_bam_user_overview_relation`
-- 
CREATE TABLE IF NOT EXISTS `mod_bam_user_overview_relation` (
  `user_id` int(11) NULL,
  `ba_id` int(11) NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Contraintes pour la table `mod_bam_user_overview_relation`
--
ALTER TABLE `mod_bam_user_overview_relation`
  ADD CONSTRAINT `mod_bam_user_overview_relation_ibfk_1` FOREIGN KEY (`user_id`) REFERENCES `contact` (`contact_id`) ON DELETE CASCADE,
  ADD CONSTRAINT `mod_bam_user_overview_relation_ibfk_2` FOREIGN KEY (`ba_id`) REFERENCES `mod_bam` (`ba_id`) ON DELETE CASCADE;



ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_logs` MODIFY level float;
ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_logs` MODIFY warning_thres float;
ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_logs` MODIFY critical_thres float;

ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_kpi_logs` MODIFY impact float;
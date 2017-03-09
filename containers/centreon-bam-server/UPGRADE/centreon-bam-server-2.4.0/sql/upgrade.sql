CREATE TABLE  `mod_bam_boolean` (
    `boolean_id` INT( 11 ) NOT NULL AUTO_INCREMENT ,
    `name` VARCHAR( 255 ) NOT NULL ,
    `config_type` TINYINT NOT NULL ,
    `impact` FLOAT NULL ,
    `impact_id` INT( 11 ) NULL ,
    `expression` TEXT NOT NULL ,
    `bool_state` BOOL NOT NULL DEFAULT '1',
    `current_state` BOOL NOT NULL DEFAULT '0',
    `comments` TEXT NULL ,
    `activate` TINYINT NOT NULL ,
    PRIMARY KEY (  `boolean_id` )
) ENGINE = INNODB CHARACTER SET utf8 COLLATE utf8_general_ci;

CREATE TABLE  `mod_bam_bool_rel` (
    `ba_id` INT( 11 ) NOT NULL,
    `boolean_id` INT( 11 ) NOT NULL,
    KEY `ba_id` (`ba_id`),
    KEY `boolean_id` (`boolean_id`)
) ENGINE = INNODB CHARACTER SET utf8 COLLATE utf8_general_ci;

ALTER TABLE `mod_bam_bool_rel`
  ADD CONSTRAINT `mod_bam_bool_ibfk_1` FOREIGN KEY (`ba_id`) REFERENCES `mod_bam` (`ba_id`) ON DELETE CASCADE,
  ADD CONSTRAINT `mod_bam_bool_ibfk_2` FOREIGN KEY (`boolean_id`) REFERENCES `mod_bam_boolean` (`boolean_id`) ON DELETE CASCADE;

ALTER TABLE @DB_CENTSTORAGE@.`mod_bam_kpi_logs` CHANGE `kpi_type` `kpi_type` ENUM('0','1','2','3') CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL;
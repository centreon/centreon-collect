ALTER TABLE `mod_bam` ADD `sla_type` TINYINT( 6 ) NULL AFTER  `level_c` ;
ALTER TABLE `mod_bam` ADD `sla_warning` FLOAT( 11 ) NULL AFTER `sla_type`;
ALTER TABLE `mod_bam` ADD `sla_critical` FLOAT( 11 ) NULL AFTER `sla_warning`;
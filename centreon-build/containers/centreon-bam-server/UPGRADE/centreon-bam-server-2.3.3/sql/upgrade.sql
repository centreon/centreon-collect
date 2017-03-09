CREATE TABLE `@DB_CENTSTORAGE@`.`mod_bam_logs_tmp` AS SELECT log_id FROM `@DB_CENTSTORAGE@`.`mod_bam_logs` GROUP BY ba_id HAVING MIN(log_id);
UPDATE `@DB_CENTSTORAGE@`.`mod_bam_logs` SET status_change_flag = '1' WHERE log_id IN (SELECT log_id FROM `@DB_CENTSTORAGE@`.`mod_bam_logs_tmp`);
DROP TABLE `@DB_CENTSTORAGE@`.`mod_bam_logs_tmp`;
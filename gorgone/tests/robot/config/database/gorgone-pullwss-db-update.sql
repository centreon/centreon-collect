-- ------------------------------------------------------------
-- upgrade script for gorgone to add pull and pullwss communication types, and new uid
-- Adds enum value '3' and '4' to gorgone_communication_type
-- Adds new field uid for more uniqueness of poller identification
-- ------------------------------------------------------------

-- 1. Update enum to allow '4'
ALTER TABLE `nagios_server`
    MODIFY `gorgone_communication_type` ENUM('1','2','3', '4')
        NOT NULL DEFAULT '1';

ALTER TABLE `nagios_server`
    ADD COLUMN `uid` BIGINT UNSIGNED DEFAULT NULL COMMENT 'Snowflake 64-bit unique identifier',
    ADD UNIQUE KEY `uniq_uid` (`uid`)
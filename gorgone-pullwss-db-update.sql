-- ------------------------------------------------------------
-- upgrade script for gorgone to add pull and pullwss communication types, and new auth token field (optionnal for now)
-- Adds enum value '3' and '4' to gorgone_communication_type
-- Adds new optional field gorgone_auth_token
-- ------------------------------------------------------------

-- 1. Update enum to allow '4'
ALTER TABLE `nagios_server`
    MODIFY `gorgone_communication_type` ENUM('1','2','3', '4')
        NOT NULL DEFAULT '1';

-- 2. Add authentication token field (optional)
ALTER TABLE `nagios_server`
    ADD COLUMN `gorgone_auth_token` VARCHAR(255) DEFAULT NULL
        AFTER `gorgone_port`;

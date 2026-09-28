-- MARIADB_DATABASE only creates one schema; gorgone-central's config expects
-- both centreon and centreon_storage (db_configuration / db_realtime).
CREATE DATABASE IF NOT EXISTS centreon_storage;
GRANT ALL ON centreon_storage.* TO 'centreon'@'%';

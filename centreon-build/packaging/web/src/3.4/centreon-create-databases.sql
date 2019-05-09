CREATE DATABASE centreon CHARACTER SET utf8;
CREATE DATABASE centreon_storage CHARACTER SET utf8;
CREATE USER 'centreon'@'localhost' IDENTIFIED BY '@CENTREON_DB_PASS@';
GRANT ALL PRIVILEGES ON centreon.* TO 'centreon'@'localhost';
GRANT ALL PRIVILEGES ON centreon_storage.* TO 'centreon'@'localhost';

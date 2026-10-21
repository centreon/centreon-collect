#!/bin/bash
# Minimal centreon database for the gorgone nodes module, and the centreon user.
# Run by the MariaDB image on the first start only (docker compose down -v to run it again).
set -e

require=""
if [ "${DB_TLS_REQUIRED:-ON}" = "ON" ]; then
    require="REQUIRE SSL"
fi

mariadb -uroot -p"$MARIADB_ROOT_PASSWORD" <<EOF
CREATE DATABASE centreon CHARACTER SET utf8mb4;
CREATE USER 'centreon'@'%' IDENTIFIED BY 'centreon' $require;
GRANT ALL PRIVILEGES ON centreon.* TO 'centreon'@'%';

USE centreon;
CREATE TABLE options (\`key\` varchar(255) DEFAULT NULL, \`value\` varchar(255) DEFAULT NULL);
INSERT INTO options (\`key\`, \`value\`) VALUES ('debug_gorgone', '0');
CREATE TABLE rs_poller_relation (remote_server_id int NOT NULL, poller_server_id int NOT NULL);
CREATE TABLE nagios_server (
    id int NOT NULL AUTO_INCREMENT PRIMARY KEY,
    name varchar(40) DEFAULT NULL,
    localhost enum('0','1') DEFAULT NULL,
    ns_ip_address varchar(200) DEFAULT NULL,
    ns_activate enum('1','0') DEFAULT '1',
    gorgone_port int DEFAULT NULL,
    gorgone_communication_type enum('1','2','3','4') NOT NULL DEFAULT '1',
    remote_id int DEFAULT NULL,
    remote_server_use_as_proxy enum('0','1') NOT NULL DEFAULT '1',
    uid bigint DEFAULT NULL
);
INSERT INTO nagios_server (id, name, localhost, ns_ip_address, ns_activate, gorgone_port, uid)
    VALUES (1, 'Central', '1', '127.0.0.1', '1', 5556, 123456789);
EOF
echo "centreon database created (DB_TLS_REQUIRED=${DB_TLS_REQUIRED:-ON})"

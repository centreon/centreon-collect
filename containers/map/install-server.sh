#!/bin/sh

set -e
set -x

# Start MySQL.
service mysql start

# Create Map database.
mysql -e "CREATE DATABASE centreon_studio"
mysql -e "CREATE USER 'centreon_map'@'localhost' IDENTIFIED BY 'centreon_map'"
mysql -e "GRANT ALL ON 'centreon_studio'.* TO 'centreon_map'@'localhost' WITH MAX_QUERIES_PER_HOUR 0 MAX_CONNECTIONS_PER_HOUR 0 MAX_UPDATES_PER_HOUR 0 MAX_USER_CONNECTIONS 0"

# Stop MySQL.
service mysql stop

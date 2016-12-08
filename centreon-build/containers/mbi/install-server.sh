#!/bin/sh

set -e
set -x

# Start services.
service mysql start

# Create databases.
mysql -e 'CREATE DATABASE centreon'
mysql -e 'CREATE DATABASE centreon_storage'

# Grant reporting user on the databases (extracted from install.sh).
REPORTING_DB_USER='centreon'
REPORTING_DB_PWD='centreon'
mysql -e "GRANT file ON *.* TO '$REPORTING_DB_USER'@'localhost' IDENTIFIED BY '$REPORTING_DB_PWD'"
mysql -e "GRANT ALL PRIVILEGES ON centreon.* to '$REPORTING_DB_USER'@'localhost' IDENTIFIED BY '$REPORTING_DB_PWD'" centreon
mysql -e "GRANT ALL PRIVILEGES ON centreon_storage.* to '$REPORTING_DB_USER'@'localhost' IDENTIFIED BY '$REPORTING_DB_PWD'" centreon_storage
mysql -e "GRANT SELECT ON centreon.* TO '$MONITORING_DB_USER'@'$MONITORING_DB_IP' IDENTIFIED BY '$MONITORING_DB_PWD'" centreon
mysql -e "GRANT SELECT ON centreon_storage.* TO '$MONITORING_DB_USER'@'$MONITORING_DB_IP' IDENTIFIED BY '$MONITORING_DB_PWD'" centreon_storage

# Install the reporting schema.
/usr/share/centreon-bi/bin/centreonBIETL -c

# Stop services.
service mysql stop

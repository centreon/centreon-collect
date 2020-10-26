#!/bin/sh

set -e
set -x

# Start services.
service mysql start

# Create databases.
mysql -e 'CREATE DATABASE centreon'
mysql -e 'CREATE DATABASE centreon_storage'

# Grant privileges to users on the databases (extracted from install.sh).
REPORTING_DB_USER='centreonbi'
REPORTING_DB_PWD='centreonbi'
MONITORING_DB_USER='centreonbi'
MONITORING_DB_PWD='centreonbi'
mysql -e "GRANT file ON *.* TO '$REPORTING_DB_USER'@'localhost' IDENTIFIED BY '$REPORTING_DB_PWD'"
mysql -e "GRANT ALL PRIVILEGES ON centreon.* to '$REPORTING_DB_USER'@'localhost' IDENTIFIED BY '$REPORTING_DB_PWD'" centreon
mysql -e "GRANT ALL PRIVILEGES ON centreon_storage.* to '$REPORTING_DB_USER'@'localhost' IDENTIFIED BY '$REPORTING_DB_PWD'" centreon_storage
mysql -e "GRANT SELECT ON centreon.* TO '$MONITORING_DB_USER'@'%' IDENTIFIED BY '$MONITORING_DB_PWD'" centreon
mysql -e "GRANT SELECT ON centreon_storage.* TO '$MONITORING_DB_USER'@'%' IDENTIFIED BY '$MONITORING_DB_PWD'" centreon_storage

# Stop services.
service mysql stop

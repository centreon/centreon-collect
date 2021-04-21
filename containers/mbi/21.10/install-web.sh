#!/bin/sh

set -e
set -x

# Start services.
service mysql start

# Install Centreon MBI server.
/tmp/install-centreon-module.php -b /usr/share/centreon/bootstrap.php -m centreon-bi-server

# Configure Centreon MBI.
# cbis.host is MySQL host wildcard (%) to fake centreonMysqlRights.pl.
mysql -e "UPDATE mod_bi_options SET opt_value='%' WHERE opt_key='cbis.host'" centreon
mysql -e "UPDATE mod_bi_options SET opt_value='true' WHERE opt_key='etl.host.dedicated'" centreon
mysql -e "UPDATE mod_bi_options SET opt_value='mbi' WHERE opt_key='reportingDB.ip'" centreon

# Grant privileges on database to Centreon MBI.
/usr/share/centreon/www/modules/centreon-bi-server/tools/centreonMysqlRights.pl

# Set valid configuration for cbis.host.
mysql -e "UPDATE mod_bi_options SET opt_value='mbi' WHERE opt_key='cbis.host'" centreon

# Stop services.
service mysql stop

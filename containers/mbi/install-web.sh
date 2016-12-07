#!/bin/sh

set -e
set -x

# Start services.
service mysql start
httpd -k start

# Install Centreon MBI server.
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-bi-server

# Configure Centreon MBI.
mysql -e "UPDATE mod_bi_options SET opt_value='mbi' WHERE opt_key='cbis.host'" centreon
mysql -e "UPDATE mod_bi_options SET opt_value='true' WHERE opt_key='etl.host.dedicated'" centreon
mysql -e "UPDATE mod_bi_options SET opt_value='mbi' WHERE opt_key='reportingDB.ip'" centreon

# Grant privileges on database to Centreon MBI.
/usr/share/centreon/www/modules/centreon-bi-server/tools/centreonMysqlRights.pl

# Stop services.
httpd -k stop
service mysql stop

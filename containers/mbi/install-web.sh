#!/bin/sh

set -e
set -x

# Start services.
service mysql start
service httpd -k start

# Install Centreon MBI server.
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-bi-server

# Configure Centreon MBI.
# XXX

# Grant privileges on database to Centreon MBI.
/usr/share/centreon/www/modules/centreon-bi-server/tools/centreonMysqlRights.pl

# Stop services.
httpd -k stop
service mysql stop

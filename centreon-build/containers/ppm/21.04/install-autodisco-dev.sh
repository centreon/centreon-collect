#!/bin/sh

set -e
set -x

service mysql start
mysql centreon < /usr/share/centreon/www/modules/centreon-pp-manager/sql/uninstall.sql
mysql centreon < /usr/share/centreon/www/modules/centreon-autodiscovery-server/sql/uninstall.sql
mysql centreon < /usr/share/centreon/www/modules/centreon-autodiscovery-server/sql/install.sql
mysql centreon < /usr/share/centreon/www/modules/centreon-pp-manager/sql/install.sql
service mysql stop

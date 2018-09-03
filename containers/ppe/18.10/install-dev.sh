#!/bin/sh

set -e
set -x

service mysql start
mysql centreon < /usr/share/centreon/www/modules/centreon-export/sql/uninstall.sql
mysql centreon < /usr/share/centreon/www/modules/centreon-export/sql/install.sql
service mysql stop

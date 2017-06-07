#!/bin/sh

set -e
set -x

service mysql start
mysql centreon < /usr/share/centreon/www/modules/centreon-license-manager/sql/uninstall.sql
mysql centreon < /usr/share/centreon/www/modules/centreon-license-manager/sql/install.sql
service mysql stop

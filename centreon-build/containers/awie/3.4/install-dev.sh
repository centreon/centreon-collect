#!/bin/sh

set -e
set -x

service mysql start
mysql centreon < /usr/share/centreon/www/modules/api-web-import-export/sql/uninstall.sql
mysql centreon < /usr/share/centreon/www/modules/api-web-import-export/sql/install.sql
service mysql stop

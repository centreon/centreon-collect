#!/bin/sh

set -e
set -x

service mysql start
mysql centreon < /usr/share/centreon/www/modules/centreon-poller-display/sql/uninstall.sql
mysql centreon < /usr/share/centreon/www/modules/centreon-poller-display/sql/install.sql
service mysql stop

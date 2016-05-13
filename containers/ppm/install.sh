#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
cd /usr/share/centreon/www/modules/centreon-license-manager/frontend/app
npm install
npm install -g gulp
gulp
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-license-manager
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-pp-manager
httpd -k stop
service mysql stop

#!/bin/sh

set -e
set -x

sed -i "s#define('CENTREON_IMP_API_URL', '');#define('CENTREON_IMP_API_URL', 'http://middleware:3000/api');#g" /usr/share/centreon/www/modules/centreon-license-manager/centreon-license-manager.conf.php
service mysql start
httpd -k start
cd /usr/share/centreon/www/modules/centreon-license-manager/frontend/app
npm install
npm install -g gulp
gulp
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-license-manager
httpd -k stop
service mysql stop

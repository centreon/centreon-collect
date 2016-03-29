#!/bin/sh

set -e
set -x

service mysql start
httpd
cd /usr/share/centreon/www/modules/centreon-license-manager/frontend/app
npm install
npm install -g gulp
gulp
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-license-manager
# Stop httpd. killall ?
service mysql stop

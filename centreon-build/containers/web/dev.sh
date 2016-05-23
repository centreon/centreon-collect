#!/bin/sh

set -e
set -x

service mysql start
php /tmp/update-centreon.php -c /etc/centreon/centreon.conf.php
service mysql stop
rm -rf /usr/share/centreon/www/install

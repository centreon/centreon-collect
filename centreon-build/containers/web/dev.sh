#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
cd /tmp/centreon/
bash install.sh -i -f /tmp/vars
php /tmp/update-centreon.php -c /etc/centreon/centreon.conf.php
centreon -d -u admin -p centreon -a POLLERGENERATE -v 1
centreon -d -u admin -p centreon -a CFGMOVE -v 1
httpd -k stop
service mysql stop

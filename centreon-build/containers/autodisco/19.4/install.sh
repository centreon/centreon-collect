#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
/tmp/install-centreon-module.php -b /usr/share/centreon/bootstrap.php -m centreon-autodiscovery-server
httpd -k stop
service mysql stop

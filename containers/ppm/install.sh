#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
/tmp/install-centreon-module.php -b /usr/share/centreon/bootstrap.php -m centreon-license-manager
/tmp/install-centreon-module.php -b /usr/share/centreon/bootstrap.php -m centreon-pp-manager
httpd -k stop
service mysql stop

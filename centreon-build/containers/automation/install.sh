#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-license-manager
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-pp-manager
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-automation
httpd -k stop
service mysql stop

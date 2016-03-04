#!/bin/sh

set -e
set -x

service mysql start
service httpd start
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-export
service httpd stop
service mysql stop

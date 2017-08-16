#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m api-web-import-export
httpd -k stop
service mysql stop

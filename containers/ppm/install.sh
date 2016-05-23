#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-pp-manager
mysql -e "GRANT ALL ON *.* to root@'%' IDENTIFIED BY 'centreon'"
httpd -k stop
service mysql stop

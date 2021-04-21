#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-pp-manager
yum install --disablerepo='ces-*-unstable*' -y --nogpgcheck 'ces-pack-*'
httpd -k stop
service mysql stop

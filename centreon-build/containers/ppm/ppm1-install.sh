#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-pp-manager
yum --disablerepo='ces-standard-unstable*' install -y --nogpgcheck ces-pack-* ces-plugins-*
httpd -k stop
service mysql stop

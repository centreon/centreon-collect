#!/bin/sh

set -e
set -x

service mysql start
httpd
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-export
# Stop httpd. killall ?
service mysql stop

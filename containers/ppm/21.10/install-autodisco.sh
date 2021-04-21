#!/bin/sh

set -e
set -x

service mysql start
su -c '/tmp/install-centreon-module.php -b /usr/share/centreon/bootstrap.php -m centreon-autodiscovery-server' -s /bin/bash apache
service mysql stop

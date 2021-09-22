#!/bin/sh

set -e
set -x

service mysql start
su -s /bin/bash -c "/tmp/install-centreon-module.php -b /usr/share/centreon/bootstrap.php -m centreon-license-manager" apache
su -s /bin/bash -c "/tmp/install-centreon-module.php -b /usr/share/centreon/bootstrap.php -m centreon-bam-server" apache
service mysql stop

#!/bin/sh

set -e
set -x

service mysql start
/tmp/install-centreon-module.php -b /usr/share/centreon/bootstrap.php -m centreon-bam-server -u
service mysql stop

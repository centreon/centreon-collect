#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-bam-server
httpd -k stop
service mysql stop

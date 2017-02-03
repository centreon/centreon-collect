#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
for w in global-health graph-monitoring hostgroup-monitoring httploader service-monitoring servicegroup-monitoring ; do
  php /tmp/install-centreon-widget.php -c /etc/centreon/centreon.conf.php -w $w
done
httpd -k stop
service mysql stop

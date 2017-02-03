#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
for w in engine-status global-health graph-monitoring grid-map host-monitoring hostgroup-monitoring httploader live-top10-cpu-usage live-top10-memory-usage service-monitoring servicegroup-monitoring tactical-overview ; do
  php /tmp/install-centreon-widget.php -c /etc/centreon/centreon.conf.php -w $w
done
httpd -k stop
service mysql stop

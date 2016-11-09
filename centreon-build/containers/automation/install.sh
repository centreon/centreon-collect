#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-license-manager
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-pp-manager
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-automation
# Temporary fix, until base-generic PP can be installed with Clapi.
mysql centreon -e "UPDATE host SET host_name='generic-active-host', host_alias='generic-active-host' WHERE host_name='generic-host'"
centreon -d -u admin -p centreon -a POLLERGENERATE -v 1
centreon -d -u admin -p centreon -a CFGMOVE -v 1
httpd -k stop
service mysql stop

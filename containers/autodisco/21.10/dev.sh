#!/bin/sh

service mysql start
/tmp/install-centreon-module.php -b /usr/share/centreon/bootstrap.php -m centreon-autodiscovery-server -u
service mysql stop

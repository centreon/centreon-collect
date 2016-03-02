#!/bin/sh

service mysql start
service httpd start
./install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-export
service httpd stop
service mysql stop

#!/bin/sh

set -e
set -x

service mysql start
php /tmp/update-centreon.php -c /etc/centreon/centreon.conf.php
mysql < /tmp/kb.sql
service mysql stop
rm -rf /usr/share/centreon/www/install

# Temporary fix due tu clapi bug : waiting new centreon stable version
rm -rf /etc/centreon-engine/*
rm -rf /etc/centreon-broker/poller*
rm -rf /etc/centreon-broker/central*

#!/bin/sh

set -e
set -x

service mysql start
php /tmp/update-centreon.php -c /etc/centreon/centreon.conf.php
mysql centreon < /tmp/dev/sql/standard.sql
mysql centreon < /tmp/dev/sql/kb.sql
mysql centreon < /tmp/dev/sql/openldap.sql
mysql centreon < /tmp/dev/sql/media.sql
centreon -d -u admin -p centreon -a POLLERGENERATE -v 1
centreon -d -u admin -p centreon -a CFGMOVE -v 1
service mysql stop
rm -rf /usr/share/centreon/www/install

# Temporary fix, should be removed when Centreon Web 2.8.0 is released.
mkdir -p /var/cache/centreon/backup || true

# Temporary fix due tu clapi bug : waiting new centreon stable version
rm -rf /etc/centreon-engine/*
rm -rf /etc/centreon-broker/poller*
rm -rf /etc/centreon-broker/central*

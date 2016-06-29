#!/bin/sh

set -e
set -x

service mysql start
sed 's/@DB_CENTSTORAGE@/centreon_storage/g' < /usr/share/centreon/www/modules/centreon-bam-server/sql/uninstall.sql | mysql centreon
sed 's/@DB_CENTSTORAGE@/centreon_storage/g' < /usr/share/centreon/www/modules/centreon-bam-server/sql/install.sql | mysql centreon
service mysql stop

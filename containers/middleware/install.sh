#!/bin/sh

set -e
set -x

find /var/lib/mysql -type f -exec touch {} \;
service mysql start
mysql -u root -pcentreon -e 'DROP DATABASE IF EXISTS imp'
mysql -u root -pcentreon -e 'CREATE DATABASE imp'
mysql -u root -pcentreon imp < /usr/local/src/centreon-imp-portal-api/database/create.sql
mysql -u root -pcentreon imp < /usr/local/src/centreon-imp-portal-api/database/1-add-download-statistics.sql
mysql -u root -pcentreon imp < /usr/local/src/centreon-imp-portal-api/database/2-salesforce.sql
mysql -u root -pcentreon imp < /usr/local/src/centreon-imp-portal-api/database/3-product.sql
mysql -u root -pcentreon imp < /usr/local/src/centreon-imp-portal-api/database/4-sales-to-online.sql
mysql -u root -pcentreon imp < /usr/local/src/contact.sql
php /usr/local/src/json2sql.php
mysql -u root -pcentreon -e "GRANT ALL ON *.* to root@'%' IDENTIFIED BY 'centreon'"
service mysql stop

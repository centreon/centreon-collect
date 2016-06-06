#!/bin/sh

set -e
set -x

service mysql start
mysql -u root -pcentreon -e 'DROP DATABASE IF EXISTS imp'
mysql -u root -pcentreon -e 'CREATE DATABASE imp'
mysql -u root -pcentreon imp < /usr/local/src/centreon-imp-portal-api/database/create.sql
mysql -u root -pcentreon imp < /usr/local/src/contact.sql
php /usr/local/src/json2sql.php
mysql -u root -pcentreon -e "GRANT ALL ON *.* to root@'%' IDENTIFIED BY 'centreon'"
service mysql stop

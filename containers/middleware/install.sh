#!/bin/sh

set -e
set -x

service mysql start
mysql -e 'CREATE DATABASE imp'
mysql imp < /usr/local/src/centreon-imp-portal-api/database/create.sql
mysql imp < /usr/local/src/imp.sql
mysql -e "GRANT ALL ON *.* to root@'%' IDENTIFIED BY 'centreon'"
service mysql stop

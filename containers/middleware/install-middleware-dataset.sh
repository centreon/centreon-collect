#!/bin/sh

set -e
set -x

service mysql start
mysql -u root -pcentreon imp < /usr/local/src/company.sql
mysql -u root -pcentreon imp < /usr/local/src/subscription.sql
mysql -u root -pcentreon imp < /usr/local/src/instance.sql
service mysql stop

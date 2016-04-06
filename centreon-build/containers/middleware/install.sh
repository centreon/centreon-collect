#!/bin/sh

set -e
set -x

service mysql start
mysql -e 'CREATE DATABASE imp'
mysql imp < /usr/local/src/centreon-imp-portal-api/database/create.sql
service mysql stop

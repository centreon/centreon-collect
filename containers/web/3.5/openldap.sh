#!/bin/sh

set -e
set -x

service mysql start
mysql centreon < /tmp/openldap.sql
service mysql stop

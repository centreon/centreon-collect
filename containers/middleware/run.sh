#!/bin/sh

set -e
set -x

service mysql start
/etc/init.d/redis_6379 start
cd /usr/local/src/centreon-imp-portal-api
gulp serve
/etc/init.d/redis_6379 stop
service mysql stop

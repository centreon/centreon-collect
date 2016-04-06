#!/bin/sh

set -e
set -x

service mysql start
cd /usr/local/src/centreon-imp-portal-api
gulp serve
service mysql stop

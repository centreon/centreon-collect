#!/bin/sh

set -e
set -x

service mysql start
mysql centreon < /tmp/media.sql
service mysql stop

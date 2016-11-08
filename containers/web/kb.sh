#!/bin/sh

set -e
set -x

service mysql start
mysql centreon < /tmp/kb.sql
service mysql stop

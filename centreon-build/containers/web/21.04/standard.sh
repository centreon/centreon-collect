#!/bin/sh

set -e
set -x

service mysql start
# Temporary fix to provide default setup while we're working on free plugin packs.
mysql centreon < /tmp/standard/sql/standard.sql
mysql centreon < /tmp/standard/sql/media.sql
centreon -u admin -p centreon -a POLLERGENERATE -v 1
centreon -u admin -p centreon -a CFGMOVE -v 1
service mysql stop

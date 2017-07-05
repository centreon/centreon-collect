#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
# Temporary fix to provide default setup while we're working on free plugin packs.
mysql centreon < /tmp/stable/sql/standard.sql
mysql centreon < /tmp/stable/sql/kb.sql
mysql centreon < /tmp/stable/sql/openldap.sql
mysql centreon < /tmp/stable/sql/media.sql
centreon -d -u admin -p centreon -a POLLERGENERATE -v 1
centreon -d -u admin -p centreon -a CFGMOVE -v 1
httpd -k stop
service mysql stop

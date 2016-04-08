#!/bin/sh

set -e
set -x

echo '10.30.2.27 support.centreon.com' >> /etc/hosts
service mysql start
screen -d -m slapd -u ldap -h ldap:// -F /etc/openldap/slapd.d/
ldapadd -f /tmp/ldap.ldif -D 'cn=Manager,dc=centreon,dc=com' -w centreon || true
/etc/init.d/redis_6379 start
cd /usr/local/src/centreon-imp-portal-api
gulp serve
/etc/init.d/redis_6379 stop
killall -TERM slapd
service mysql stop

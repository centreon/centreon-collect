#!/bin/sh

set -e
set -x

echo '10.30.2.62 support.centreon.com' >> /etc/hosts
echo '10.24.11.73 crm.int.centreon.com' >> /etc/hosts
service mysql start
screen -d -m slapd -u ldap -h ldap:// -F /etc/openldap/slapd.d/
started=1
while [ "$started" -ne 0 ] ; do
  started=`nc -q 1 localhost 389 ; echo $?`
done
ldapadd -f /tmp/ldap.ldif -D 'cn=Manager,dc=centreon,dc=com' -w centreon || true
/etc/init.d/redis_6379 start
cd /usr/local/src/centreon-imp-portal-api
node localserver/app.js
/etc/init.d/redis_6379 stop
killall -TERM slapd
service mysql stop

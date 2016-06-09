#!/bin/sh

set -e
set -x

service mysql start
screen -d -m slapd -u openldap -h ldap:// -F /etc/ldap/slapd.d/
set +e
started=1
while [ "$started" -ne 0 ] ; do
  started=`nc -w 1 localhost 389 ; echo $?`
  sleep 1
done
while [ "$started" -ne 0 ] ; do
  started=`nc -w 1 redis 6379 ; echo $?`
  sleep 1
done
set -e
ldapadd -f /tmp/ldap.ldif -D 'cn=Manager,dc=centreon,dc=com' -w centreon || true
cd /usr/local/src/centreon-imp-portal-api
node localserver/app.js
killall -TERM slapd
service mysql stop

#!/bin/sh

set -e
set -x

find /var/lib/mysql -type f -exec touch {} \;
service mysql start
set +e
started=1
while [ "$started" -ne 0 ] ; do
  started=`mysql -u root -pcentreon imp -e 'SELECT * FROM company' > /dev/null ; echo $?`
  sleep 1
done
started=1
while [ "$started" -ne 0 ] ; do
  started=`nc -w 1 redis 6379 ; echo $?`
  sleep 1
done
started=1
while [ "$started" -ne 0 ] ; do
  started=`nc -w 1 openldap 389 ; echo $?`
  sleep 1
done
started=1
while [ "$started" -ne 0 ] ; do
  started=`curl -I http://sqs:9324 | grep 404 ; echo $?`
  sleep 1
done
set -e
cd /usr/local/src/centreon-imp-portal-api
node localserver/app.js
service mysql stop

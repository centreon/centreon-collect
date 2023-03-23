#!/bin/bash

if [ -x /usr/bin/mariadb ] ; then
  mysql=mariadb
elif [ -x /usr/bin/mysql ] ; then
  mysql=mysql
fi

docker run --name mariadbtest -e MYSQL_ROOT_PASSWORD=centreon -p 3306:3306 -d docker.io/library/mariadb:10.5
$mysql -h 127.0.0.1 -u root -pcentreon < ../resources/centreon_storage.sql
$mysql -h 127.0.0.1 -u root -pcentreon < ../resources/centreon.sql

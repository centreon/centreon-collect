#!/bin/bash

database_type=$1

DBUserRoot="root"
DBPassRoot="centreon"

DBStorage=$(awk '($1=="${DBName}") {print $2}' /scripts/tests/resources/db_variables.robot)
DBConf=$(awk '($1=="${DBNameConf}") {print $2}' /scripts/tests/resources/db_variables.robot)

cd /scripts

#create users
if [ $database_type == 'mysql' ]; then
    echo "create users mysql"
    mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "CREATE USER IF NOT EXISTS 'centreon'@'%' IDENTIFIED WITH mysql_native_password BY 'centreon'"
    mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "CREATE USER IF NOT EXISTS 'root_centreon'@'%' IDENTIFIED WITH mysql_native_password BY 'centreon'"
else
    #mariadb case
    ss -plant | grep -w 3306
    while [ $? -ne 0 ]
    do
        sleep 1
        ss -plant | grep -w 3306
    done
    sleep 1

    echo "create users mariadb"
    mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "CREATE USER IF NOT EXISTS 'centreon'@'%' IDENTIFIED BY 'centreon'"
    mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "CREATE USER IF NOT EXISTS 'root_centreon'@'%' IDENTIFIED BY 'centreon'"
fi
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "GRANT ALL PRIVILEGES ON *.* to 'centreon'@'%'"
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "GRANT ALL PRIVILEGES ON *.* to 'root_centreon'@'%'"
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "flush privileges"
#clean if use persistant storage
echo "clean databases ${DBConf} ${DBStorage} "
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "drop database if exists ${DBConf}"
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "drop database if exists ${DBStorage}"
#create databases
echo "create database conf ${DBConf}"
cat /scripts/resources/centreon.sql | sed "s/DBNameConf/${DBConf}/g" > /tmp/centreon.sql

mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 < /tmp/centreon.sql

echo "create database storage ${DBStorage}"
if [ $DBStorage != 'centreon_storage' ]
then
    \rm /tmp/centreon_storage.sql
    cat /scripts/resources/centreon_storage.sql | sed "s/centreon_storage/${DBStorage}/g" > /tmp/centreon_storage.sql
    mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 < /tmp/centreon_storage.sql
else
    mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 < /scripts/resources/centreon_storage.sql
fi


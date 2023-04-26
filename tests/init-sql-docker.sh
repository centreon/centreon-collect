#!/bin/bash

ss -nl | grep "0.0.0.0:3306"
while [ $? -ne 0 ]
do
    sleep 1
    ss -nl | grep "0.0.0.0:3306"
done

DBUserRoot="root"
DBPassRoot="centreon"

DBStorage=$(awk '($1=="${DBName}") {print $2}' /scripts/tests/resources/db_variables.robot)
DBConf=$(awk '($1=="${DBNameConf}") {print $2}' /scripts/tests/resources/db_variables.robot)

cd /scripts

#create users
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "grant all privileges ON *.* to 'centreon'@'%' identified by 'centreon'"
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "grant all privileges ON *.* to 'root_centreon'@'%' identified by 'centreon'"
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "flush privileges"
#clean if use persistant storage
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "drop database if exists " ${DBConf}
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "drop database if exists " ${DBStorage}
#create databases
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "drop database ${DBConf}"
cat /scripts/resources/centreon.sql | sed "s/DBNameConf/${DBConf}/g" > /tmp/centreon.sql

mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 < /tmp/centreon.sql

if [ $DBStorage != 'centreon_storage' ]
then
    \rm /tmp/centreon_storage.sql
    cat /scripts/resources/centreon_storage.sql | sed "s/centreon_storage/${DBStorage}/g" > /tmp/centreon_storage.sql
    mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 < /tmp/centreon_storage.sql    
else
    mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 < /scripts/resources/centreon_storage.sql
fi


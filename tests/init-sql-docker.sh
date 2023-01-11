#!/bin/bash

ss -nl | grep "0.0.0.0:3306"
while [ $? -ne 0 ]
do
    sleep 1
    ss -nl | grep "0.0.0.0:3306"
done

DBUserRoot="root"
DBPassRoot="centreon"

cd /scripts

#create users
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "grant all privileges ON *.* to 'centreon'@'%' identified by 'centreon'"
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "flush privileges"
#clean if use persistant storage
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "drop database if exists centreon"
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "drop database if exists centreon_storage"
#create databases
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 < resources/centreon.sql
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 < resources/centreon_storage.sql
#activate queries log
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "SET GLOBAL general_log=1;"
mysql --user="$DBUserRoot" --password="$DBPassRoot" -h 127.0.0.1 -e "SET GLOBAL general_log_file='/scripts/tests/mariadb_log/mariadb.log';"

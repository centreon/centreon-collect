#!/bin/bash

rm -rf /tmp/mariadb_log

DBUserRoot=$(awk '($1=="${DBUserRoot}") {print $2}' resources/db_variables.resource)
DBPassRoot=$(awk '($1=="${DBPassRoot}") {print $2}' resources/db_variables.resource)
DBStorage=$(awk '($1=="${DBName}") {print $2}' resources/db_variables.resource)
DBConf=$(awk '($1=="${DBNameConf}") {print $2}' resources/db_variables.resource)

if [ -z $DBUserRoot ] ; then
    DBUserRoot="root"
fi

if [ -z $DBPassRoot ] ; then
    DBPassRoot="centreon"
fi

mysql -e "set password for '$DBUserRoot'@'localhost' = PASSWORD('$DBPassRoot')"
mysql -e "GRANT ALL PRIVILEGES ON *.* TO '$DBUserRoot'@'localhost'"
mysql -e "flush privileges"

mysql --user="$DBUserRoot" --password="$DBPassRoot" -e "drop database ${DBConf}"
cat ../resources/centreon.sql | sed "s/DBNameConf/${DBConf}/g" > /tmp/centreon.sql

mysql --user="$DBUserRoot" --password="$DBPassRoot" < /tmp/centreon.sql

if [ $DBStorage != 'centreon_storage' ]
then
    \rm /tmp/centreon_storage.sql
    cat ../resources/centreon_storage.sql | sed "s/centreon_storage/${DBStorage}/g" > /tmp/centreon_storage.sql
    mysql --user="$DBUserRoot" --password="$DBPassRoot" < /tmp/centreon_storage.sql
else
    mysql --user="$DBUserRoot" --password="$DBPassRoot" < ../resources/centreon_storage.sql
fi

mkdir /tmp/mariadb_log
chown mysql: /tmp/mariadb_log

#activate queries log
mysql --user="$DBUserRoot" --password="$DBPassRoot"  -e "SET GLOBAL general_log=1;"
mysql --user="$DBUserRoot" --password="$DBPassRoot"  -e "SET GLOBAL general_log_file='/tmp/mariadb_log/mariadb.log';"


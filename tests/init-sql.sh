#!/bin/bash
DBUser=$(awk '($1=="${DBUser}") {print $2}' resources/db_variables.robot)
DBUserRoot=$(awk '($1=="${DBUserRoot}") {print $2}' resources/db_variables.robot)
DBPass=$(awk '($1=="${DBPass}") {print $2}' resources/db_variables.robot)
DBPassRoot=$(awk '($1=="${DBPassRoot}") {print $2}' resources/db_variables.robot)
DBStorage=$(awk '($1=="${DBName}") {print $2}' resources/db_variables.robot)
DBConf=$(awk '($1=="${DBNameConf}") {print $2}' resources/db_variables.robot)

if [ -z $DBUserRoot ] ; then
    DBUserRoot="root"
fi

if [ -z $DBPassRoot ] ; then
    DBPassRoot="centreon"
fi

mysql -e  "CREATE USER IF NOT EXISTS '$DBUser'@'localhost'"
mysql -e "set password for '$DBUser'@'localhost' = PASSWORD('$DBPass')"
mysql -e "GRANT ALL PRIVILEGES ON $DBStorage.* TO $DBUser@localhost"
mysql -e "GRANT ALL PRIVILEGES ON $DBConf.* TO $DBUser@localhost"
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

#activate queries log
mysql --user="$DBUserRoot" --password="$DBPassRoot"  -e "SET GLOBAL general_log=1;"
mysql --user="$DBUserRoot" --password="$DBPassRoot"  -e "SET GLOBAL general_log_file='/tmp/mariadb.log';"


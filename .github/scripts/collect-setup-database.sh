#!/bin/bash
set -e
set -x


database_type=$1

. /etc/os-release
distrib=${ID}
distrib=$(echo $distrib | tr '[:lower:]' '[:upper:]')


if [ $database_type == 'mysql' ]; then
    echo "########################### Start MySQL ######################################"
    #workaround of forbidden execution of mysqld
    cp /usr/libexec/mysqld /usr/libexec/mysqldtoto
    /usr/libexec/mysqldtoto --user=root --initialize-insecure
    /usr/libexec/mysqldtoto --user=root &

    while [ ! -S /var/lib/mysql/mysql.sock ] && [ ! -S /var/run/mysqld/mysqld.sock ]; do
        sleep 10
    done

else
    echo "########################### Start MariaDB ######################################"
    if [ "$distrib" = "ALMALINUX" ]; then
      mysql_install_db --user=root --basedir=/usr --datadir=/var/lib/mysql
      mariadbd --socket=/var/lib/mysql/mysql.sock --user=root > /dev/null 2>&1 &
    else
      mkdir -p /run/mysqld
      chown mysql:mysql /run/mysqld
      mariadbd --socket=/run/mysqld/mysqld.sock --user=root > /dev/null 2>&1 &
    fi
fi

sleep 5
echo "########################### Init centreon database ############################"

mysql -e "CREATE USER IF NOT EXISTS 'centreon'@'localhost' IDENTIFIED BY 'centreon'"
mysql -e "CREATE USER IF NOT EXISTS 'root_centreon'@'localhost' IDENTIFIED BY 'centreon'"


mysql -e "GRANT SELECT,UPDATE,DELETE,INSERT,CREATE,DROP,INDEX,ALTER,LOCK TABLES,CREATE TEMPORARY TABLES, EVENT,CREATE VIEW ON *.* TO  'centreon'@'localhost'"
mysql -e "GRANT ALL PRIVILEGES ON *.* TO 'root_centreon'@'localhost'"

cat resources/centreon.sql | sed "s/DBNameConf/centreon/g" > /tmp/centreon.sql

echo "create storage database"
mysql -u root_centreon -pcentreon < resources/centreon_storage.sql

echo "create conf database"
mysql -u root_centreon -pcentreon < /tmp/centreon.sql


#!/bin/bash
set -e
set -x

export RUN_ENV=docker

test_file=$1
database_type=$2

. /etc/os-release
distrib=${ID}
distrib=$(echo $distrib | tr '[:lower:]' '[:upper:]')

if [ o${database_type} == 'o-mysql' ] && [ ! -f tests/${test_file}.mysql ]; then
    echo > tests/log.html
    echo '<?xml version="1.0" encoding="UTF-8"?>' > tests/output.xml
    echo '<robot generator="Robot 6.0.2 (Python 3.9.14 on linux)" generated="20230517 15:35:12.235" rpa="false" schemaversion="3"></robot>' >> tests/output.xml
    echo > tests/report.html
    exit 0
fi

echo "########################### Configure and start sshd ###########################"
ssh-keygen -t rsa -f /etc/ssh/ssh_host_rsa_key -P "" <<<y
ssh-keygen -t ecdsa -f /etc/ssh/ssh_host_ecdsa_key -P "" <<<y
ssh-keygen -t ed25519 -f /etc/ssh/ssh_host_ed25519_key -P "" <<<y
/usr/sbin/sshd -D  &

if [ $database_type == '-mysql' ]; then
    echo "########################### Start MySQL ######################################"
    #workaround of forbidden execution of mysqld
    cp /usr/libexec/mysqld /usr/libexec/mysqldtoto
    /usr/libexec/mysqldtoto --user=root --initialize-insecure
    /usr/libexec/mysqldtoto --user=root &

    while [ ! -S /var/lib/mysql/mysql.sock ] && [ ! -S /var/run/mysqld/mysqld.sock ]; do
        sleep 10
    done

    sleep 5
    echo "########################### Init centreon database ############################"

    mysql -e "CREATE USER IF NOT EXISTS 'centreon'@'localhost' IDENTIFIED WITH mysql_native_password BY 'centreon';"
    mysql -e "CREATE USER IF NOT EXISTS 'root_centreon'@'localhost' IDENTIFIED WITH mysql_native_password BY 'centreon';"
else
    echo "########################### Start MariaDB ######################################"
    mysql_install_db --user=root --basedir=/usr --datadir=/var/lib/mysql
    mariadbd --socket=/var/lib/mysql/mysql.sock --user=root > /dev/null 2>&1 &
    sleep 5

    echo "########################### Init centreon database ############################"

    mysql -e "CREATE USER IF NOT EXISTS 'centreon'@'localhost' IDENTIFIED BY 'centreon';"
    mysql -e "CREATE USER IF NOT EXISTS 'root_centreon'@'localhost' IDENTIFIED BY 'centreon';"
fi

mysql -e "GRANT SELECT,UPDATE,DELETE,INSERT,CREATE,DROP,INDEX,ALTER,LOCK TABLES,CREATE TEMPORARY TABLES, EVENT,CREATE VIEW ON *.* TO  'centreon'@'localhost';"
mysql -e "GRANT ALL PRIVILEGES ON *.* TO 'root_centreon'@'localhost'"

cat resources/centreon.sql | sed "s/DBNameConf/centreon/g" > /tmp/centreon.sql

mysql -u root_centreon -pcentreon < resources/centreon_storage.sql
mysql -u root_centreon -pcentreon < /tmp/centreon.sql

echo "########################## Install centreon collect ###########################"

echo "Installation..."
if [ "$distrib" = "ALMALINUX" ]; then
  #/usr/bin/rpm -Uvvh --force --nodeps *.rpm
  dnf install -y ./*.rpm
else
  apt-get update
  apt-get install -y ./*.deb
fi

echo "########################### Install Robot Framework ###########################"
cd tests
pip3 install -U robotframework robotframework-databaselibrary pymysql python-dateutil psutil

if [ "$distrib" = "ALMALINUX" ]; then
  dnf groupinstall -y "Development Tools"
  dnf install -y python3-devel
else
  apt-get update
  apt-get install -y build-essential
  apt-get install -y python3-dev
fi

pip3 install grpcio grpcio_tools

echo "##### site-packages #####"
find / -name site-packages

echo "##### grpc #####"
find / -name grpc

echo "##### Starting tests #####"
./init-proto.sh

echo "####################### Run Centreon Collect Robot Tests #######################"
robot $test_file


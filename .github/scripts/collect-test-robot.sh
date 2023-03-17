#!/bin/bash
set -e
set -x

export RUN_ENV=docker

echo "########################### Configure and start sshd ###########################"
ssh-keygen -t rsa -f /etc/ssh/ssh_host_rsa_key -P ""
ssh-keygen -t ecdsa -f /etc/ssh/ssh_host_ecdsa_key -P ""
ssh-keygen -t ed25519 -f /etc/ssh/ssh_host_ed25519_key -P ""
/usr/sbin/sshd -D > /dev/null 2>&1 &

echo "########################### Start MariaDB ######################################"
mysql_install_db --user=root --basedir=/usr --datadir=/var/lib/mysql
mariadbd --socket=/var/lib/mysql/mysql.sock --user=root > /dev/null 2>&1 &
sleep 5

echo "########################### Init centreon database ############################"

mysql -e "CREATE USER IF NOT EXISTS 'centreon'@'localhost' IDENTIFIED BY 'centreon';"

mysql -e "GRANT SELECT,UPDATE,DELETE,INSERT,CREATE,DROP,INDEX,ALTER,LOCK TABLES,CREATE TEMPORARY TABLES, EVENT,CREATE VIEW ON *.* TO  'centreon'@'localhost';"
mysql -e "CREATE USER IF NOT EXISTS 'root_centreon'@'localhost' IDENTIFIED BY 'centreon';"
mysql -e "GRANT ALL PRIVILEGES ON *.* TO 'root_centreon'@'localhost'"

cat resources/centreon.sql | sed "s/DBNameConf/centreon/g" > /tmp/centreon.sql

mysql -u centreon -pcentreon < resources/centreon_storage.sql
mysql -u centreon -pcentreon < /tmp/centreon.sql

echo "########################## Install centreon collect ###########################"

echo "Installation..."
/usr/bin/rpm -Uvvh --force --nodeps *.rpm

echo "########################### Install Robot Framework ###########################"
cd tests
pip3 install -U robotframework robotframework-databaselibrary pymysql python-dateutil

yum groupinstall "Development Tools" -y
yum install python3-devel -y

pip3 install grpcio grpcio_tools

echo "##### site-packages #####"
find / -name site-packages

echo "##### grpc #####"
find / -name grpc

echo "##### Starting tests #####"
./init-proto.sh

echo "####################### Run Centreon Collect Robot Tests #######################"
robot $1


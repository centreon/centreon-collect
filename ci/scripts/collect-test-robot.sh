#!/bin/bash
set -e
set -x

export RUN_ENV=docker

echo "########################### Configure and start sshd ###########################"
ssh-keygen -t rsa -f /etc/ssh/ssh_host_rsa_key -P ""
ssh-keygen -t ecdsa -f /etc/ssh/ssh_host_ecdsa_key -P ""
ssh-keygen -t ed25519 -f /etc/ssh/ssh_host_ed25519_key -P ""
/usr/sbin/sshd > /dev/null 2>&1 &

echo "########################### Start MariaDB ######################################"
mariadbd --user=root > /dev/null 2>&1 &
sleep 5

echo "########################### Init centreon database ############################"

mysql -e "CREATE USER IF NOT EXISTS 'centreon'@'localhost' IDENTIFIED BY 'centreon';"

mysql -e "GRANT SELECT,UPDATE,DELETE,INSERT,CREATE,DROP,INDEX,ALTER,LOCK TABLES,CREATE TEMPORARY TABLES, EVENT,CREATE VIEW ON *.* TO  'centreon'@'localhost';"

mysql -u centreon -pcentreon < resources/centreon_storage.sql
mysql -u centreon -pcentreon < resources/centreon.sql

echo "########################## Install centreon collect ###########################"

echo "Here are the rpm files to install"
ls *.rpm

echo "Installation..."
/usr/bin/rpm -Uvvh --force --nodeps $(find $(pwd) -name '*.rpm')

echo "########################### install robot framework ############################"
cd /src/tests/
pip3 install -U robotframework robotframework-databaselibrary pymysql python-dateutil

yum install "Development Tools" python3-devel -y

pip3 install grpcio==1.33.2 grpcio_tools==1.33.2

./init-proto.sh

echo "########################### run centreon collect test robot ############################"
cd /src/tests/
robot --nostatusrc .

echo "########################### generate folder report ############################"
mkdir reports
cp log.html output.xml report.html reports


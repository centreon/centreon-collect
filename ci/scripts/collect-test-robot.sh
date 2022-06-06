#!/bin/bash
set -e

USER = $1
TOKEN = $2
BRANCH = $3
export RUN_ENV=docker
echo "########################### configure and start sshd ############################"
ssh-keygen -t rsa -f /etc/ssh/ssh_host_rsa_key
ssh-keygen -t ecdsa -f /etc/ssh/ssh_host_ecdsa_key
ssh-keygen -t ed25519 -f /etc/ssh/ssh_host_ed25519_key
/usr/sbin/sshd > /dev/null 2>&1 &

echo "########################### start mariadb ############################"
mariadbd --user=root > /dev/null 2>&1 &
sleep 5

echo "########################### init centreon database ############################"

cd /src/tests/
mysql -e "CREATE USER IF NOT EXISTS 'centreon'@'localhost' IDENTIFIED BY 'centreon';"

mysql -e "GRANT SELECT,UPDATE,DELETE,INSERT,CREATE,DROP,INDEX,ALTER,LOCK TABLES,CREATE TEMPORARY TABLES, EVENT,CREATE VIEW ON *.* TO  'centreon'@'localhost';"

mysql -u centreon -pcentreon < resources/centreon_storage.sql
mysql -u centreon -pcentreon < resources/centreon.sql

echo "########################### download and install centreon collect ############################"

curl https://$USER:$TOKEN@jenkins.int.centreon.com/job/$BRANCH/lastSuccessfulBuild/artifact/*zip*/myfile.zip --output artifact.zip
unzip artifact.zip
cd artifact
rpm -i centreon*.el7.x86_64.rpm

#Build
make -j9
make -j9 install

echo "########################### install robot framework ############################"
cd /src/tests/
pip3 install -U robotframework robotframework-databaselibrary pymysql

yum install "Development Tools" python3-devel -y

pip3 install grpcio==1.33.2 grpcio_tools==1.33.2

./init-proto.sh

echo "########################### run centreon collect test robot ############################"
cd /src/tests/
robot --nostatusrc .

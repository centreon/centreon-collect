#!/bin/bash
set -e

export RUN_ENV=docker

echo "########################### Configure and start sshd ###########################"
ssh-keygen -t rsa -f /etc/ssh/ssh_host_rsa_key
ssh-keygen -t ecdsa -f /etc/ssh/ssh_host_ecdsa_key
ssh-keygen -t ed25519 -f /etc/ssh/ssh_host_ed25519_key
/usr/sbin/sshd > /dev/null 2>&1 &

echo "########################### Start MariaDB ######################################"
mariadbd --user=root > /dev/null 2>&1 &
sleep 5

echo "########################### Init centreon database ############################"

cd /src/tests/
mysql -e "CREATE USER IF NOT EXISTS 'centreon'@'localhost' IDENTIFIED BY 'centreon';"

mysql -e "GRANT SELECT,UPDATE,DELETE,INSERT,CREATE,DROP,INDEX,ALTER,LOCK TABLES,CREATE TEMPORARY TABLES, EVENT,CREATE VIEW ON *.* TO  'centreon'@'localhost';"

mysql -u centreon -pcentreon < resources/centreon_storage.sql
mysql -u centreon -pcentreon < resources/centreon.sql

echo "########################## Install centreon collect ###########################"

#if [[ -d /src/build ]] ; then
#  rm -rf /src/build
#fi
#
#mkdir /src/build
#cd /src/build
#DISTRIB=$(lsb_release -rs | cut -f1 -d.)
#if [ "$DISTRIB" = "7" ] ; then
#    source /opt/rh/devtoolset-9/enable
#fi
##conan install .. -s compiler.cppstd=14 -s compiler.libcxx=libstdc++11 --build=missing
#if [ $(cat /etc/issue | awk '{print $1}') = "Debian" ] ; then
#    CXXFLAGS="-Wall -Wextra" cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_MODULE_SIMU=On -DWITH_BENCH=On -DWITH_CREATE_FILES=OFF ..
#else 
#    CXXFLAGS="-Wall -Wextra" cmake3 -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_MODULE_SIMU=On -DWITH_BENCH=On -DWITH_CREATE_FILES=OFF ..
#fi
#
##Build
#make -j9
#make -j9 install

rpm -Uvh --force --nodeps /tmp/packages/*.rpm

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


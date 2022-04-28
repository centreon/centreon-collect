#!/bin/bash
set -e

echo "########################### build and install centreon collect ############################"

rm -rf /src/build
mkdir /src/build
cd /src/build/
DISTRIB=$(lsb_release -rs | cut -f1 -d.)
if [ "$DISTRIB" = "7" ] ; then
    source /opt/rh/devtoolset-9/enable
fi 
conan install .. -s compiler.cppstd=14 -s compiler.libcxx=libstdc++11 --build=missing
if [ $(cat /etc/issue | awk '{print $1}') = "Debian" ] ; then
    CXXFLAGS="-Wall -Wextra" cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_MODULE_SIMU=On -DWITH_BENCH=On -DWITH_CREATE_FILES=OFF ..
else 
    CXXFLAGS="-Wall -Wextra" cmake3 -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_MODULE_SIMU=On -DWITH_BENCH=On -DWITH_CREATE_FILES=OFF ..
fi

#Build
make -j9
make -j9 install

echo "########################### start mariadb ############################"
# mysql_install_db --user=root --ldata=/var/lib/mysql/
mariadbd --user=root --verbose &
ps ax
sleep 5

echo "########################### init centreon database ############################"

cd /src/tests/
mysql -e "CREATE USER IF NOT EXISTS 'centreon'@'localhost' IDENTIFIED BY 'centreon';"

mysql -e "GRANT SELECT,UPDATE,DELETE,INSERT,CREATE,DROP,INDEX,ALTER,LOCK TABLES,CREATE TEMPORARY TABLES, EVENT,CREATE VIEW ON *.* TO  'centreon'@'localhost';"

# mysql -u centreon -pcentreon -e "DROP DATABASE centreon_storage;"
# mysql -u centreon -pcentreon -e "DROP DATABASE centreon;"

mysql -u centreon -pcentreon < resources/centreon_storage.sql
mysql -u centreon -pcentreon < resources/centreon.sql

echo "########################### install robot framework ############################"
cd /src/tests/
pip3 install -U robotframework robotframework-databaselibrary pymysql

yum install "Development Tools" python3-devel -y

pip3 install grpcio==1.33.2 grpcio_tools==1.33.2

./init-proto.sh

echo "########################### run centreon collect test robot ############################"
cd /src/tests/
robot .

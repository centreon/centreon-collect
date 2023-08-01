#!/bin/bash
set -e
set -x

export RUN_ENV=docker

test_file=$1
database_type=$2
#this env variable is a json that contains some test params
export TESTS_PARAMS='$3'

. /etc/os-release
distrib=${ID}
distrib=$(echo $distrib | tr '[:lower:]' '[:upper:]')

#cpu=$(lscpu | awk '$1 ~ "Architecture" { print $2 }')
if [[ "$test_file" =~ "unstable" ]] ; then
  exit 0
fi

if [ ${database_type} == 'mysql' ] && [ ! -f tests/${test_file}.mysql ]; then
    echo > tests/log.html
    echo '<?xml version="1.0" encoding="UTF-8"?>' > tests/output.xml
    echo '<robot generator="Robot 6.0.2 (Python 3.9.14 on linux)" generated="20230517 15:35:12.235" rpa="false" schemaversion="3"></robot>' >> tests/output.xml
    echo > tests/report.html
    exit 0
fi

echo "###########################  start sshd ###########################"
/usr/sbin/sshd -D  &

if [ $database_type == 'mysql' ]; then
    echo "########################### Start MySQL ######################################"
    /usr/libexec/mysqldtoto --user=root &
else
    echo "########################### Start MariaDB ######################################"
    if [ "$distrib" = "ALMALINUX" ]; then
      mariadbd --socket=/var/lib/mysql/mysql.sock --user=root > /dev/null 2>&1 &
    else
      mariadbd --socket=/run/mysqld/mysqld.sock --user=root > /dev/null 2>&1 &
    fi
    sleep 5

fi

mysql -e "GRANT SELECT,UPDATE,DELETE,INSERT,CREATE,DROP,INDEX,ALTER,LOCK TABLES,CREATE TEMPORARY TABLES, EVENT,CREATE VIEW ON *.* TO  'centreon'@'localhost';"
mysql -e "GRANT ALL PRIVILEGES ON *.* TO 'root_centreon'@'localhost'"

cat resources/centreon.sql | sed "s/DBNameConf/centreon/g" > /tmp/centreon.sql

mysql -u root_centreon -pcentreon < resources/centreon_storage.sql
mysql -u root_centreon -pcentreon < /tmp/centreon.sql

#remove git dubious ownership
git config --global --add safe.directory $PWD

echo "########################### Install Robot Framework ###########################"
cd tests
pip3 install -U robotframework robotframework-databaselibrary robotframework-httpctrl RobotFramework-Examples pymysql python-dateutil psutil

if [ "$distrib" = "ALMALINUX" ]; then
  dnf groupinstall -y "Development Tools"
  dnf install -y python3-devel
else
  apt-get update
  apt-get install -y build-essential
  apt-get install -y python3-dev
fi

pip3 install grpcio grpcio_tools py-cpuinfo cython unqlite gitpython boto3

echo "########################## Install centreon collect ###########################"
echo "Installation..."
if [ "$distrib" = "ALMALINUX" ]; then
  dnf clean all
  rm -f ./*-selinux-*.rpm # avoid to install selinux packages which are dependent to centreon-common-selinux
  dnf install -y ./*.rpm
else
  apt-get update
  apt-get install -y ./*.deb
fi


ulimit -c unlimited
echo '/tmp/core.%p' > /proc/sys/kernel/core_pattern

#remove git dubious ownership
/usr/bin/git config --global --add safe.directory $PWD

echo "##### Starting tests #####"
cd tests
./init-proto.sh

echo "####################### Run Centreon Collect Robot Tests #######################"
robot -e unstable $test_file

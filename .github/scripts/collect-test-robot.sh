#!/bin/bash
set -e
set -x

export RUN_ENV=docker

test_file=$1
database_type=$2

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

ulimit -c unlimited
echo '/tmp/core.%p' > /proc/sys/kernel/core_pattern

echo $(pwd)
echo "##########################################  Tests 1 ##########################################"
ls -al
echo "##########################################  Tests 2 ##########################################"
ls -al /
echo "##########################################  Tests 3 ##########################################"
ls -al /usr
echo "##########################################  Tests 4 ##########################################"
ls -al /usr/bin


#remove git dubious ownership
/usr/bin/git config --global --add safe.directory $PWD

echo "##### Starting tests #####"
cd tests
./init-proto.sh

echo "####################### Run Centreon Collect Robot Tests #######################"
robot -e unstable $test_file

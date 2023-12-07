#!/bin/bash
set -e
set -x

export RUN_ENV=docker

test_file=$1
distrib=$2

if [[ "$test_file" =~ "unstable" ]] ; then
  exit 0
fi

echo "###########################  start sshd ###########################"
/usr/sbin/sshd -D  &

echo "########################### Start MariaDB ######################################"
if [ "$distrib" != "bullseye" ]; then
  mariadbd --socket=/var/lib/mysql/mysql.sock --user=root > /dev/null 2>&1 &
else
  mariadbd --socket=/run/mysqld/mysqld.sock --user=root > /dev/null 2>&1 &
fi
sleep 5


echo "########################## Install centreon collect ###########################"
echo "Installation..."
if [ "$distrib" != "bullseye" ]; then
  dnf clean all
  rm -f ./*-selinux-*.rpm # avoid to install selinux packages which are dependent to centreon-common-selinux
  rpm -Uvh --nodeps --force ./*.rpm
else
  apt-get update
  apt-get install -y ./*.deb
fi


#remove git dubious ownership
/usr/bin/git config --global --add safe.directory $PWD

echo "##### Starting tests #####"
cd tests
./init-proto.sh

echo "####################### Run Centreon Collect Robot Tests #######################"
robot -e unstable $test_file

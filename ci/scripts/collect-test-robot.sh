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

echo "Installation..."
/usr/bin/rpm -Uvvh --force --nodeps *.rpm

if [ -x /usr/bin/pip3.8 ] ; then
  pip=/usr/bin/pip3.8
else
  pip=$(which pip3)
fi

echo "########################### Install Robot Framework ###########################"
cd /src/tests/
$pip install -U robotframework robotframework-databaselibrary pymysql python-dateutil

yum groupinstall "Development Tools" -y
yum install python3-devel -y

# Get OS version id
VERSION_ID=$(grep '^VERSION_ID' /etc/os-release | sed -En 's/^VERSION_ID="([[:digit:]])\.[[:digit:]]"/\1/p')

# Force version for el7 only
if [ -f /etc/os-release ]; then
    case "$VERSION_ID" in
        7)
            echo "$pip install grpcio==1.33.2 grpcio_tools==1.33.2"
            $pip install grpcio==1.33.2 grpcio_tools==1.33.2
            ;;
        8)
            echo "$pip install grpcio grpcio_tools"
            $pip install grpcio grpcio_tools
            ;;
        *)
            echo "OS Version is neither 7 or 8"
            ;;
    esac
fi

echo "##### site-packages #####"
find / -name site-packages

echo "##### grpc #####"
find / -name grpc

echo "##### Starting tests #####"
./init-proto.sh

echo "####################### Run Centreon Collect Robot Tests #######################"
cd /src/tests/
robot --nostatusrc .

echo "########################### Generate Folder Report #############################"
mkdir reports
cp log.html output.xml report.html reports


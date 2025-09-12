#!/bin/bash
set -e
set -x

export RUN_ENV=docker

test_file=$1

if [ -f "/.venv/bin/activate" ]; then
  echo "########################### activate python virtual env ###########################"
  source /.venv/bin/activate
fi

ulimit -c unlimited
ulimit -S -n 524288

echo '/tmp/core.%p' > /proc/sys/kernel/core_pattern

mysql -h mariadb -u root -ppassword -e "CREATE DATABASE \`centreon\`"
mysql -h mariadb -u root -ppassword -e "CREATE DATABASE \`centreon-storage\`"
mysql -h mariadb -u root -ppassword -e "GRANT ALL PRIVILEGES ON centreon.* TO 'centreon'@'%'"
mysql -h mariadb -u root -ppassword -e "GRANT ALL PRIVILEGES ON  \`centreon-storage\`.* TO 'centreon'@'%'"
mysql -h mariadb -u root -ppassword 'centreon' < centreon/centreon/www/install/createTables.sql
mysql -h mariadb -u root -ppassword 'centreon-storage' < centreon/centreon/www/install/createTablesCentstorage.sql

echo "##### Starting tests #####"
robot -v 'DBHOST:mariadb' -v 'DBNAME:centreon' -v 'DBNAME_STORAGE:centreon-storage' -v 'DBUSER:centreon' gorgone/tests/robot/tests/${test_file}

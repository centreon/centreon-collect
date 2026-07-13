#!/bin/bash
set -e
set -x

export RUN_ENV=docker

test_file=$1

if [ -f "/.venv/bin/activate" ]; then
  echo "########################### activate python virtual env ###########################"
  source /.venv/bin/activate
fi

# Wait for the database to be up and running.
set +e
while true ; do
  timeout 20 mysql -h${MYSQL_HOST} -uroot -ppassword -e 'SELECT User FROM user' mysql
  retval=$?
  if [ "$retval" = 0 ] ; then
    echo 'DB server is running.'
    break ;
  else
    echo 'DB server is not yet responding.'
    sleep 1
  fi
done
set -e

mysql -h ${MYSQL_HOST} -u root -ppassword -e "CREATE DATABASE \`centreon\`"
mysql -h ${MYSQL_HOST} -u root -ppassword -e "CREATE DATABASE \`centreon-storage\`"
mysql -h ${MYSQL_HOST} -u root -ppassword -e "CREATE DATABASE \`centreon-remote\`"
mysql -h ${MYSQL_HOST} -u root -ppassword -e "GRANT ALL PRIVILEGES ON centreon.* TO 'centreon'@'%'"
mysql -h ${MYSQL_HOST} -u root -ppassword -e "GRANT ALL PRIVILEGES ON  \`centreon-storage\`.* TO 'centreon'@'%'"
mysql -h ${MYSQL_HOST} -u root -ppassword -e "GRANT ALL PRIVILEGES ON  \`centreon-remote\`.* TO 'centreon'@'%'"
mysql -h ${MYSQL_HOST} -u root -ppassword 'centreon' < /usr/local/src/centreon/centreon/www/install/createTables.sql
mysql -h ${MYSQL_HOST} -u root -ppassword 'centreon-remote' < /usr/local/src/centreon/centreon/www/install/createTables.sql
mysql -h ${MYSQL_HOST} -u root -ppassword 'centreon-storage' < /usr/local/src/centreon/centreon/www/install/createTablesCentstorage.sql

echo "##### Starting tests #####"
robot -v 'DBHOST:mariadb' -v 'DBNAME:centreon' -v 'DBNAME_STORAGE:centreon-storage' -v 'DBUSER:centreon' ${test_file}

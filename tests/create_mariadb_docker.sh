#!/bin/bash

SCRIPT_DIR=$(dirname "${BASH_SOURCE[0]}")
cd ${SCRIPT_DIR}

#clean and create mariadb logs directory
mkdir mariadb_log
\rm mariadb_log/*
touch mariadb_log/mariadb.log
chmod 666 mariadb_log/mariadb.log
touch mariadb_log/error.log
chmod 666 mariadb_log/error.log
touch mariadb_log/slow_query.log
chmod 666 mariadb_log/slow_query.log

cd ..
PARENT_DIR=$(pwd)
cd ${SCRIPT_DIR}

docker run -d -p 3306:3306 --name mariadb_test --rm --env MARIADB_ROOT_PASSWORD=centreon -v ${PARENT_DIR}:/scripts -v ${PARENT_DIR}/tests/mariadb_docker_conf:/etc/mysql/conf.d mariadb:latest 
if [ $? -eq 0 ]
then
    docker exec mariadb_test /scripts/tests/init-sql-docker.sh
    if [ $? -eq 0 ]
    then
        echo "container mariadb_test started, mariadb logs in mariadb_log directory"
    fi
fi


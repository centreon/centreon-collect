#!/bin/bash

SCRIPT_DIR=$(dirname "${BASH_SOURCE[0]}")
cd ${SCRIPT_DIR}

#clean and create mysql logs directory
mkdir mysql_log
/bin/rm -f mysql_log/*
touch mysql_log/mysql.log
chmod 666 mysql_log/mysql.log
touch mysql_log/error.log
chmod 666 mysql_log/error.log
touch mysql_log/slow_query.log
chmod 666 mysql_log/slow_query.log

cd ..
PARENT_DIR=$(pwd)
cd ${SCRIPT_DIR}

docker run -d -p 3306:3306 --name mysql_test --rm --env MYSQL_ROOT_PASSWORD=centreon -v ${PARENT_DIR}:/scripts -v ${PARENT_DIR}/tests/mysql_docker_conf:/etc/mysql/conf.d mysql:latest 
if [ $? -eq 0 ]
then
    grep "ready for connections. Bind-address: '::' port: 3306" ${PARENT_DIR}/tests/mysql_log/error.log
    while [ $? -ne 0 ]
    do
        sleep 1
        grep "ready for connections. Bind-address: '::' port: 3306" ${PARENT_DIR}/tests/mysql_log/error.log
    done

    sleep 1

    docker exec mysql_test /scripts/tests/init-sql-docker.sh
    if [ $? -eq 0 ]
    then
        echo "container mysql_test started, mysql logs in mysql_log directory"
    fi
fi


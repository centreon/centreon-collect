#!/bin/bash

DBUserRoot=$(awk '($1=="${DBUserRoot}") {print $2}' resources/db_variables.robot)
DBPassRoot=$(awk '($1=="${DBPassRoot}") {print $2}' resources/db_variables.robot)

if [ -z $DBUserRoot ] ; then
    DBUserRoot="root"
fi

if [ -z $DBPassRoot ] ; then
    DBPassRoot="centreon"
fi

mysql --user="$DBUserRoot" --password="$DBPassRoot" -e "drop database centreon"
mysql --user="$DBUserRoot" --password="$DBPassRoot" < ../resources/centreon.sql
mysql --user="$DBUserRoot" --password="$DBPassRoot" < ../resources/centreon_storage.sql

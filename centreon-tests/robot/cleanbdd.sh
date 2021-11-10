#!/bin/sh
# Purge Centreon Log
# Auteur :      guillaume
# Date :        04/05/2015
# Version :     1.0
# MaJ :
 
#####################################
# Config Options : To be modified   #
#####################################
BDD_IP="localhost"
BDD_LOGIN="centreon"
BDD_PWD="centreon"
OPTI_TABLE="N" # Y / N
DATA_TO_KEEP="2" # to see the format option, see https://dev.mysql.com/doc/refman/4.1/en/date-and-time-functions.html#function_date-add
 
#####################################
# Variables     : Do not modified   #
#####################################
DBNAME="centreon_storage"
 
#####################################
# Purge Log                         #
#####################################
echo "Optimization started @ `date +%H:%M:%S`"
echo "Keeping $DATA_TO_KEEP month of data"
echo ""
 
# Delete log entries
echo "Deleting old entries"
purge_req="delete from $DBNAME.logs where date(FROM_UNIXTIME(ctime)) < date_add(curdate(),interval -$DATA_TO_KEEP month)"
mysql -h $BDD_IP -u $BDD_LOGIN -p$BDD_PWD -e "$purge_req"
 
# Optimize log table ?
if [ "$OPTI_TABLE" == "Y" ]; then
                echo ""
                echo "Optimizing log table"
                opti_req="optimize no_write_to_binlog table $DBNAME.log"
                mysql -h $BDD_IP -u $BDD_LOGIN -p$BDD_PWD -e "$opti_req"
elif [ "$OPTI_TABLE" == "N" ]; then
        echo ""
        echo "Log table Optimization not activate"
fi
 
echo "Optimization ended @ `date +%H:%M:%S`"
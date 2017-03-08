#!/bin/bash

USER=$(grep '\'user\' /etc/centreon/centreon.conf.php | awk -F= '{ print $2 }' | sed -e 's/[;" ]//g')
PASSWORD=$(grep '\'password\' /etc/centreon/centreon.conf.php | awk -F= '{ print $2 }' | sed -e "s/[;' ]//g")
DB_CENTREON=$(grep '\'db\' /etc/centreon/centreon.conf.php | awk -F= '{ print $2 }' | sed -e 's/[;" ]//g')

FILE_PATH="/etc/centreon-broker/"
HASH_PATH="/etc/centreon/"
config_name_files=( "centreon-poller-display" "bam-poller-display" )
extend_save='-save.txt'
extend_sql='.sql'

for ((i=0; i < ${#config_name_files[@]}; i++))
do
SQL_FILE=${FILE_PATH}${config_name_files[$i]}${extend_sql}
    if [ -f ${SQL_FILE} ] ; then
    HASH_FILE=${HASH_PATH}${config_name_files[$i]}${extend_save}
        if [ -f ${HASH_FILE} ] ; then
            hash_content=`cat ${HASH_FILE}`
        else
            hash_content=''
        fi
    file_content=`cat ${SQL_FILE}`
    hashFile=`md5sum < ${HASH_FILE}`
        if [ "${hash_content}" != "${hashFile}" ] ; then
            mysql --user="$USER" --password="$PASSWORD" --database="$DB_CENTREON" --execute="$file_content"
            echo ${hashFile} > ${HASH_FILE}
            service cbd restart
        fi
   fi
done

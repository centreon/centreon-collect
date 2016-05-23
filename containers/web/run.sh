#!/bin/sh

#set -e
set -x

service mysql start
while true ; do
  retval=`mysql centreon -e 'SELECT * FROM nagios_server' > /dev/null ; echo $?`
  if [ "$retval" = 0 ] ; then
    break ;
  else
    echo 'DB server is not yet available.'
    sleep 1
  fi
done
mysql -e "GRANT ALL ON *.* to root@'%' IDENTIFIED BY 'centreon'"
service cbd start
service centengine start
service centcore start
httpd -k start
tailf /var/log/httpd/error_log

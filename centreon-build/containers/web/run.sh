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
service cbd start
service centengine start
service centcore start
service httpd start
bash
service httpd stop
service centcore stop
service centengine stop
service cbd stop
service mysql stop

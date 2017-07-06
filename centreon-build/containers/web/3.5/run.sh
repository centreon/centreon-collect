#!/bin/sh

#set -e
set -x

# Start database server.
service mysql start

# Wait for the database to be up and running.
while true ; do
  timeout 10 mysql -e 'SELECT * FROM nagios_server' centreon
  retval=$?
  if [ "$retval" = 0 ] ; then
    break ;
  else
    echo 'DB server is not yet responding.'
    sleep 1
  fi
done

# Generate configuration
centreon -d -u admin -p centreon -a POLLERGENERATE -v 1
centreon -d -u admin -p centreon -a CFGMOVE -v 1

# Start Centreon services.
/etc/init.d/cbd start
/etc/init.d/centengine start
su - centreon -c centcore &

# Start Centreon Web.
httpd -k start

# Print logs.
tailf /var/log/httpd/error_log

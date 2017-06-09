#!/bin/sh

#set -e
set -x

# Start database server.
service mysql start

# Start Centreon services.
/etc/init.d/cbd start
/etc/init.d/centengine start
su - centreon -c centcore &

# Fix to allow Centreon Web to use our special init.d script and not
# call this cumbersome init system.
rm -rf /etc/systemd

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

# Export configuration of poller.
while true ; do
  timeout 10 nc -w 1 poller 22
  retval=$?
  if [ "$retval" = 0 ] ; then
    break ;
  else
    echo "Poller's SSH service is not yet responding."
    sleep 1
  fi
done
centreon -u admin -p centreon -a APPLYCFG -v 'Poller'

# Start Centreon Web.
httpd -k start

# Print logs.
tailf /var/log/httpd/error_log

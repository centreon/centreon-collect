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

# Start Centreon Web.
httpd -k start

# Wait for Centreon Web to be up and running.
while true ; do
  timeout -k 20 15 centreon -u admin -p centreon -o contact -a show
  retval=$?
  if [ "$retval" = 0 ] ; then
    break ;
  else
    echo 'Centreon Web is not yet available.'
    sleep 1
  fi
done

# Print logs.
tailf /var/log/httpd/error_log

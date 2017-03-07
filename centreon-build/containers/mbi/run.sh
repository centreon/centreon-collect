#!/bin/sh

#set -e
set -x

# Run MySQL.
service mysql start

# Wait for MySQL to be started.
while true ; do
  timeout 10 mysql -e 'SELECT NOW()'
  retval=$?
  if [ "$retval" = 0 ] ; then
    break ;
  else
    echo 'DB server is not yet responding.'
    sleep 1
  fi
done

# Install the reporting schema.
/usr/share/centreon-bi/bin/centreonBIETL -c

# Start cbis.
service cbis start

while true ; do
  sleep 10
done

# Stop cbis.
service cbis stop

# Stop MySQL.
service mysql stop

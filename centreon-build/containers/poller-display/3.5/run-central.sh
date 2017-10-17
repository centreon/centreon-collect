#!/bin/sh

# Wait for poller to be available.
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
sleep 5
centreon -u admin -p centreon -a APPLYCFG -v 'Poller'

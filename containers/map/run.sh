#!/bin/sh

set -x

# Run Tomcat differently for CentOS 6/7.
ISCENTOS7=`lsb_release -r | cut -f 2 | grep -e '^7\.'`
if [ \! -z "$ISCENTOS7" ] ; then
  /usr/libexec/tomcat/server start &
else
  service tomcat6 start
fi

# Wait for log file to be created.
while [ \! -e /var/log/centreon-studio/centreon-studio.log ] ; do
  echo "Waiting for Centreon Studio log file..."
  sleep 2
done
tailf /var/log/centreon-studio/centreon-studio.log

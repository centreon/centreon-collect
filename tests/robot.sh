#!/bin/bash

getent group centreon-engine || groupadd centreon-engine

sed -i -r 's/(\$\{DBUserRoot\}\s*)root_centreon/\1root/g' resources/db_variables.resource
ulimit -c unlimited
sysctl -w kernel.core_pattern=/tmp/core-%e.%p.%h.%t

robot -L TRACE $*
rep=$(date +%s)
mkdir $rep
mv report.html log.html output.xml $rep
if [ -f processing.log ] ; then
  mv processing.log $rep
fi

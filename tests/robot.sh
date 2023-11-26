#!/bin/bash

sed -i -r 's/(\$\{DBUserRoot\}\s*)root_centreon/\1root/g' resources/db_variables.robot
ulimit -c unlimited
sysctl -w kernel.core_pattern=/tmp/core-%e.%p.%h.%t

if [ -n $CENTENGINE_LEGACY ] ; then
  export CENTENGINE_LEGACY=0
fi
echo "CENTENGINE_LEGACY=$CENTENGINE_LEGACY"

robot -L TRACE $*
retval=$?
#robot $*
rep=$(date +%s)
mkdir $rep
mv report.html log.html output.xml $rep
if [ -f processing.log ] ; then
  mv processing.log $rep
fi
exit $retval

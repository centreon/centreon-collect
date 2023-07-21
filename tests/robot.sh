#!/bin/bash

sed -i -r 's/(\$\{DBUserRoot\}\s*)root_centreon/\1root/g' resources/db_variables.robot
ulimit -c unlimited
sysctl -w kernel.core_pattern=/tmp/core-%e.%p.%h.%t

export CENTENGINE_LEGACY=0

#robot -L TRACE $*
robot $*
rep=$(date +%s)
mkdir $rep
mv report.html log.html output.xml $rep
if [ -f processing.log ] ; then
  mv processing.log $rep
fi

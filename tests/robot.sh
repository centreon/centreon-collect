#!/bin/bash

ulimit -c unlimited
sysctl -w kernel.core_pattern=/tmp/core-%e.%p.%h.%t

robot $*
rep=$(date +%s)
mkdir $rep
mv report.html log.html output.xml $rep
if [ -f processing.log ] ; then
  mv processing.log $rep
fi

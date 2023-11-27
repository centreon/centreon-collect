#!/bin/bash

token=$(curl -H "Content-Type: application/json" -X POST --data '{ "client_id": "$XRAY_CLIENT_ID","client_secret": "$XRAY_CLIENT_SECRET" }'  https://xray.cloud.getxray.app/api/v2/authenticate| tr -d '"')


sed -i -r 's/(\$\{DBUserRoot\}\s*)root_centreon/\1root/g' resources/db_variables.robot
ulimit -c unlimited
sysctl -w kernel.core_pattern=/tmp/core-%e.%p.%h.%t

robot -L TRACE $*

curl -H "Content-Type: text/xml" -X POST -H "Authorization: Bearer $token"  --data @"output.xml" https://xray.cloud.getxray.app/api/v2/import/execution/robot?projectKey=MON

rep=$(date +%s)
mkdir $rep
mv report.html log.html output.xml $rep
if [ -f processing.log ] ; then
  mv processing.log $rep
fi

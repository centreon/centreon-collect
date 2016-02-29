#!/bin/sh

# Pull monitoring-ppe-running container.
docker pull ci.int.centreon.com:5000/monitoring-ppe:centos6

# Start a container.
containerid=`docker run -d -t -p 80 ci.int.centreon.com:5000/monitoring-ppe:centos6`
port=`docker port "$containerid" 80 | cut -d : -f 2`

# Copy acceptance tests from container to host.
docker cp "$containerid:/usr/local/src/centreon-export" .

# Prepare for acceptance tests run.
cd centreon-export/www/modules/centreon-export
sed 's/@CENTREONSERVER@/localhost:$port/g' < behat.yml.in > behat.yml

# Run acceptance tests.
/opt/behat/vendor/bin/behat --format junit --out ../../../../mon-ppe-acceptance.xml

# Stop container.
docker stop "$containerid"

#!/bin/sh

# Launch monitoring-unitttest container.
docker pull 10.24.11.199:5000/monitoring-unittest:centos6
containerid=`docker create 10.24.11.199:5000/monitoring-unittest:centos6 /usr/local/bin/unittest-lm`

# Copy sources to container.
docker cp centreon-license-manager "$containerid:/usr/local/src/centreon-license-manager"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/centreon-license-manager.xml" centreon-license-manager.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

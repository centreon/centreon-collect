#!/bin/sh

# Launch monitoring-unittest container.
docker pull 10.24.11.199:5000/monitoring-unittest:centos6
containerid=`docker create 10.24.11.199:5000/monitoring-unittest:centos6 /usr/local/bin/unittest-engine`

# Copy sources to container.
docker cp centreon-engine "$containerid:/usr/local/src/centreon-engine"

# Build project and run unit tests.
docker start -a "$containerid"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

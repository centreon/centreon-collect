#!/bin/sh

# Launch mon-unittest container.
docker pull ci.int.centreon.com:5000/mon-unittest:centos6
containerid=`docker create ci.int.centreon.com:5000/mon-unittest:centos6 /usr/local/bin/unittest-broker`

# Copy sources to container.
docker cp centreon-broker "$containerid:/usr/local/src/centreon-broker"

# Build project and run unit tests.
docker start -a "$containerid"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

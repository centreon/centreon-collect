#!/bin/sh

set -e
set -x

# Launch mon-unittest container.
docker pull ci.int.centreon.com:5000/mon-unittest:centos7
containerid=`docker create ci.int.centreon.com:5000/mon-unittest:centos7 /usr/local/bin/unittest-engine`

# Copy sources to container.
docker cp centreon-engine "$containerid:/usr/local/src/centreon-engine"

# Build project and run unit tests.
docker start -a "$containerid"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

#!/bin/sh

set -e
set -x

# Launch monitoring-unitttest container.
docker pull ci.int.centreon.com:5000/mon-unittest:centos6
containerid=`docker create ci.int.centreon.com:5000/mon-unittest:centos6 /usr/local/bin/unittest-lm`

# Copy sources to container.
docker cp centreon-license-manager "$containerid:/usr/local/src/centreon-license-manager"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/centreon-license-manager.xml" centreon-license-manager.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

#!/bin/sh

set -e
set -x

# Launch mon-unittest container.
docker pull ci.int.centreon.com:5000/mon-unittest:centos7
containerid=`docker create ci.int.centreon.com:5000/mon-unittest:centos7 /usr/local/bin/unittest-middleware`

# Copy sources to container.
docker cp centreon-imp-portal-api "$containerid:/usr/local/src/centreon-imp-portal-api"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/centreon-middleware_ut.xml" centreon-middleware_ut.xml
docker cp "$containerid:/tmp/centreon-middleware_coverage.xml" centreon-middleware_coverage.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

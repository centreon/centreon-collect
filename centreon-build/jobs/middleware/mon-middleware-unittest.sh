#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Launch mon-unittest container.
docker pull ci.int.centreon.com:5000/mon-unittest:centos$CENTOS_VERSION
containerid=`docker create ci.int.centreon.com:5000/mon-unittest:centos$CENTOS_VERSION /usr/local/bin/unittest-middleware`

# Copy sources to container.
docker cp centreon-imp-portal-api "$containerid:/usr/local/src/centreon-imp-portal-api"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/centreon-middleware_ut.xml" centreon-middleware_ut.xml
docker cp "$containerid:/tmp/centreon-middleware_coverage.xml" centreon-middleware_coverage.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

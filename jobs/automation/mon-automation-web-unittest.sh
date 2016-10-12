#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7"
  exit 1
fi
DISTRIB="$1"

# Launch mon-unittest container.
UT_IMAGE=ci.int.centreon.com:5000/mon-unittest:$DISTRIB
docker pull $UT_IMAGE
containerid=`docker create $UT_IMAGE /usr/local/bin/unittest-automation-web`

# Copy sources to container.
docker cp centreon-automation "$containerid:/usr/local/src/centreon-automation"

# Run unit tests.
docker start -a "$containerid"
#docker cp "$containerid:/tmp/ut.xml" ut.xml
#docker cp "$containerid:/tmp/coverage.xml" coverage.xml
docker cp "$containerid:/tmp/codestyle.xml" codestyle.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

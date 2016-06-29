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
containerid=`docker create $UT_IMAGE /usr/local/bin/unittest-web`

# Copy sources to container.
docker cp centreon-web "$containerid:/usr/local/src/centreon-web"

# Run unit tests.
docker start -a "$containerid"
#docker cp "$containerid:/tmp/centreon-web_ut.xml" centreon-web_ut.xml
#docker cp "$containerid:/tmp/centreon-web_coverage.xml" centreon-web_coverage.xml
docker cp "$containerid:/tmp/centreon-web_codestyle.xml" centreon-web_codestyle.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

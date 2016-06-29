#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Launch mon-unitttest container.
docker pull ci.int.centreon.com:5000/mon-unittest:centos$CENTOS_VERSION
containerid=`docker create ci.int.centreon.com:5000/mon-unittest:centos$CENTOS_VERSION /usr/local/bin/unittest-ppe`

# Copy sources to container.
docker cp centreon-export "$containerid:/usr/local/src/centreon-export"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/centreon-ppe_ut.xml" centreon-ppe_ut.xml
docker cp "$containerid:/tmp/centreon-ppe_coverage.xml" centreon-ppe_coverage.xml
docker cp "$containerid:/tmp/centreon-ppe_codestyle.xml" centreon-ppe_codestyle.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

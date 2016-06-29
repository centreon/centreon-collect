#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Launch monitoring-unitttest container.
docker pull ci.int.centreon.com:5000/mon-unittest:centos$CENTOS_VERSION
containerid=`docker create ci.int.centreon.com:5000/mon-unittest:centos$CENTOS_VERSION /usr/local/bin/unittest-lm`

# Copy sources to container.
docker cp centreon-license-manager "$containerid:/usr/local/src/centreon-license-manager"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/centreon-license-manager_ut.xml" centreon-license-manager_ut.xml
docker cp "$containerid:/tmp/centreon-license-manager_coverage.xml" centreon-license-manager_coverage.xml
docker cp "$containerid:/tmp/centreon-license-manager_codestyle.xml" centreon-license-manager_codestyle.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

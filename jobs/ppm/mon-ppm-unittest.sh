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
containerid=`docker create ci.int.centreon.com:5000/mon-unittest:centos$CENTOS_VERSION /usr/local/bin/unittest-ppm`

# Copy sources to container.
docker cp centreon-import "$containerid:/usr/local/src/centreon-import"

# Run unit tests.
docker start -a "$containerid"
# docker cp "$containerid:/tmp/centreon-ppm_ut.xml" centreon-ppm_ut.xml
# docker cp "$containerid:/tmp/centreon-ppm_coverage.xml" centreon-ppm_coverage.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

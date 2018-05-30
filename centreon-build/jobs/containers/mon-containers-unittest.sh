#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Check arguments.
if [ "$#" -lt 2 ] ; then
  echo "USAGE: $0 <version> <distrib>"
  exit 1
fi
VERSION="$1"
DISTRIB="$2"

# Prepare Dockerfile.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < unittest/Dockerfile.in > unittest/Dockerfile

# Build image.
UNITTEST_IMG="ci.int.centreon.com:5000/mon-unittest:$DISTRIB"
docker pull "ci.int.centreon.com:5000/mon-dependencies:$DISTRIB"
docker build --no-cache -t "$UNITTEST_IMG" -f unittest/Dockerfile .
docker push "$UNITTEST_IMG"

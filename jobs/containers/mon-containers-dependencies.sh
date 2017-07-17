#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <distrib>"
  exit 1
fi
DISTRIB="$1"

# Prepare Dockerfile.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < dependencies/Dockerfile.in > dependencies/Dockerfile

# Build image.
DEP_IMG="ci.int.centreon.com:5000/mon-dependencies:$DISTRIB"
docker build --no-cache -t "$DEP_IMG" -f dependencies/Dockerfile .
docker push "$DEP_IMG"

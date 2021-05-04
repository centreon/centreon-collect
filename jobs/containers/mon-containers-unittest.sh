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
cp -r ../centreon-build/containers centreon-build-containers
cd centreon-build-containers
cp unittest/Dockerfile.in unittest/Dockerfile
if [ -e "unittest/$VERSION/Dockerfile.post.$DISTRIB.in" ] ; then
  cat "unittest/$VERSION/Dockerfile.post.$DISTRIB.in" >> unittest/Dockerfile
fi
sed -i -e "s/@VERSION@/$VERSION/g" -e "s/@DISTRIB@/$DISTRIB/g" unittest/Dockerfile

# Build image.
UNITTEST_IMG="registry.centreon.com/mon-unittest-$VERSION:$DISTRIB"
docker pull "registry.centreon.com/mon-dependencies-$VERSION:$DISTRIB"
docker build --no-cache -t "$UNITTEST_IMG" -f unittest/Dockerfile .
docker push "$UNITTEST_IMG"

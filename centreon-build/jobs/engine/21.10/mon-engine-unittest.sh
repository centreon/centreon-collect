#!/bin/sh

set -e

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-engine

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|...>"
  exit 1
fi
DISTRIB="$1"

# Remove old report files.
rm -f ut.xml

# Fetch sources.
rm -rf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
get_internal_source "engine/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Launch mon-unittest container.
UNITTEST_IMAGE=registry.centreon.com/mon-unittest-21.10:$DISTRIB
docker pull $UNITTEST_IMAGE
containerid=`docker create $UNITTEST_IMAGE /usr/local/bin/unittest-engine $DISTRIB`

# Copy sources to container.
docker cp "$PROJECT-$VERSION" "$containerid:/usr/local/src/$PROJECT"
docker cp `dirname $0`/unittest.sh "$containerid:/usr/local/bin/unittest-engine"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/ut.xml" ut.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

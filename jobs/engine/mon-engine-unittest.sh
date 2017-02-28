#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Remove old report files.
rm -f ut.xml

# Fetch sources.
rm -f "centreon-engine-$VERSION.tar.gz"
wget "http://srvi-repo.int.centreon.com/sources/internal/centreon-engine-$VERSION-$RELEASE/centreon-engine-$VERSION.tar.gz"
tar xzf "centreon-engine-$VERSION.tar.gz"

# Launch mon-unittest container.
UNITTEST_IMAGE=ci.int.centreon.com:5000/mon-unittest:$DISTRIB
docker pull $UNITTEST_IMAGE
containerid=`docker create $UNITTEST_IMAGE /usr/local/bin/unittest-engine`

# Copy sources to container.
docker cp "centreon-engine-$VERSION" "$containerid:/usr/local/src/centreon-engine"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/ut.xml" ut.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

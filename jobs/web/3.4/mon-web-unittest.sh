#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7"
  exit 1
fi
DISTRIB="$1"

# Fetch sources.
rm -f "centreon-$VERSION.tar.gz"
get_internal_source "web/centreon-web-$VERSION-$RELEASE/centreon-$VERSION.tar.gz"
tar xzf "centreon-$VERSION.tar.gz"

# Launch mon-unittest container.
UT_IMAGE=ci.int.centreon.com:5000/mon-unittest:$DISTRIB
docker pull $UT_IMAGE
containerid=`docker create $UT_IMAGE /usr/local/bin/unittest-phing centreon-web`

# Copy sources to container.
docker cp "centreon-$VERSION" "$containerid:/usr/local/src/centreon-web"

# Run unit tests.
docker start -a "$containerid"
#docker cp "$containerid:/tmp/ut.xml" ut.xml
#docker cp "$containerid:/tmp/coverage.xml" coverage.xml
docker cp "$containerid:/tmp/codestyle.xml" codestyle.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

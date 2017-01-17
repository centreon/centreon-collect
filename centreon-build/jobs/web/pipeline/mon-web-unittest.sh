#!/bin/sh

set -e
set -x

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
wget "http://srvi-repo.int.centreon.com/sources/internal/centreon-web-$VERSION-$RELEASE/centreon-web-$VERSION.tar.gz"
tar xzf "centreon-web-$VERSION.tar.gz"

# Launch mon-unittest container.
UT_IMAGE=ci.int.centreon.com:5000/mon-unittest:$DISTRIB
docker pull $UT_IMAGE
containerid=`docker create $UT_IMAGE /usr/local/bin/unittest-web`

# Copy sources to container.
docker cp "centreon-web-$VERSION" "$containerid:/usr/local/src/centreon-web"

# Run unit tests.
docker start -a "$containerid"
#docker cp "$containerid:/tmp/centreon-web_ut.xml" ut.xml
#docker cp "$containerid:/tmp/centreon-web_coverage.xml" coverage.xml
docker cp "$containerid:/tmp/centreon-web_codestyle.xml" codestyle.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

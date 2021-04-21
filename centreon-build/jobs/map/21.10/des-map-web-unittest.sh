#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-map
PACKAGE=centreon-map-web-client

# Check arguments.
if [ -z "$VERSIONWEB" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|...>"
  exit 1
fi
DISTRIB="$1"

# Fetch sources.
rm -rf "$PROJECT-$VERSIONWEB.tar.gz" "$PROJECT-$VERSIONWEB"
get_internal_source "map-web/$PROJECT-web-$VERSIONWEB-$RELEASE/$PACKAGE-$VERSIONWEB.tar.gz"
tar xzf "$PACKAGE-$VERSIONWEB.tar.gz"

# Launch mon-unittest container.
UT_IMAGE=registry.centreon.com/mon-unittest-21.10:$DISTRIB
docker pull $UT_IMAGE
containerid=`docker create $UT_IMAGE /usr/local/bin/unittest.sh $PROJECT`

# Copy sources to container.
docker cp `dirname $0`/des-map-web-unittest.container.sh "$containerid:/usr/local/bin/unittest.sh"
docker cp "$PACKAGE-$VERSIONWEB" "$containerid:/usr/local/src/$PROJECT"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/codestyle-be.xml" codestyle-be.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

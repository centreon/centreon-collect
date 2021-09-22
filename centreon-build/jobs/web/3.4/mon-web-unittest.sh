#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-web

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7>"
  exit 1
fi
DISTRIB="$1"

# Fetch sources.
rm -f "centreon-$VERSION.tar.gz"
get_internal_source "web/$PROJECT-$VERSION-$RELEASE/centreon-$VERSION.tar.gz"
tar xzf "centreon-$VERSION.tar.gz"


####################
##### FRONTEND #####
####################

# Prepare NodeJS container.
NODEJS_IMAGE="node:6"
docker pull "$NODEJS_IMAGE"
containerid=`docker create $NODEJS_IMAGE sh /tmp/unittest.sh`

# Copy files to container.
docker cp "centreon-$VERSION" "$containerid:/usr/local/src/centreon"
docker cp `dirname $0`/unittest.sh "$containerid:/tmp/unittest.sh"

# Run unit tests and build release.
rm -rf "coverage" "centreon-release-$VERSION"
docker start -a "$containerid"
docker cp "$containerid:/usr/local/src/centreon/coverage" "coverage"
docker cp "$containerid:/usr/local/src/centreon/jest-test-results.xml" "jest-test-results.xml"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"


###################
##### BACKEND #####
###################

# Launch mon-unittest container.
UT_IMAGE=registry.centreon.com/mon-unittest-3.4:$DISTRIB
docker pull $UT_IMAGE
containerid=`docker create $UT_IMAGE /usr/local/bin/unittest-phing centreon`

# Copy sources to container.
docker cp "centreon-$VERSION" "$containerid:/usr/local/src/centreon"

# Run unit tests.
docker start -a "$containerid"
#docker cp "$containerid:/tmp/ut.xml" ut.xml
#docker cp "$containerid:/tmp/coverage.xml" coverage.xml
docker cp "$containerid:/tmp/codestyle.xml" codestyle.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

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
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Fetch sources.
rm -f "centreon-$VERSION.tar.gz"
get_internal_source "web/centreon-web-$VERSION-$RELEASE/centreon-$VERSION.tar.gz"
tar xzf "centreon-$VERSION.tar.gz"


####################
##### FRONTEND #####
####################

# Prepare NodeJS container.
NODEJS_IMAGE="node:6"
docker pull "$NODEJS_IMAGE"
containerid=`docker create $NODEJS_IMAGE /tmp/unittest.sh`

# Copy files to container.
docker cp "$PROJECT-$VERSION" "$containerid:/usr/local/src/$PROJECT"
docker cp `dirname $0`/unittest.sh "$containerid:/tmp/unittest.sh"

# Run unit tests and build release.
rm -rf "coverage" "$PROJECT-release-$VERSION.tar.gz" "$PROJECT-release-$VERSION"
docker start -a "$containerid"
docker cp "$containerid:/usr/local/src/$PROJECT/coverage" "coverage"
docker cp "$containerid:/usr/local/src/$PROJECT/build" "$PROJECT-$VERSION-release"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"


###################
##### BACKEND #####
###################

# Launch mon-unittest container.
UT_IMAGE=ci.int.centreon.com:5000/mon-unittest-3.4:$DISTRIB
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

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
  echo "USAGE: $0 <backend|frontend>"
  exit 1
fi
DISTRIB="centos7"
DEV_TYPE="$1"

# Fetch sources.
rm -rf "$PROJECT-$VERSION"
tar xzf "$PROJECT-$VERSION.tar.gz"

if [ "$DEV_TYPE" = "backend" ]; then
  tar xzf vendor.tar.gz -C "$PROJECT-$VERSION"
elif [ "$DEV_TYPE" = "frontend" ]; then
  tar xzf node_modules.tar.gz -C "$PROJECT-$VERSION"
fi

# Launch mon-unittest container.
UT_IMAGE=registry.centreon.com/mon-unittest-21.10:$DISTRIB
docker pull $UT_IMAGE
containerid=`docker create $UT_IMAGE /usr/local/bin/unittest.sh $PROJECT`

# Copy sources to container.
docker cp `dirname $0`/mon-web-unittest-$DEV_TYPE.container.sh "$containerid:/usr/local/bin/unittest.sh"
docker cp "$PROJECT-$VERSION" "$containerid:/usr/local/src/$PROJECT"

# Run unit tests.
docker start -a "$containerid"

if [ "$DEV_TYPE" = "backend" ]; then
  docker cp "$containerid:/tmp/ut-be.xml" ut-be.xml
  docker cp "$containerid:/tmp/coverage-be.xml" coverage-be.xml
  docker cp "$containerid:/tmp/codestyle-be.xml" codestyle-be.xml
  docker cp "$containerid:/tmp/phpstan.xml" phpstan.xml
elif [ "$DEV_TYPE" = "frontend" ]; then
  docker cp "$containerid:/tmp/ut-fe.xml" ut-fe.xml
  docker cp "$containerid:/tmp/codestyle-fe.xml" codestyle-fe.xml
fi

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

#!/bin/sh

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-license-manager

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
MAJOR=`echo $VERSION | cut -d . -f 1,2`

# Fetch sources.
rm -rf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
get_internal_source "lm/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
if [ "$MAJOR" -eq "20.04"] ; then
  get_internal_source "lm/$PROJECT-$VERSION-$RELEASE/hooks.tar.gz"
  tar xzf ../../../../../hooks.tar.gz
  cd ../../../../..
else
  get_internal_source "lm/$PROJECT-$VERSION-$RELEASE/frontend.tar.gz"
  tar xzf "$PROJECT-$VERSION.tar.gz"
  cd "$PROJECT-$VERSION/www/modules/centreon-license-manager"
  tar xzf ../../../../frontend.tar.gz
  cd ../../../..
fi

# Launch mon-unittest container.
UT_IMAGE=registry.centreon.com/mon-unittest-$MAJOR:$DISTRIB
docker pull $UT_IMAGE
containerid=`docker create -e VERSION=$VERSION $UT_IMAGE /usr/local/bin/unittest.sh $PROJECT`

# Copy sources to container.
docker cp `dirname $0`/lm-unittest.container.sh "$containerid:/usr/local/bin/unittest.sh"
docker cp "$PROJECT-$VERSION" "$containerid:/usr/local/src/$PROJECT"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/ut-be.xml" ut-be.xml
docker cp "$containerid:/tmp/ut-fe.xml" ut-fe.xml
docker cp "$containerid:/tmp/coverage.xml" coverage.xml
docker cp "$containerid:/tmp/codestyle-be.xml" codestyle-be.xml
docker cp "$containerid:/tmp/codestyle-fe.xml" codestyle-fe.xml
if [ "$MAJOR" -eq "21.10" -o "$MAJOR" -eq "21.04" -o "$MAJOR" -eq "20.04"] ; then
  docker cp "$containerid:/tmp/phpstan.xml" phpstan.xml
fi

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

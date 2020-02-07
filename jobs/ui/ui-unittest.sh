#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-ui

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Fetch sources.
rm -rf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
get_internal_source "ui/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Launch mon-unittest container.
UT_IMAGE=registry.centreon.com/puppeteer:latest
docker pull $UT_IMAGE
containerid=`docker create $UT_IMAGE /usr/local/bin/unittest.sh $PROJECT`

# Copy sources to container.
docker cp `dirname $0`/ui-unittest.container.sh "$containerid:/usr/local/bin/unittest.sh"
docker cp "$PROJECT-$VERSION" "$containerid:/usr/local/src/$PROJECT"

# Run unit tests.
rm -rf ut.xml codestyle.xml snapshots
docker start -a "$containerid"
docker cp "$containerid:/tmp/ut.xml" ut.xml
#docker cp "$containerid:/tmp/coverage.xml" coverage.xml
docker cp "$containerid:/tmp/codestyle.xml" codestyle.xml
docker cp "$containerid:/usr/local/src/centreon-ui/src/__image_snapshots__/__diff_output__" snapshots || true

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-hub-ui

# Fetch sources.
rm -rf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION" "$PROJECT"
get_internal_source "hub/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Prepare NodeJS container.
NODEJS_IMAGE="node:8"
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

# Send release tarball.
tar czf "$PROJECT-$VERSION-release.tar.gz" "$PROJECT-$VERSION-release"
put_internal_source "hub" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION-release.tar.gz"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

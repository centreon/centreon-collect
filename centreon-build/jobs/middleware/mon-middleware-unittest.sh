#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-middleware

# Fetch sources.
rm -rf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
get_internal_source "lm/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Launch mon-unittest container.
UT_IMAGE=ci.int.centreon.com:5000/mon-unittest:centos7
docker pull $UT_IMAGE
containerid=`docker create $UT_IMAGE /usr/local/bin/unittest-middleware`

# Copy sources to container.
docker cp "$PROJECT-$VERSION" "$containerid:/usr/local/src/$PROJECT"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/centreon-middleware_ut.xml" centreon-middleware_ut.xml
docker cp "$containerid:/tmp/centreon-middleware_coverage.xml" centreon-middleware_coverage.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"
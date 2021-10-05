#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-imp-portal-api

# Fetch sources.
rm -rf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
get_internal_source "middleware/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Launch mon-unittest container.
UT_IMAGE=registry.centreon.com/mon-unittest:centos7
docker pull $UT_IMAGE
containerid=`docker create $UT_IMAGE /usr/local/bin/unittest-middleware`

# Copy sources to container.
docker cp "$PROJECT-$VERSION" "$containerid:/usr/local/src/centreon-imp-portal-api"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/centreon-middleware_ut.xml" centreon-middleware_ut.xml
#docker cp "$containerid:/tmp/centreon-middleware_coverage.xml" centreon-middleware_coverage.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"
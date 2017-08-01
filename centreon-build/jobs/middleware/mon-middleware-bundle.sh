#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-imp-portal-api

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Pull base image.
docker pull ubuntu:16.04

# Fetch middleware sources.
get_internal_source "middleware/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf $PROJECT-$VERSION.tar.gz
mv $PROJECT-$VERSION $PROJECT

# CentOS middleware image.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers

# Build image.
REGISTRY="ci.int.centreon.com:5000"
MIDDLEWARE_IMAGE="$REGISTRY/mon-middleware-$VERSION-$RELEASE:latest"
MIDDLEWARE_WIP_IMAGE="$REGISTRY/mon-middleware-wip:latest"
docker build --no-cache -t "$MIDDLEWARE_IMAGE" -f middleware/latest.Dockerfile .
docker push "$MIDDLEWARE_IMAGE"
docker tag "$MIDDLEWARE_IMAGE" "$MIDDLEWARE_WIP_IMAGE"
docker push "$MIDDLEWARE_WIP_IMAGE"

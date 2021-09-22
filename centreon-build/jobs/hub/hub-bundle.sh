#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-hub-ui

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Prepare build directory.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
get_internal_source "hub/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION-release.tar.gz"
rm -rf "$PROJECT-$VERSION-release" "$PROJECT"
tar xzf "$PROJECT-$VERSION-release.tar.gz"
mv "$PROJECT-$VERSION-release" "$PROJECT"

# Pull base image.
docker pull httpd:latest

# Build image.
REGISTRY="registry.centreon.com"
HUB_IMAGE="$REGISTRY/hub-$VERSION-$RELEASE:latest"
HUB_WIP_IMAGE="$REGISTRY/hub-wip:latest"
docker build --no-cache -t "$HUB_IMAGE" -f hub/Dockerfile .
docker push "$HUB_IMAGE"
docker tag "$HUB_IMAGE" "$HUB_WIP_IMAGE"
docker push "$HUB_WIP_IMAGE"

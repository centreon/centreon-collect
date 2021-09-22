#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Set Docker images as latest.
REGISTRY='registry.centreon.com'
docker pull "$REGISTRY/hub-$VERSION-$RELEASE:latest"
docker tag "$REGISTRY/hub-$VERSION-$RELEASE:latest" "$REGISTRY/hub:latest"
docker push "$REGISTRY/hub:latest"

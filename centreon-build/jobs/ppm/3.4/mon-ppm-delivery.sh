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
distrib="centos7"
docker pull "$REGISTRY/mon-ppm-$VERSION-$RELEASE:$distrib"
docker tag "$REGISTRY/mon-ppm-$VERSION-$RELEASE:$distrib" "$REGISTRY/mon-ppm-3.4:$distrib"
docker push "$REGISTRY/mon-ppm-3.4:$distrib"

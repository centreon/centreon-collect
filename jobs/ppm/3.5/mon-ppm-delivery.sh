#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Set Docker images as latest.
REGISTRY='ci.int.centreon.com:5000'
for distrib in centos7 ; do
  docker pull "$REGISTRY/mon-ppm-$VERSION-$RELEASE:$distrib"
  docker tag "$REGISTRY/mon-ppm-$VERSION-$RELEASE:$distrib" "$REGISTRY/mon-ppm-3.5:$distrib"
  docker push "$REGISTRY/mon-ppm-3.5:$distrib"
done

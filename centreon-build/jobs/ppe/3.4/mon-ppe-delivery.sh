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
for distrib in centos7 ; do
  docker pull "$REGISTRY/mon-ppe-$VERSION-$RELEASE:$distrib"
  docker tag "$REGISTRY/mon-ppe-$VERSION-$RELEASE:$distrib" "$REGISTRY/mon-ppe-3.4:$distrib"
  docker push "$REGISTRY/mon-ppe-3.4:$distrib"
done

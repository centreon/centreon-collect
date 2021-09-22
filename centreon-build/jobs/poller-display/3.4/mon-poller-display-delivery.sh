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
for image in mon-poller-display mon-poller-display-central ; do
  for distrib in centos7 ; do
    docker pull "$REGISTRY/$image-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/$image-$VERSION-$RELEASE:$distrib" "$REGISTRY/$image-3.4:$distrib"
    docker push "$REGISTRY/$image-3.4:$distrib"
  done
done

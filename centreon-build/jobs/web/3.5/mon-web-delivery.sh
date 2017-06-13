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
for image in mon-web-fresh mon-web mon-web-widgets ; do
  for distrib in centos6 centos7 ; do
    docker pull "$REGISTRY/$image-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/$image-$VERSION-$RELEASE:$distrib" "$REGISTRY/$image-3.5:$distrib"
    docker push "$REGISTRY/$image-3.5:$distrib"
  done
done

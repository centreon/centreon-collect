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
  # -server- image.
  docker pull "$REGISTRY/des-mbi-server-$VERSION-$RELEASE:$distrib"
  docker tag "$REGISTRY/des-mbi-server-$VERSION-$RELEASE:$distrib" "$REGISTRY/des-mbi-server-18.9:$distrib"
  docker push "$REGISTRY/des-mbi-server-18.9:$distrib"

  # -web- image.
  docker pull "$REGISTRY/des-mbi-web-$VERSION-$RELEASE:$distrib"
  docker tag "$REGISTRY/des-mbi-web-$VERSION-$RELEASE:$distrib" "$REGISTRY/des-mbi-web-18.9:$distrib"
  docker push "$REGISTRY/des-mbi-web-18.9:$distrib"
done

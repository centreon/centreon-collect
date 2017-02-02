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
docker tag "$REGISTRY/mon-web-fresh-$VERSION-$RELEASE:centos6" "$REGISTRY/mon-web-fresh:centos6"
docker tag "$REGISTRY/mon-web-fresh-$VERSION-$RELEASE:centos7" "$REGISTRY/mon-web-fresh:centos7"
docker tag "$REGISTRY/mon-web-$VERSION-$RELEASE:centos6" "$REGISTRY/mon-web:centos6"
docker tag "$REGISTRY/mon-web-$VERSION-$RELEASE:centos7" "$REGISTRY/mon-web:centos7"
docker push "$REGISTRY/mon-web-fresh:centos6"
docker push "$REGISTRY/mon-web-fresh:centos7"
docker push "$REGISTRY/mon-web:centos6"
docker push "$REGISTRY/mon-web:centos7"

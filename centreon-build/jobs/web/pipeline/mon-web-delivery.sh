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
FRESH_CENTOS6="$REGISTRY/mon-web-fresh-$VERSION-$RELEASE:centos6"
FRESH_CENTOS7="$REGISTRY/mon-web-fresh-$VERSION-$RELEASE:centos7"
STANDARD_CENTOS6="$REGISTRY/mon-web-$VERSION-$RELEASE:centos6"
STANDARD_CENTOS7="$REGISTRY/mon-web-$VERSION-$RELEASE:centos7"

docker pull "$FRESH_CENTOS6"
docker tag "$FRESH_CENTOS6" "$REGISTRY/mon-web-fresh:centos6"
docker push "$REGISTRY/mon-web-fresh:centos6"

docker pull "$FRESH_CENTOS7"
docker tag "$FRESH_CENTOS7" "$REGISTRY/mon-web-fresh:centos7"
docker push "$REGISTRY/mon-web-fresh:centos7"

docker pull "$STANDARD_CENTOS6"
docker tag "$STANDARD_CENTOS6" "$REGISTRY/mon-web:centos6"
docker push "$REGISTRY/mon-web:centos6"

docker pull "$STANDARD_CENTOS7"
docker tag "$STANDARD_CENTOS7" "$REGISTRY/mon-web:centos7"
docker push "$REGISTRY/mon-web:centos7"

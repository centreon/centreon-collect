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
PPM_CENTOS6="$REGISTRY/mon-pppm-$VERSION-$RELEASE:centos6"
PPM_CENTOS7="$REGISTRY/mon-pppm-$VERSION-$RELEASE:centos7"

docker pull "$PPM_CENTOS6"
docker tag "$PPM_CENTOS6" "$REGISTRY/mon-ppm:centos6"
docker push "$REGISTRY/mon-ppm:centos6"

docker pull "$PPM_CENTOS7"
docker tag "$PPM_CENTOS7" "$REGISTRY/mon-ppm:centos7"
docker push "$REGISTRY/mon-ppm:centos7"

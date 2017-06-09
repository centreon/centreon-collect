#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Pull images.
WEB_IMAGE=ci.int.centreon.com:5000/mon-web-3.4:$DISTRIB
docker pull $WEB_IMAGE

# Prepare Dockerfile.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < ppm/3.4/ppm.Dockerfile.in > ppm/ppm.Dockerfile

# Build image.
REGISTRY="ci.int.centreon.com:5000"
PPM_IMAGE="$REGISTRY/mon-ppm-$VERSION-$RELEASE:$DISTRIB"
docker build --no-cache -t "$PPM_IMAGE" -f ppm/ppm.Dockerfile .
docker push "$PPM_IMAGE"

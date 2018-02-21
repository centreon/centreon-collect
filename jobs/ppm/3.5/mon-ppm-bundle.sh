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
WEB_IMAGE=ci.int.centreon.com:5000/mon-web-3.5:$DISTRIB
docker pull $WEB_IMAGE

# Prepare Dockerfile.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < ppm/3.5/ppm.Dockerfile.in > ppm/ppm.Dockerfile

# Build images.
REGISTRY="ci.int.centreon.com:5000"
PPM_IMAGE="$REGISTRY/mon-ppm-$VERSION-$RELEASE:$DISTRIB"
PPM_WIP_IMAGE="$REGISTRY/mon-ppm-3.5-wip:$DISTRIB"
docker build --no-cache -t "$PPM_IMAGE" -f ppm/ppm.Dockerfile .
docker push "$PPM_IMAGE"
docker tag "$PPM_IMAGE" "$PPM_WIP_IMAGE"
docker push "$PPM_WIP_IMAGE"

sed -e "s#@IMAGE@#$PPM_IMAGE#g" -e "s#@DISTRIB@#$DISTRIB#g" < ppm/3.5/ppm-autodisco.Dockerfile.in > ppm/ppm-autodisco.Dockerfile
PPM_AUTODISCO_IMAGE="$REGISTRY/mon-ppm-autodisco-$VERSION-$RELEASE:$DISTRIB"
PPM_AUTODISCO_WIP_IMAGE="$REGISTRY/mon-ppm-autodisco-3.5-wip:$DISTRIB"
docker build --no-cache -t "$PPM_AUTODISCO_IMAGE" -f ppm/ppm-autodisco.Dockerfile .
docker push "$PPM_AUTODISCO_IMAGE"
docker tag "$PPM_AUTODISCO_IMAGE" "$PPM_AUTODISCO_WIP_IMAGE"
docker push "$PPM_AUTODISCO_WIP_IMAGE"

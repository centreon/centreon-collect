#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|...>"
  exit 1
fi
DISTRIB="$1"

# Pull images.
REGISTRY="ci.int.centreon.com:5000"
PPM_IMAGE="$REGISTRY/mon-ppm-$VERSION-$RELEASE:$DISTRIB"
PPM_WIP_IMAGE="$REGISTRY/mon-ppm-18.10-wip:$DISTRIB"
PPM_AUTODISCO_IMAGE="$REGISTRY/mon-ppm-autodisco-$VERSION-$RELEASE:$DISTRIB"
PPM_AUTODISCO_WIP_IMAGE="$REGISTRY/mon-ppm-autodisco-18.10-wip:$DISTRIB"
WEB_IMAGE="$REGISTRY/mon-lm-18.10:$DISTRIB"
docker pull $WEB_IMAGE

# Prepare Dockerfiles.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../../containers centreon-build-containers
cd centreon-build-containers
sed -e "s#@BASE_IMAGE@#$WEB_IMAGE#g" -e "s#@DISTRIB@#$DISTRIB#g" < ppm/18.10/ppm.Dockerfile.in > ppm/ppm.Dockerfile
sed -e "s#@BASE_IMAGE@#$PPM_IMAGE#g" -e "s#@DISTRIB@#$DISTRIB#g" < ppm/18.10/ppm-autodisco.Dockerfile.in > ppm/ppm-autodisco.Dockerfile

# Build ppm image.
docker build --no-cache -t "$PPM_IMAGE" -f ppm/ppm.Dockerfile .
docker push "$PPM_IMAGE"
docker tag "$PPM_IMAGE" "$PPM_WIP_IMAGE"
docker push "$PPM_WIP_IMAGE"

# Build ppm-autodisco image.
docker build --no-cache -t "$PPM_AUTODISCO_IMAGE" -f ppm/ppm-autodisco.Dockerfile .
docker push "$PPM_AUTODISCO_IMAGE"
docker tag "$PPM_AUTODISCO_IMAGE" "$PPM_AUTODISCO_WIP_IMAGE"
docker push "$PPM_AUTODISCO_WIP_IMAGE"

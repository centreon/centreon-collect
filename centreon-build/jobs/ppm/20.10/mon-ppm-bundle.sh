#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-pp-manager

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|centos8|...>"
  exit 1
fi
DISTRIB="$1"

# Pull images.
REGISTRY="registry.centreon.com"
PPM_IMAGE="$REGISTRY/mon-ppm-$VERSION-$RELEASE:$DISTRIB"
PPM_WIP_IMAGE="$REGISTRY/mon-ppm-20.10-wip:$DISTRIB"
PPM_AUTODISCO_IMAGE="$REGISTRY/mon-ppm-autodisco-$VERSION-$RELEASE:$DISTRIB"
PPM_AUTODISCO_WIP_IMAGE="$REGISTRY/mon-ppm-autodisco-20.10-wip:$DISTRIB"
WEB_IMAGE="$REGISTRY/mon-web-20.10:$DISTRIB"
docker pull $WEB_IMAGE

# Prepare Dockerfiles.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../../containers centreon-build-containers
cd centreon-build-containers
sed -e "s#@BASE_IMAGE@#$WEB_IMAGE#g" -e "s#@DISTRIB@#$DISTRIB#g" < ppm/20.10/ppm.Dockerfile.in > ppm/ppm.Dockerfile
sed -e "s#@BASE_IMAGE@#$PPM_IMAGE#g" -e "s#@DISTRIB@#$DISTRIB#g" < ppm/20.10/ppm-autodisco.Dockerfile.in > ppm/ppm-autodisco.Dockerfile

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

REGISTRY="registry.centreon.com"
if [ "$DISTRIB" = "centos7" -o "$DISTRIB" = "centos8" ] ; then
  for image in mon-ppm mon-ppm-autodisco ; do
    docker pull "$REGISTRY/$image-$VERSION-$RELEASE:$DISTRIB"
    docker tag "$REGISTRY/$image-$VERSION-$RELEASE:$DISTRIB" "$REGISTRY/$image-20.10:$DISTRIB"
    docker push "$REGISTRY/$image-20.10:$DISTRIB"
  done
fi
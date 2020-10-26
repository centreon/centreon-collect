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
PPM_WIP_IMAGE="$REGISTRY/mon-ppm-21.04-wip:$DISTRIB"
PPM_AUTODISCO_IMAGE="$REGISTRY/mon-ppm-autodisco-$VERSION-$RELEASE:$DISTRIB"
PPM_AUTODISCO_WIP_IMAGE="$REGISTRY/mon-ppm-autodisco-21.04-wip:$DISTRIB"
WEB_IMAGE="$REGISTRY/mon-web-21.04:$DISTRIB"
docker pull $WEB_IMAGE

# Prepare Dockerfiles.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../../containers centreon-build-containers
cd centreon-build-containers
sed -e "s#@BASE_IMAGE@#$WEB_IMAGE#g" -e "s#@DISTRIB@#$DISTRIB#g" < ppm/21.04/ppm.Dockerfile.in > ppm/ppm.Dockerfile
sed -e "s#@BASE_IMAGE@#$PPM_IMAGE#g" -e "s#@DISTRIB@#$DISTRIB#g" < ppm/21.04/ppm-autodisco.Dockerfile.in > ppm/ppm-autodisco.Dockerfile
if [ "$DISTRIB" '=' centos7 ] ; then
  sed "s#@PROJECT@#$PROJECT#g;s#@SUBDIR@#21.04/el7/noarch/ppm/$PROJECT-$VERSION-$RELEASE#g" < repo/centreon-internal.repo.in > repo/centreon-internal.repo
else
  sed "s#@PROJECT@#$PROJECT#g;s#@SUBDIR@#21.04/el8/noarch/ppm/$PROJECT-$VERSION-$RELEASE#g" < repo/centreon-internal.repo.in > repo/centreon-internal.repo
fi

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

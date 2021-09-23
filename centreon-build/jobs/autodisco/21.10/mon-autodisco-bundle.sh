#!/bin/sh

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-autodiscovery

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

if [ $(curl https://registry.centreon.com/v2/mon-web-$BRANCH_NAME/tags/list -s | egrep  '^\{"name"' | wc -l) -eq 1 ]
then
    CENTREON_WEB_BRANCH_NAME=$BRANCH_NAME
else
    CENTREON_WEB_BRANCH_NAME=21.10
fi

# Pull base image.
WEB_IMAGE=registry.centreon.com/mon-web-$CENTREON_WEB_BRANCH_NAME:$DISTRIB
docker pull $WEB_IMAGE

# Prepare Dockerfiles.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../../containers centreon-build-containers
cd centreon-build-containers
sed "s#@CENTREON_WEB_IMAGE@#$WEB_IMAGE#g" < autodisco/21.10/Dockerfile.in > autodisco/Dockerfile

# Build image.
REGISTRY="registry.centreon.com"
AUTODISCO_IMAGE="$REGISTRY/mon-autodisco-$VERSION-$RELEASE:$DISTRIB"
AUTODISCO_WIP_IMAGE=$(echo "$REGISTRY/mon-autodisco-$BRANCH_NAME:$DISTRIB" | sed -e 's/\(.*\)/\L\1/')
docker build --no-cache -t "$AUTODISCO_IMAGE" -f autodisco/Dockerfile .
docker push "$AUTODISCO_IMAGE"
docker tag "$AUTODISCO_IMAGE" "$AUTODISCO_WIP_IMAGE"
docker push "$AUTODISCO_WIP_IMAGE"

REGISTRY="registry.centreon.com"
if [ "$BUILD" == "REFERENCE" ]
then
  if [ "$DISTRIB" = "centos7" -o "$DISTRIB" = "centos8" ] ; then
    docker pull "$REGISTRY/mon-autodisco-$VERSION-$RELEASE:$DISTRIB"
    docker tag "$REGISTRY/mon-autodisco-$VERSION-$RELEASE:$DISTRIB" "$REGISTRY/mon-autodisco-21.10:$DISTRIB"
    docker push "$REGISTRY/mon-autodisco-21.10:$DISTRIB"
  fi
fi

#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7|...>"
  exit 1
fi
DISTRIB="$1"
if [ "$DISTRIB" = "centos6" ] ; then
  CENTOS_VERSION=6
else
  CENTOS_VERSION=7
fi

# Pull base image.
REGISTRY="registry.centreon.com"
DEP_IMAGE="$REGISTRY/mon-dependencies-3.4:$DISTRIB"
BASE_IMAGE="$REGISTRY/mon-web-3.4:$DISTRIB"
SERVER_IMAGE="$REGISTRY/des-map-server-$VERSION-$RELEASE:$DISTRIB"
SERVER_WIP_IMAGE=$(echo "$REGISTRY/des-map-server-$BRANCH_NAME:$DISTRIB" | sed -e 's/\(.*\)/\L\1/')
WEB_IMAGE="$REGISTRY/des-map-web-$VERSION-$RELEASE:$DISTRIB"
WEB_WIP_IMAGE=$(echo "$REGISTRY/des-map-web-$BRANCH_NAME:$DISTRIB" | sed -e 's/\(.*\)/\L\1/')
docker pull $DEP_IMAGE
docker pull $BASE_IMAGE

# Prepare Dockerfiles.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../../containers centreon-build-containers
cd centreon-build-containers
sed "s#@BASE_IMAGE@#$DEP_IMAGE#g" < map/3.4/server.Dockerfile.in > map/server.$DISTRIB.Dockerfile
sed "s#@BASE_IMAGE@#$BASE_IMAGE#g" < map/3.4/web.Dockerfile.in > map/web.$DISTRIB.Dockerfile
sed "s#@PROJECT@#map-server#g;s#@SUBDIR@#3.4/el$CENTOS_VERSION/noarch/map/centreon-map-$VERSION-$RELEASE#g" < repo/centreon-internal.repo.in > repo/centreon-internal.repo

# Server image.
docker build --no-cache -t "$SERVER_IMAGE" -f map/server.$DISTRIB.Dockerfile .
docker push "$SERVER_IMAGE"
docker tag "$SERVER_IMAGE" "$SERVER_WIP_IMAGE"
docker push "$SERVER_WIP_IMAGE"

# Web image.
docker build --no-cache -t "$WEB_IMAGE" -f map/web.$DISTRIB.Dockerfile .
docker push "$WEB_IMAGE"
docker tag "$WEB_IMAGE" "$WEB_WIP_IMAGE"
docker push "$WEB_WIP_IMAGE"

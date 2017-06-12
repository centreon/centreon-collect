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
if [ "$DISTRIB" = "centos6" ] ; then
  CENTOS_VERSION=6
else
  CENTOS_VERSION=7
fi

# Pull base image.
REGISTRY="ci.int.centreon.com:5000"
DEP_IMAGE="$REGISTRY/mon-dependencies:$DISTRIB"
BASE_IMAGE="$REGISTRY/mon-web:$DISTRIB"
SERVER_IMAGE="$REGISTRY/des-map-server-$VERSION-$RELEASE:$DISTRIB"
WEB_IMAGE="$REGISTRY/des-map-web-$VERSION-$RELEASE:$DISTRIB"
docker pull $DEP_IMAGE
docker pull $BASE_IMAGE

# Prepare Dockerfiles.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < map/server.Dockerfile.in > map/server.$DISTRIB.Dockerfile
sed "s/@BASE_IMAGE@/$BASE_IMAGE/g" < map/web.Dockerfile.in > map/web.$DISTRIB.Dockerfile
sed "s/@VERSION@/3.5/g;s/@DISTRIB@/el$CENTOS_VERSION/g" < repo/centreon-internal.repo.in > repo/centreon-internal.repo

# Build image.
docker build --no-cache -t "$SERVER_IMAGE" -f map/server.$DISTRIB.Dockerfile .
docker build --no-cache -t "$WEB_IMAGE" -f map/web.$DISTRIB.Dockerfile .
docker push "$SERVER_IMAGE"
docker push "$WEB_IMAGE"

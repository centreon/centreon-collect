#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-map

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

# Pull base image.
REGISTRY="registry.centreon.com"
DEP_IMAGE="$REGISTRY/mon-dependencies-20.10:$DISTRIB"
BASE_IMAGE="$REGISTRY/mon-web-20.10:$DISTRIB"
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
sed "s#@BASE_IMAGE@#$DEP_IMAGE#g" < map/20.10/server.Dockerfile.in > map/server.$DISTRIB.Dockerfile
sed "s#@BASE_IMAGE@#$BASE_IMAGE#g" < map/20.10/web.Dockerfile.in > map/web.$DISTRIB.Dockerfile
if [ "$DISTRIB" = 'centos7' ] ; then
  sed "s#@PROJECT@#map-web#g;s#@SUBDIR@#20.10/el7/noarch/map-web/$PROJECT-web-$VERSIONWEB-$RELEASE#g" < repo/centreon-internal.repo.in > repo/centreon-internal.repo
  sed "s#@PROJECT@#map-server#g;s#@SUBDIR@#20.10/el7/noarch/map-server/$PROJECT-server-$VERSIONSERVER-$RELEASE#g" < repo/centreon-internal.repo.in >> repo/centreon-internal.repo
  sed "s#@PROJECT@#map-server-ng#g;s#@SUBDIR@#20.10/el7/noarch/map-server-ng/$PROJECT-server-$VERSIONSERVER-$RELEASE#g" < repo/centreon-internal.repo.in >> repo/centreon-internal.repo
else
  sed "s#@PROJECT@#map-web#g;s#@SUBDIR@#20.10/el8/noarch/map-web/$PROJECT-web-$VERSIONWEB-$RELEASE#g" < repo/centreon-internal.repo.in > repo/centreon-internal.repo
  sed "s#@PROJECT@#map-server#g;s#@SUBDIR@#20.10/el8/noarch/map-server/$PROJECT-server-$VERSIONSERVER-$RELEASE#g" < repo/centreon-internal.repo.in >> repo/centreon-internal.repo
  sed "s#@PROJECT@#map-server-ng#g;s#@SUBDIR@#20.10/el8/noarch/map-server-ng/$PROJECT-server-$VERSIONSERVER-$RELEASE#g" < repo/centreon-internal.repo.in >> repo/centreon-internal.repo
fi

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

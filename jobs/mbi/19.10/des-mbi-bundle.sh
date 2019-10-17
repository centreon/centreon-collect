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
CENTOS_VERSION=7

# Pull base image.
DEP_IMAGE=registry.centreon.com/mon-dependencies-19.10:$DISTRIB
WEB_IMAGE=registry.centreon.com/mon-web-19.10:$DISTRIB
docker pull $DEP_IMAGE
docker pull $WEB_IMAGE

# Prepare Dockerfiles.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../../containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < mbi/19.10/server.Dockerfile.in > mbi/server.Dockerfile
sed "s/@DISTRIB@/$DISTRIB/g" < mbi/19.10/web.Dockerfile.in > mbi/web.Dockerfile
sed "s#@PROJECT@#centreon-bi-server#g;s#@SUBDIR@#19.10/el7/noarch/mbi-web/centreon-bi-server-$VERSION-$RELEASE#g" < repo/centreon-internal.repo.in > repo/centreon-internal.repo

# Build image.
REGISTRY="registry.centreon.com"
MBI_SERVER_IMAGE="$REGISTRY/des-mbi-server-$VERSION-$RELEASE:$DISTRIB"
MBI_SERVER_WIP_IMAGE="$REGISTRY/des-mbi-server-19.10-wip:$DISTRIB"
MBI_WEB_IMAGE="$REGISTRY/des-mbi-web-$VERSION-$RELEASE:$DISTRIB"
MBI_WEB_WIP_IMAGE="$REGISTRY/des-mbi-web-19.10-wip:$DISTRIB"
docker build --no-cache -t "$MBI_SERVER_IMAGE" -f mbi/server.Dockerfile .
docker build --no-cache -t "$MBI_WEB_IMAGE" -f mbi/web.Dockerfile .
docker tag "$MBI_SERVER_IMAGE" "$MBI_SERVER_WIP_IMAGE"
docker tag "$MBI_WEB_IMAGE" "$MBI_WEB_WIP_IMAGE"
docker push "$MBI_SERVER_IMAGE"
docker push "$MBI_SERVER_WIP_IMAGE"
docker push "$MBI_WEB_IMAGE"
docker push "$MBI_WEB_WIP_IMAGE"

#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-awie

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

# Pull Centreon Web image.
WEB_IMAGE=registry.centreon.com/mon-web-21.04:$DISTRIB
docker pull $WEB_IMAGE

# Prepare Dockerfile.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../../containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < awie/21.04/awie.Dockerfile.in > awie/Dockerfile

# Build image.
REGISTRY="registry.centreon.com"
AWIE_IMAGE="$REGISTRY/mon-awie-$VERSION-$RELEASE:$DISTRIB"
AWIE_WIP_IMAGE="$REGISTRY/mon-awie-21.04-wip:$DISTRIB"
docker build --no-cache -t "$AWIE_IMAGE" -f awie/Dockerfile .
docker push "$AWIE_IMAGE"
docker tag "$AWIE_IMAGE" "$AWIE_WIP_IMAGE"
docker push "$AWIE_WIP_IMAGE"

REGISTRY="registry.centreon.com"
if [ "$DISTRIB" = "centos7" -o "$DISTRIB" = "centos8" ] ; then
  docker pull "$REGISTRY/mon-awie-$VERSION-$RELEASE:$DISTRIB"
  docker tag "$REGISTRY/mon-awie-$VERSION-$RELEASE:$DISTRIB" "$REGISTRY/mon-awie-21.04:$DISTRIB"
  docker push "$REGISTRY/mon-awie-21.04:$DISTRIB"
fi
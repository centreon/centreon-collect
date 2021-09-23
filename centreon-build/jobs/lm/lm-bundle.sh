#!/bin/sh

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-license-manager

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
MAJOR=`echo $VERSION | cut -d . -f 1,2`

# Pull images.
WEB_IMAGE=registry.centreon.com/mon-web-$MAJOR:$DISTRIB
docker pull $WEB_IMAGE

# Prepare Dockerfile.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < lm/$MAJOR/lm.Dockerfile.in > lm/lm.$DISTRIB.Dockerfile

# Build image.
REGISTRY="registry.centreon.com"
LM_IMAGE="$REGISTRY/mon-lm-$VERSION-$RELEASE:$DISTRIB"
LM_WIP_IMAGE="$REGISTRY/mon-lm-$MAJOR-wip:$DISTRIB"
docker build --no-cache -t "$LM_IMAGE" -f lm/lm.$DISTRIB.Dockerfile .
docker push "$LM_IMAGE"
docker tag "$LM_IMAGE" "$LM_WIP_IMAGE"
docker push "$LM_WIP_IMAGE"

REGISTRY="registry.centreon.com"

if [ "$BUILD" == "REFERENCE" ] ; then
  if [ "$DISTRIB" = "centos7" -o "$DISTRIB" = "centos8" ] ; then
    docker pull "$REGISTRY/mon-lm-$VERSION-$RELEASE:$DISTRIB"
    docker tag "$REGISTRY/mon-lm-$VERSION-$RELEASE:$DISTRIB" "$REGISTRY/mon-lm-$MAJOR:$DISTRIB"
    docker push "$REGISTRY/mon-lm-$MAJOR:$DISTRIB"
  fi
fi